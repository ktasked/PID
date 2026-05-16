#ifndef PIDTUNER_H
#define PIDTUNER_H

#include <QObject>
#include <QVector>
#include <QPair>
#include <optional>

/**
 * @brief Структура для хранения результатов расчёта ПИД-коэффициентов
 */
struct PIDCoefficients {
    double Kp = 0.0;  // Пропорциональная составляющая
    double Ki = 0.0;  // Интегральная составляющая
    double Kd = 0.0;  // Дифференциальная составляющая
    
    bool isValid() const { return Kp >= 0 && Ki >= 0 && Kd >= 0; }
};

/**
 * @brief Структура для хранения характеристик замкнутой системы
 */
struct SystemCharacteristics {
    double settlingTime = 0.0;      // Время регулирования (сек)
    double overshoot = 0.0;         // Перерегулирование (%)
    double steadyStateError = 0.0;  // Статическая ошибка
    bool isStable = true;           // Устойчивость системы
    
    QString stabilityMessage() const {
        return isStable ? "Система устойчива" : "Возможна неустойчивость";
    }
};

/**
 * @brief Класс для расчёта параметров ПИД-регулятора методом Циглера-Николса
 * 
 * Метод Циглера-Николса по реакции на ступенчатое воздействие (Process Reaction Curve):
 * - Основан на аппроксимации объекта моделью первого порядка с запаздыванием:
 *   W(s) = K * exp(-τ*s) / (T*s + 1)
 * - где K - коэффициент усиления, T - постоянная времени, τ - транспортное задержание
 * 
 * Формулы для ПИД-регулятора (классический метод Циглера-Николса):
 *   Kp = 1.2 * T / (K * τ)
 *   Ki = 0.6 / (K * τ)  => Ti = 2 * τ
 *   Kd = 0.6 * T / K    => Td = 0.5 * τ
 * 
 * Ограничения метода:
 * - Применим только для объектов с S-образной переходной характеристикой
 * - Требует знания параметров K, T, τ
 * - Может давать чрезмерно агрессивную настройку (большое перерегулирование ~40%)
 * - Не подходит для объектов с большим запаздыванием (τ/T > 0.5)
 */
class PIDTuner : public QObject {
    Q_OBJECT

public:
    explicit PIDTuner(QObject *parent = nullptr);
    ~PIDTuner() override = default;

    /**
     * @brief Установка параметров объекта управления
     * @param K Коэффициент усиления (должен быть > 0)
     * @param T Постоянная времени (должна быть > 0)
     * @param tau Транспортное задержание (должно быть >= 0)
     * @return true если параметры корректны
     */
    bool setObjectParameters(double K, double T, double tau);

    /**
     * @brief Расчёт коэффициентов ПИД-регулятора методом Циглера-Николса
     * @return Структура с коэффициентами Kp, Ki, Kd
     */
    PIDCoefficients calculateZieglerNichols() const;

    /**
     * @brief Расчёт коэффициентов ПИД-регулятора с уменьшенным перерегулированием
     *        (модифицированный метод Циглера-Николса)
     * @param dampingFactor Коэффициент демпфирования (0.5-1.0, где 1.0 - стандартный Ц-Н)
     * @return Структура с коэффициентами Kp, Ki, Kd
     */
    PIDCoefficients calculateDampedZieglerNichols(double dampingFactor = 0.7) const;

    /**
     * @brief Оценка характеристик замкнутой системы с рассчитанным ПИД-регулятором
     * @param coeffs Коэффициенты ПИД-регулятора
     * @return Характеристики системы
     */
    SystemCharacteristics estimateSystemCharacteristics(const PIDCoefficients& coeffs) const;

    /**
     * @brief Моделирование переходного процесса разомкнутого объекта
     * @param duration Длительность моделирования (сек)
     * @param dt Шаг дискретизации (сек)
     * @return Вектор пар (время, значение отклика)
     */
    QVector<QPair<double, double>> simulateOpenLoopResponse(double duration = 10.0, double dt = 0.01) const;

    /**
     * @brief Моделирование переходного процесса замкнутой системы с ПИД-регулятором
     * @param coeffs Коэффициенты ПИД-регулятора
     * @param duration Длительность моделирования (сек)
     * @param dt Шаг дискретизации (сек)
     * @return Вектор пар (время, значение выхода)
     */
    QVector<QPair<double, double>> simulateClosedLoopResponse(
        const PIDCoefficients& coeffs, 
        double duration = 10.0, 
        double dt = 0.01) const;

    /**
     * @brief Проверка корректности параметров объекта
     */
    bool hasValidParameters() const { return m_hasValidParams; }

    /**
     * @brief Получение текущего коэффициента усиления
     */
    double gain() const { return m_K; }

    /**
     * @brief Получение текущей постоянной времени
     */
    double timeConstant() const { return m_T; }

    /**
     * @brief Получение текущего транспортного задержания
     */
    double delay() const { return m_tau; }

    /**
     * @brief Сброс параметров
     */
    void reset();

signals:
    /**
     * @brief Сигнал об ошибке валидации параметров
     */
    void validationError(const QString& message);

private:
    double m_K = 0.0;       // Коэффициент усиления
    double m_T = 0.0;       // Постоянная времени
    double m_tau = 0.0;     // Транспортное задержание
    bool m_hasValidParams = false;

    /**
     * @brief Валидация параметров объекта
     */
    std::optional<QString> validateParameters(double K, double T, double tau) const;

    /**
     * @brief Расчёт передаточной функции объекта в точке s
     *        W(s) = K * exp(-τ*s) / (T*s + 1)
     */
    std::pair<double, double> transferFunction(double omega) const;
};

#endif // PIDTUNER_H
