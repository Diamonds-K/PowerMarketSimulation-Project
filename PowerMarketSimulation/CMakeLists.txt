cmake_minimum_required(VERSION 3.16)

project(PowerMarketSimulation VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Sql Charts)

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
target_link_libraries(PowerMarketSimulation PRIVATE pms_core Qt6::Widgets Qt6::Charts)

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

get_filename_component(PMS_COMPILER_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(PMS_QT_RUNTIME_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
set_tests_properties(ClearingTest SettlementTest OptimizationTest PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${PMS_COMPILER_RUNTIME_DIR};PATH=path_list_prepend:${PMS_QT_RUNTIME_DIR}")

# 可选目标：把 Qt 运行库与插件复制到可执行文件旁边，使程序可脱离开发环境直接双击运行。
if(WIN32 AND TARGET Qt6::qmake)
    get_target_property(PMS_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(PMS_QT_BIN_DIR "${PMS_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(PMS_WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${PMS_QT_BIN_DIR}")
    if(PMS_WINDEPLOYQT_EXECUTABLE)
        add_custom_target(deploy
            COMMAND "${PMS_WINDEPLOYQT_EXECUTABLE}"
                    --no-translations --no-system-d3d-compiler --no-opengl-sw
                    "$<TARGET_FILE:PowerMarketSimulation>"
            DEPENDS PowerMarketSimulation
            COMMENT "Deploying Qt runtime next to PowerMarketSimulation")
    endif()
endif()
