cmake_minimum_required(VERSION 3.18)
project(CClock LANGUAGES C)

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
set(SOURCES src/main.c)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(GTK_LAYER_SHELL REQUIRED IMPORTED_TARGET gtk4-layer-shell-0)

add_executable(cclock ${SOURCES})

target_include_directories(cclock PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(cclock PRIVATE PkgConfig::GTK4 PkgConfig::GTK_LAYER_SHELL)
