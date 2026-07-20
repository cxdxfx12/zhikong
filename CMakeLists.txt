cmake_minimum_required(VERSION 3.20)
project(ExpressDailyManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Charts)

# QXlsx - bundled source
add_subdirectory(thirdparty/QXlsx/QXlsx)
target_compile_definitions(QXlsx PRIVATE HAS_QXLSX)

set(SOURCES
    src/main.cpp
    src/db/Database.cpp
    src/db/EntityDao.cpp
    src/db/ColumnDao.cpp
    src/db/DailyDao.cpp
    src/model/Entity.cpp
    src/model/ColumnDef.cpp
    src/model/DailyValue.cpp
    src/model/DisplayDataModel.cpp
    src/model/CategoryHeaderView.cpp
    src/ui/MainWindow.cpp
    src/ui/DataEntryDialog.cpp
    src/ui/EntityManageDialog.cpp
    src/ui/ColumnManageDialog.cpp
    src/ui/ImportDialog.cpp
    src/ui/ChartDialog.cpp
    src/ui/CompareDialog.cpp
    src/service/AggregationService.cpp
    src/service/ImportService.cpp
    src/service/ExportService.cpp
    src/service/ChartService.cpp
    src/utils/FormatUtils.cpp
)

set(HEADERS
    src/db/Database.h
    src/db/EntityDao.h
    src/db/ColumnDao.h
    src/db/DailyDao.h
    src/model/Entity.h
    src/model/ColumnDef.h
    src/model/DailyValue.h
    src/model/DisplayDataModel.h
    src/model/CategoryHeaderView.h
    src/ui/MainWindow.h
    src/ui/DataEntryDialog.h
    src/ui/EntityManageDialog.h
    src/ui/ColumnManageDialog.h
    src/ui/ImportDialog.h
    src/ui/ChartDialog.h
    src/ui/CompareDialog.h
    src/service/AggregationService.h
    src/service/ImportService.h
    src/service/ExportService.h
    src/service/ChartService.h
    src/utils/FormatUtils.h
)

set(RESOURCES
    resources/app.qrc
)

if(WIN32)
    set(ICON_RESOURCE resources/app.rc)
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES} ${ICON_RESOURCE})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Charts
    QXlsx
)

target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_QXLSX)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/thirdparty/QXlsx/QXlsx/header
)

if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
