cmake_minimum_required(VERSION 3.18)
project(cclock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Widgets Network DBus)
find_package(LayerShellQt REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(cclock ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(cclock
    URI CClock
    VERSION 1.0
    QML_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Main.qml
)

target_link_libraries(cclock PRIVATE Qt6::Quick Qt6::Widgets Qt6::Network Qt6::DBus LayerShellQt::Interface)

add_executable(cclock-geom-test ${CMAKE_CURRENT_SOURCE_DIR}/src/geom_test.cpp)
enable_testing()
add_test(NAME geom COMMAND cclock-geom-test)

install(TARGETS cclock RUNTIME DESTINATION bin)
install(FILES cclock.1 DESTINATION share/man/man1)
install(FILES cclock.svg DESTINATION share/icons/hicolor/scalable/apps)
