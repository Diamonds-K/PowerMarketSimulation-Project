cmake_minimum_required(VERSION 3.16)

project(PowerMarketSimulation VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql)

set(PMS_CORE_SOURCES
    Model/BidSegment/BidSegment.cpp
    Model/BidSheet/BidSheet.cpp
    Model/Generator/Generator.cpp
    Model/Consumer/Consumer.cpp
    Model/MarketInput/MarketInput.cpp
    Model/MarketResult/MarketResult.cpp
    Model/SettlementResult/SettlementResult.cpp
    Validation/BidValidator/BidValidator.cpp
    Validation/ConstraintChecker/ConstraintChecker.cpp
    Clearing/ClearingEngine/ClearingEngine.cpp
    Clearing/PiecewiseClearing/PiecewiseClearing.cpp
    Clearing/QuadraticClearing/QuadraticClearing.cpp
    Settlement/SettlementEngine/SettlementEngine.cpp
    Data/CSVReader/CSVReader.cpp
    Data/CSVWriter/CSVWriter.cpp
    Data/SQLiteManager/SQLiteManager.cpp
    TradingCenter/TradingCenter.cpp
)

add_library(pms_core STATIC ${PMS_CORE_SOURCES})
target_include_directories(pms_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pms_core PUBLIC Qt6::Sql)

add_executable(PowerMarketSimulation
    main.cpp
    UI/MainWindow/MainWindow.cpp
    UI/GeneratorWidget/GeneratorWidget.cpp
    UI/ConsumerWidget/ConsumerWidget.cpp
    UI/TradingCenterWidget/TradingCenterWidget.cpp
)
target_link_libraries(PowerMarketSimulation PRIVATE pms_core Qt6::Widgets)

enable_testing()

add_executable(ClearingTest Test/ClearingTest/ClearingTest.cpp)
target_link_libraries(ClearingTest PRIVATE pms_core)
add_test(NAME ClearingTest COMMAND ClearingTest)

add_executable(SettlementTest Test/SettlementTest/SettlementTest.cpp)
target_link_libraries(SettlementTest PRIVATE pms_core)
add_test(NAME SettlementTest COMMAND SettlementTest)

add_executable(OptimizationTest Test/OptimizationTest/OptimizationTest.cpp)
target_link_libraries(OptimizationTest PRIVATE pms_core)
add_test(NAME OptimizationTest COMMAND OptimizationTest)
