cmake_minimum_required(VERSION 3.18)
project(cclock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Widgets)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(cclock ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(cclock
    URI CClock
    VERSION 1.0
    QML_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Main.qml
)

target_link_libraries(cclock PRIVATE Qt6::Quick Qt6::Widgets LayerShellQt::Interface)

install(TARGETS cclock RUNTIME DESTINATION bin)
