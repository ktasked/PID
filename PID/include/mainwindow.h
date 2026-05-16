#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScopedPointer>
#include <memory>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QChartView;
class QChart;
class QLineSeries;
class QValueAxis;
class QVBoxLayout;
class QWidget;

// Forward declaration для бизнес-логики
class PIDTuner;
struct PIDCoefficients;
struct SystemCharacteristics;

/**
 * @brief Главное окно приложения PID Tuner
 * 
 * Отвечает за:
 * - Отображение интерфейса ввода параметров
 * - Обработку пользовательского ввода
 * - Отображение результатов расчёта
 * - Построение графиков переходных процессов
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    /**
     * @brief Обработчик кнопки "Рассчитать"
     */
    void onCalculateClicked();

    /**
     * @brief Обработчик кнопки "Сбросить"
     */
    void onResetClicked();

    /**
     * @brief Обработчик ошибки валидации от PIDTuner
     */
    void onValidationError(const QString& message);

private:
    /**
     * @brief Создание виджетов интерфейса
     */
    void createWidgets();

    /**
     * @brief Создание компоновки интерфейса
     */
    void createLayout();

    /**
     * @brief Настройка соединений сигналов и слотов
     */
    void setupConnections();

    /**
     * @brief Обновление таблицы с результатами расчёта
     */
    void updateResultsTable(const PIDCoefficients& coeffs, 
                           const SystemCharacteristics& chars);

    /**
     * @brief Построение графика переходных процессов
     */
    void updateCharts(const QVector<QPair<double, double>>& openLoop,
                     const QVector<QPair<double, double>>& closedLoop);

    /**
     * @brief Валидация входных данных перед расчётом
     * @return true если все данные корректны
     */
    bool validateInput() const;

    // Виджеты ввода параметров
    QDoubleSpinBox* m_spinBoxK = nullptr;      // Коэффициент усиления
    QDoubleSpinBox* m_spinBoxT = nullptr;      // Постоянная времени
    QDoubleSpinBox* m_spinBoxTau = nullptr;    // Транспортное задержание
    
    // Виджеты отображения результатов
    QTableWidget* m_resultsTable = nullptr;
    
    // График
    QChartView* m_chartView = nullptr;
    QChart* m_chart = nullptr;
    QLineSeries* m_openLoopSeries = nullptr;
    QLineSeries* m_closedLoopSeries = nullptr;
    
    // Кнопки
    QPushButton* m_calculateButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    
    // Метка статуса
    QLabel* m_statusLabel = nullptr;
    
    // Бизнес-логика
    std::unique_ptr<PIDTuner> m_pidTuner;
};

#endif // MAINWINDOW_H
