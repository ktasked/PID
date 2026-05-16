/**
 * @file main.cpp
 * @brief Точка входа в приложение PID Tuner
 * 
 * Приложение для автоматического подбора параметров ПИД-регулятора
 * методом Циглера-Николса по реакции на ступенчатое воздействие.
 */

#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <QLocale>
#include <QLoggingCategory>

#include "mainwindow.h"

// Глобальная категория логирования для приложения
Q_LOGGING_CATEGORY(appLog, "app")

int main(int argc, char *argv[])
{
    // Создание приложения Qt
    QApplication app(argc, argv);

    // Установка информации о приложении
    QCoreApplication::setOrganizationName("PIDTuner");
    QCoreApplication::setApplicationName("PID Tuner");
    QCoreApplication::setApplicationVersion("1.0.0");

    // Настройка локали для корректного отображения чисел
    QLocale::setDefault(QLocale::Russian);

    // Применение современного стиля Fusion
    app.setStyle(QStyleFactory::create("Fusion"));

    // Настройка шрифта приложения
    QFont defaultFont("Segoe UI", 10);
    #ifdef Q_OS_MACOS
        defaultFont = QFont("SF Pro Text", 13);
    #elif defined(Q_OS_LINUX)
        defaultFont = QFont("Ubuntu", 10);
    #endif
    app.setFont(defaultFont);

    // Настройка палитры (опционально, для тёмной темы можно изменить)
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(245, 245, 245));
    palette.setColor(QPalette::WindowText, QColor(0, 0, 0));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 225));
    palette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    palette.setColor(QPalette::Text, QColor(0, 0, 0));
    palette.setColor(QPalette::Button, QColor(240, 240, 240));
    palette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor(76, 175, 80));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    // Включение логирования (можно отключить в релизной версии)
    #ifdef QT_DEBUG
        QLoggingCategory::setFilterRules("app.*=true\n"
                                         "qt.qpa.*=false");
    #else
        QLoggingCategory::setFilterRules("app.pidtuner=false");
    #endif

    qCInfo(appLog) << "Запуск приложения PID Tuner";
    qCInfo(appLog) << "Qt version:" << qVersion();

    try {
        // Создание и отображение главного окна
        MainWindow window;
        window.show();

        // Запуск цикла событий
        int result = app.exec();

        qCInfo(appLog) << "Завершение приложения, код выхода:" << result;
        return result;

    } catch (const std::exception& e) {
        qCCritical(appLog) << "Необработанное исключение:" << e.what();
        QMessageBox::critical(nullptr, "Критическая ошибка",
            QString("Произошла непредвиденная ошибка: %1").arg(e.what()));
        return -1;
    } catch (...) {
        qCCritical(appLog) << "Неизвестное исключение";
        QMessageBox::critical(nullptr, "Критическая ошибка",
            "Произошла непредвиденная ошибка");
        return -1;
    }
}
