# PID Tuner - Автоматический подбор параметров ПИД-регулятора
# Требуется Qt 6 (Widgets + Charts) или Qt 5.15+

cmake_minimum_required(VERSION 3.16)
project(PIDTuner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Поиск Qt
find_package(Qt6 COMPONENTS Widgets Charts QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Charts)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

message(STATUS "Using Qt version ${Qt${QT_VERSION_MAJOR}_VERSION}")

# Исходные файлы
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/pidtuner.cpp
)

set(HEADERS
    include/mainwindow.h
    include/pidtuner.h
)

# Создание исполняемого файла
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

# Подключение заголовочных файлов
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Линковка с Qt
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets
        Qt6::Charts
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Widgets
        Qt5::Charts
    )
endif()

# Установка
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
