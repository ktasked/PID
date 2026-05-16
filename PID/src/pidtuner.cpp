#include "pidtuner.h"
#include <QtMath>
#include <QLoggingCategory>

// Логирование для отладки
Q_LOGGING_CATEGORY(pidTunerLog, "app.pidtuner")

PIDTuner::PIDTuner(QObject *parent)
    : QObject(parent)
{
}

bool PIDTuner::setObjectParameters(double K, double T, double tau)
{
    auto error = validateParameters(K, T, tau);
    if (error.has_value()) {
        qCWarning(pidTunerLog) << "Validation failed:" << error.value();
        emit validationError(error.value());
        m_hasValidParams = false;
        return false;
    }

    m_K = K;
    m_T = T;
    m_tau = tau;
    m_hasValidParams = true;

    qCDebug(pidTunerLog) << "Parameters set: K=" << K << ", T=" << T << ", tau=" << tau;
    return true;
}

std::optional<QString> PIDTuner::validateParameters(double K, double T, double tau) const
{
    // Проверка на конечность значений
    if (!std::isfinite(K)) {
        return QString("Коэффициент усиления K должен быть конечным числом");
    }
    if (!std::isfinite(T)) {
        return QString("Постоянная времени T должна быть конечным числом");
    }
    if (!std::isfinite(tau)) {
        return QString("Транспортное задержание τ должно быть конечным числом");
    }

    // Проверка положительности K и T
    if (K <= 0.0) {
        return QString("Коэффициент усиления K должен быть больше 0 (текущее значение: %1)").arg(K);
    }
    if (T <= 0.0) {
        return QString("Постоянная времени T должна быть больше 0 (текущее значение: %1)").arg(T);
    }

    // Проверка неотрицательности tau
    if (tau < 0.0) {
        return QString("Транспортное задержание τ не может быть отрицательным (текущее значение: %1)").arg(tau);
    }

    // Предупреждение о большом запаздывании (не блокирующее)
    if (tau > 0.0 && (tau / T) > 0.5) {
        qCWarning(pidTunerLog) << "Большое отношение τ/T =" << (tau / T) 
                               << ". Метод Циглера-Николса может быть неточным.";
    }

    return std::nullopt;  // Ошибок нет
}

PIDCoefficients PIDTuner::calculateZieglerNichols() const
{
    PIDCoefficients coeffs;

    if (!m_hasValidParams) {
        qCWarning(pidTunerLog) << "Cannot calculate: invalid parameters";
        return coeffs;
    }

    // Классические формулы Циглера-Николса для ПИД-регулятора
    // по реакции на ступенчатое воздействие:
    //
    // Kp = 1.2 * T / (K * τ)
    // Ki = 0.6 / (K * τ)      => Ti = Kp/Ki = 2τ
    // Kd = 0.6 * T / K        => Td = Kd/Kp = 0.5τ
    //
    // Если τ = 0, используем предельный случай (только ПИ-регулятор)
    
    if (m_tau > 1e-9) {
        coeffs.Kp = 1.2 * m_T / (m_K * m_tau);
        coeffs.Ki = 0.6 / (m_K * m_tau);
        coeffs.Kd = 0.6 * m_T / m_K;
    } else {
        // Предельный случай при τ → 0
        // Используем очень малое значение для численной стабильности
        const double epsilon = 1e-6;
        coeffs.Kp = 1.2 * m_T / (m_K * epsilon);
        coeffs.Ki = 0.6 / (m_K * epsilon);
        coeffs.Kd = 0.6 * m_T / m_K;
        
        qCWarning(pidTunerLog) << "Запаздывание τ близко к 0, возможны очень большие коэффициенты";
    }

    qCDebug(pidTunerLog) << "Ziegler-Nichols calculated: Kp=" << coeffs.Kp 
                         << ", Ki=" << coeffs.Ki << ", Kd=" << coeffs.Kd;

    return coeffs;
}

PIDCoefficients PIDTuner::calculateDampedZieglerNichols(double dampingFactor) const
{
    PIDCoefficients coeffs;

    if (!m_hasValidParams) {
        qCWarning(pidTunerLog) << "Cannot calculate: invalid parameters";
        return coeffs;
    }

    // Ограничение коэффициента демпфирования
    dampingFactor = std::clamp(dampingFactor, 0.5, 1.0);

    // Модифицированные формулы с коэффициентом демпфирования
    // Уменьшаем Kp и Kd для снижения перерегулирования
    //
    // Kp = (1.2 * T / (K * τ)) * dampingFactor
    // Ki = (0.6 / (K * τ)) * dampingFactor
    // Kd = (0.6 * T / K) * dampingFactor

    if (m_tau > 1e-9) {
        coeffs.Kp = (1.2 * m_T / (m_K * m_tau)) * dampingFactor;
        coeffs.Ki = (0.6 / (m_K * m_tau)) * dampingFactor;
        coeffs.Kd = (0.6 * m_T / m_K) * dampingFactor;
    } else {
        const double epsilon = 1e-6;
        coeffs.Kp = (1.2 * m_T / (m_K * epsilon)) * dampingFactor;
        coeffs.Ki = (0.6 / (m_K * epsilon)) * dampingFactor;
        coeffs.Kd = (0.6 * m_T / m_K) * dampingFactor;
    }

    qCDebug(pidTunerLog) << "Damped Ziegler-Nichols (factor=" << dampingFactor 
                         << "): Kp=" << coeffs.Kp << ", Ki=" << coeffs.Ki 
                         << ", Kd=" << coeffs.Kd;

    return coeffs;
}

SystemCharacteristics PIDTuner::estimateSystemCharacteristics(const PIDCoefficients& coeffs) const
{
    SystemCharacteristics chars;

    if (!m_hasValidParams || !coeffs.isValid()) {
        chars.isStable = false;
        return chars;
    }

    // Оценка характеристик на основе эмпирических зависимостей
    // для системы с ПИД-регулятором, настроенным методом Ц-Н
    
    // Время регулирования приблизительно равно 3-4 постоянным времени объекта
    // с учётом влияния регулятора
    chars.settlingTime = 3.0 * m_T + 2.0 * m_tau;

    // Перерегулирование для классического Ц-Н ~40%
    // Для модифицированного - меньше
    double nominalOvershoot = 40.0;  // %
    if (coeffs.Kp < 1.2 * m_T / (m_K * std::max(m_tau, 1e-6))) {
        // Если Kp уменьшен, перерегулирование меньше
        double ratio = coeffs.Kp / (1.2 * m_T / (m_K * std::max(m_tau, 1e-6)));
        nominalOvershoot *= ratio;
    }
    chars.overshoot = nominalOvershoot;

    // Статическая ошибка для ПИД-регулятора理论上 равна 0
    // (благодаря интегральной составляющей)
    chars.steadyStateError = 0.0;

    // Оценка устойчивости по соотношению параметров
    // Система считается устойчивой, если τ/T < 1.0 и коэффициенты положительные
    chars.isStable = (m_tau / m_T < 1.0) && coeffs.Kp > 0 && coeffs.Ki > 0;

    // Дополнительная проверка: слишком большие коэффициенты могут вызвать неустойчивость
    double maxGain = 10.0 * m_T / (m_K * std::max(m_tau, 1e-6));
    if (coeffs.Kp > maxGain) {
        chars.isStable = false;
    }

    qCDebug(pidTunerLog) << "System characteristics estimated: settlingTime=" << chars.settlingTime
                         << ", overshoot=" << chars.overshoot << "%, stable=" << chars.isStable;

    return chars;
}

QVector<QPair<double, double>> PIDTuner::simulateOpenLoopResponse(double duration, double dt) const
{
    QVector<QPair<double, double>> response;

    if (!m_hasValidParams) {
        return response;
    }

    // Моделирование отклика на единичное ступенчатое воздействие
    // для объекта: W(s) = K * exp(-τ*s) / (T*s + 1)
    //
    // Временная характеристика:
    // y(t) = 0, при t < τ
    // y(t) = K * (1 - exp(-(t-τ)/T)), при t >= τ

    const int numPoints = static_cast<int>(duration / dt) + 1;
    response.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i) {
        double t = i * dt;
        double y = 0.0;

        if (t >= m_tau) {
            y = m_K * (1.0 - std::exp(-(t - m_tau) / m_T));
        }

        response.append(qMakePair(t, y));
    }

    return response;
}

QVector<QPair<double, double>> PIDTuner::simulateClosedLoopResponse(
    const PIDCoefficients& coeffs, 
    double duration, 
    double dt) const
{
    QVector<QPair<double, double>> response;

    if (!m_hasValidParams || !coeffs.isValid()) {
        return response;
    }

    // Численное моделирование замкнутой системы с ПИД-регулятором
    // Используем метод Эйлера для дискретизации
    //
    // ПИД-регулятор в непрерывной форме:
    // u(t) = Kp * e(t) + Ki * ∫e(t)dt + Kd * de(t)/dt
    //
    // Объект: T * dy/dt + y = K * u(t - τ)
    //
    // Дискретизация с шагом dt:
    // - Интеграл: sum_e += e[i] * dt
    // - Производная: de_dt = (e[i] - e[i-1]) / dt
    // - Управление: u = Kp*e + Ki*sum_e + Kd*de_dt
    // - Объект: y[i+1] = y[i] + dt/T * (K*u_delayed - y[i])

    const int numPoints = static_cast<int>(duration / dt) + 1;
    response.reserve(numPoints);

    // Буфер для учёта запаздывания
    const int delaySteps = static_cast<int>(m_tau / dt);
    QVector<double> uBuffer(delaySteps + 1, 0.0);
    int uBufferIndex = 0;

    double y = 0.0;           // Выход объекта
    double sum_e = 0.0;       // Накопленная ошибка (интеграл)
    double e_prev = 0.0;      // Предыдущая ошибка
    double u_delayed = 0.0;   // Управление с задержкой

    const double setpoint = 1.0;  // Единичное ступенчатое воздействие

    for (int i = 0; i < numPoints; ++i) {
        double t = i * dt;

        // Ошибка регулирования
        double e = setpoint - y;

        // Интегральная составляющая (с анти-windup)
        sum_e += e * dt;
        const double maxIntegral = 10.0;  // Ограничение интеграла
        sum_e = std::clamp(sum_e, -maxIntegral, maxIntegral);

        // Производная составляющая
        double de_dt = (i > 0) ? (e - e_prev) / dt : 0.0;

        // ПИД-управление
        double u = coeffs.Kp * e + coeffs.Ki * sum_e + coeffs.Kd * de_dt;

        // Ограничение управления (опционально)
        const double maxControl = 100.0;
        u = std::clamp(u, -maxControl, maxControl);

        // Запись в буфер задержки
        uBuffer[uBufferIndex] = u;
        uBufferIndex = (uBufferIndex + 1) % uBuffer.size();

        // Получение управления с задержкой
        u_delayed = uBuffer[uBufferIndex];

        // Дискретизация объекта (метод Эйлера)
        // T * dy/dt + y = K * u_delayed
        double dy_dt = (m_K * u_delayed - y) / m_T;
        y += dy_dt * dt;

        response.append(qMakePair(t, y));

        e_prev = e;
    }

    qCDebug(pidTunerLog) << "Closed-loop simulation completed: " << response.size() << " points";

    return response;
}

void PIDTuner::reset()
{
    m_K = 0.0;
    m_T = 0.0;
    m_tau = 0.0;
    m_hasValidParams = false;
    qCDebug(pidTunerLog) << "PIDTuner reset";
}

std::pair<double, double> PIDTuner::transferFunction(double omega) const
{
    // Расчёт частотной характеристики W(jω)
    // W(s) = K * exp(-τ*s) / (T*s + 1), где s = jω
    //
    // W(jω) = K * exp(-jωτ) / (jωT + 1)
    //       = K * (cos(ωτ) - j*sin(ωτ)) / (1 + jωT)
    //
    // После умножения на сопряжённое:
    // Re = K * (cos(ωτ) - ωT*sin(ωτ)) / (1 + ω²T²)
    // Im = K * (-sin(ωτ) - ωT*cos(ωτ)) / (1 + ω²T²)

    const double denom = 1.0 + std::pow(omega * m_T, 2);
    const double cos_wtau = std::cos(omega * m_tau);
    const double sin_wtau = std::sin(omega * m_tau);

    double re = m_K * (cos_wtau - omega * m_T * sin_wtau) / denom;
    double im = m_K * (-sin_wtau - omega * m_T * cos_wtau) / denom;

    return {re, im};
}
