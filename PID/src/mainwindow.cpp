#include "mainwindow.h"
#include "pidtuner.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QChartView>
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QFormLayout>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_pidTuner(std::make_unique<PIDTuner>(this))
{
    setWindowTitle("PID Tuner - Автоматическая настройка ПИД-регулятора");
    setMinimumSize(900, 700);

    createWidgets();
    createLayout();
    setupConnections();

    statusBar()->showMessage("Готов к работе. Введите параметры объекта и нажмите \"Рассчитать\"");
}

MainWindow::~MainWindow() = default;

void MainWindow::createWidgets()
{
    // ===== Виджеты ввода параметров =====
    
    // Коэффициент усиления K
    auto createDoubleSpinBox = [](double min, double max, double step, double value, int decimals) {
        auto spinBox = new QDoubleSpinBox();
        spinBox->setRange(min, max);
        spinBox->setSingleStep(step);
        spinBox->setValue(value);
        spinBox->setDecimals(decimals);
        spinBox->setMaximumWidth(150);
        return spinBox;
    };

    m_spinBoxK = createDoubleSpinBox(0.01, 1000.0, 0.1, 1.0, 2);
    m_spinBoxK->setToolTip("Коэффициент усиления объекта (K > 0)");

    m_spinBoxT = createDoubleSpinBox(0.01, 1000.0, 0.1, 1.0, 2);
    m_spinBoxT->setToolTip("Постоянная времени объекта (T > 0), сек");

    m_spinBoxTau = createDoubleSpinBox(0.0, 100.0, 0.01, 0.1, 2);
    m_spinBoxTau->setToolTip("Транспортное задержание (τ ≥ 0), сек");

    // ===== Кнопки =====
    m_calculateButton = new QPushButton("🔢 Рассчитать");
    m_calculateButton->setMinimumHeight(40);
    m_calculateButton->setFont(QFont("Arial", 11, QFont::Bold));
    m_calculateButton->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; padding: 8px; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );

    m_resetButton = new QPushButton("🔄 Сбросить");
    m_resetButton->setMinimumHeight(40);
    m_resetButton->setFont(QFont("Arial", 10));
    m_resetButton->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; border-radius: 5px; padding: 8px; }"
        "QPushButton:hover { background-color: #da190b; }"
    );

    // ===== Таблица результатов =====
    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(2);
    m_resultsTable->setRowCount(8);
    m_resultsTable->setHorizontalHeaderLabels({"Параметр", "Значение"});
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Заполнение названий строк
    const QStringList rowLabels = {
        "Kp (пропорциональный)",
        "Ki (интегральный)",
        "Kd (дифференциальный)",
        "Время регулирования",
        "Перерегулирование",
        "Статическая ошибка",
        "Устойчивость",
        "Метод расчёта"
    };

    for (int i = 0; i < rowLabels.size(); ++i) {
        m_resultsTable->setItem(i, 0, new QTableWidgetItem(rowLabels[i]));
        m_resultsTable->item(i, 0)->setFlags(m_resultsTable->item(i, 0)->flags() & ~Qt::ItemIsEditable);
    }

    // ===== График =====
    m_chart = new QChart();
    m_chart->setTitle("Переходные процессы");
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_openLoopSeries = new QLineSeries();
    m_openLoopSeries->setName("Разомкнутый объект");
    m_openLoopSeries->setColor(Qt::blue);
    m_openLoopSeries->setPen(QPen(Qt::blue, 2));

    m_closedLoopSeries = new QLineSeries();
    m_closedLoopSeries->setName("Замкнутая система с ПИД");
    m_closedLoopSeries->setColor(Qt::red);
    m_closedLoopSeries->setPen(QPen(Qt::red, 2));

    m_chart->addSeries(m_openLoopSeries);
    m_chart->addSeries(m_closedLoopSeries);

    // Оси координат
    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText("Время, сек");
    axisX->setRange(0, 10);
    axisX->setTickCount(11);

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText("Выход");
    axisY->setRange(0, 1.5);
    axisY->setTickCount(7);

    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);

    m_openLoopSeries->attachAxis(axisX);
    m_openLoopSeries->attachAxis(axisY);
    m_closedLoopSeries->attachAxis(axisX);
    m_closedLoopSeries->attachAxis(axisY);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(300);

    // ===== Метка статуса =====
    m_statusLabel = new QLabel();
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #f0f0f0; border-radius: 3px; }");
}

void MainWindow::createLayout()
{
    // Центральный виджет
    auto centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    auto mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // ===== Группа ввода параметров =====
    auto inputGroup = new QGroupBox("📊 Параметры объекта управления");
    auto inputLayout = new QFormLayout(inputGroup);
    inputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    inputLayout->setSpacing(10);

    auto labelK = new QLabel("Коэффициент усиления (K):");
    labelK->setToolTip("Отношение установившегося выхода к ступенчатому входу");
    inputLayout->addRow(labelK, m_spinBoxK);

    auto labelT = new QLabel("Постоянная времени (T), сек:");
    labelT->setToolTip("Время, за которое выход достигает 63.2% от установившегося значения");
    inputLayout->addRow(labelT, m_spinBoxT);

    auto labelTau = new QLabel("Транспортное задержание (τ), сек:");
    labelTau->setToolTip("Время чистого запаздывания реакции объекта");
    inputLayout->addRow(labelTau, m_spinBoxTau);

    // Кнопки в отдельной строке
    auto buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_calculateButton, 1);
    buttonLayout->addWidget(m_resetButton, 0);
    inputLayout->addRow(buttonLayout);

    mainLayout->addWidget(inputGroup);

    // ===== Группа результатов =====
    auto resultsGroup = new QGroupBox("📋 Результаты расчёта");
    auto resultsLayout = new QVBoxLayout(resultsGroup);
    resultsLayout->addWidget(m_resultsTable);
    mainLayout->addWidget(resultsGroup);

    // ===== Группа графика =====
    auto chartGroup = new QGroupBox("📈 Графики переходных процессов");
    auto chartLayout = new QVBoxLayout(chartGroup);
    chartLayout->addWidget(m_chartView);
    mainLayout->addWidget(chartGroup);

    // ===== Статус =====
    mainLayout->addWidget(m_statusLabel);
}

void MainWindow::setupConnections()
{
    connect(m_calculateButton, &QPushButton::clicked, 
            this, &MainWindow::onCalculateClicked);
    
    connect(m_resetButton, &QPushButton::clicked,
            this, &MainWindow::onResetClicked);
    
    connect(m_pidTuner.get(), &PIDTuner::validationError,
            this, &MainWindow::onValidationError);
}

bool MainWindow::validateInput() const
{
    // Проверка на корректность ввода (дополнительная к проверке в PIDTuner)
    if (m_spinBoxK->value() <= 0.0) {
        QMessageBox::warning(this, "Ошибка ввода", 
            "Коэффициент усиления K должен быть больше 0");
        return false;
    }

    if (m_spinBoxT->value() <= 0.0) {
        QMessageBox::warning(this, "Ошибка ввода",
            "Постоянная времени T должна быть больше 0");
        return false;
    }

    if (m_spinBoxTau->value() < 0.0) {
        QMessageBox::warning(this, "Ошибка ввода",
            "Транспортное задержание τ не может быть отрицательным");
        return false;
    }

    return true;
}

void MainWindow::onCalculateClicked()
{
    if (!validateInput()) {
        return;
    }

    // Установка параметров в PIDTuner
    bool success = m_pidTuner->setObjectParameters(
        m_spinBoxK->value(),
        m_spinBoxT->value(),
        m_spinBoxTau->value()
    );

    if (!success) {
        // Ошибка уже обработана через сигнал validationError
        return;
    }

    // Расчёт коэффициентов ПИД
    PIDCoefficients coeffs = m_pidTuner->calculateZieglerNichols();

    if (!coeffs.isValid()) {
        QMessageBox::critical(this, "Ошибка расчёта",
            "Не удалось рассчитать коэффициенты ПИД-регулятора");
        return;
    }

    // Оценка характеристик системы
    SystemCharacteristics chars = m_pidTuner->estimateSystemCharacteristics(coeffs);

    // Обновление таблицы результатов
    updateResultsTable(coeffs, chars);

    // Моделирование и построение графиков
    auto openLoop = m_pidTuner->simulateOpenLoopResponse(10.0, 0.01);
    auto closedLoop = m_pidTuner->simulateClosedLoopResponse(coeffs, 10.0, 0.01);
    updateCharts(openLoop, closedLoop);

    // Обновление статуса
    m_statusLabel->setText(
        QString("✅ Расчёт выполнен успешно. Метод: Циглера-Николса. "
                "Kp=%1, Ki=%2, Kd=%3")
        .arg(coeffs.Kp, 0, 'f', 4)
        .arg(coeffs.Ki, 0, 'f', 4)
        .arg(coeffs.Kd, 0, 'f', 4)
    );
    m_statusLabel->setStyleSheet(
        "QLabel { padding: 5px; background-color: #d4edda; border-radius: 3px; color: #155724; }"
    );

    statusBar()->showMessage("Расчёт завершён", 5000);
}

void MainWindow::onResetClicked()
{
    // Сброс параметров ввода к значениям по умолчанию
    m_spinBoxK->setValue(1.0);
    m_spinBoxT->setValue(1.0);
    m_spinBoxTau->setValue(0.1);

    // Очистка таблицы результатов
    m_resultsTable->clearContents();
    for (int i = 0; i < m_resultsTable->rowCount(); ++i) {
        m_resultsTable->setItem(i, 1, new QTableWidgetItem("-"));
    }

    // Очистка графиков
    m_openLoopSeries->clear();
    m_closedLoopSeries->clear();

    // Сброс PIDTuner
    m_pidTuner->reset();

    // Обновление статуса
    m_statusLabel->setText("Ожидание ввода параметров...");
    m_statusLabel->setStyleSheet(
        "QLabel { padding: 5px; background-color: #f0f0f0; border-radius: 3px; }"
    );

    statusBar()->showMessage("Параметры сброшены", 3000);
}

void MainWindow::onValidationError(const QString& message)
{
    QMessageBox::warning(this, "Ошибка валидации", message);
    m_statusLabel->setText(QString("❌ Ошибка: %1").arg(message));
    m_statusLabel->setStyleSheet(
        "QLabel { padding: 5px; background-color: #f8d7da; border-radius: 3px; color: #721c24; }"
    );
}

void MainWindow::updateResultsTable(const PIDCoefficients& coeffs, 
                                    const SystemCharacteristics& chars)
{
    // Форматирование значений для отображения
    auto formatValue = [](double value, const QString& unit = "") {
        return QString("%1 %2").arg(value, 0, 'f', 4).arg(unit);
    };

    m_resultsTable->setItem(0, 1, new QTableWidgetItem(formatValue(coeffs.Kp)));
    m_resultsTable->setItem(1, 1, new QTableWidgetItem(formatValue(coeffs.Ki)));
    m_resultsTable->setItem(2, 1, new QTableWidgetItem(formatValue(coeffs.Kd)));
    m_resultsTable->setItem(3, 1, new QTableWidgetItem(formatValue(chars.settlingTime, "сек")));
    m_resultsTable->setItem(4, 1, new QTableWidgetItem(QString("%1 %").arg(chars.overshoot, 0, 'f', 2)));
    m_resultsTable->setItem(5, 1, new QTableWidgetItem(formatValue(chars.steadyStateError)));
    
    auto stabilityItem = new QTableWidgetItem(chars.stabilityMessage());
    stabilityItem->setForeground(chars.isStable ? QBrush(Qt::darkGreen) : QBrush(Qt::red));
    stabilityItem->setFont(QFont("Arial", 10, QFont::Bold));
    m_resultsTable->setItem(6, 1, stabilityItem);
    
    m_resultsTable->setItem(7, 1, new QTableWidgetItem("Циглера-Николса"));

    // Автоподбор ширины столбцов
    m_resultsTable->resizeColumnsToContents();
}

void MainWindow::updateCharts(const QVector<QPair<double, double>>& openLoop,
                             const QVector<QPair<double, double>>& closedLoop)
{
    // Очистка старых данных
    m_openLoopSeries->clear();
    m_closedLoopSeries->clear();

    // Добавление данных разомкнутого отклика
    for (const auto& point : openLoop) {
        m_openLoopSeries->append(point.first, point.second);
    }

    // Добавление данных замкнутого отклика
    for (const auto& point : closedLoop) {
        m_closedLoopSeries->append(point.first, point.second);
    }

    // Автонастройка осей
    if (!openLoop.isEmpty() || !closedLoop.isEmpty()) {
        double maxX = 10.0;
        double maxY = 1.5;

        for (const auto& point : openLoop) {
            maxX = std::max(maxX, point.first);
            maxY = std::max(maxY, point.second);
        }
        for (const auto& point : closedLoop) {
            maxX = std::max(maxX, point.first);
            maxY = std::max(maxY, point.second);
        }

        // Округление для красивых осей
        maxX = std::ceil(maxX * 1.1);
        maxY = std::ceil(maxY * 1.2);

        auto axes = m_chart->axes();
        if (axes.size() >= 2) {
            auto axisX = qobject_cast<QValueAxis*>(axes[0]);
            auto axisY = qobject_cast<QValueAxis*>(axes[1]);
            
            if (axisX && axisY) {
                axisX->setRange(0, maxX);
                axisY->setRange(0, maxY);
            }
        }
    }
}
