cmake_minimum_required(VERSION 3.20)
project(magpie-overlay VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL DBus)
find_package(PkgConfig REQUIRED)

pkg_check_modules(X11 REQUIRED x11 xcomposite xext xtst gl)

add_executable(magpie-overlay
    src/main.cpp
    src/capture.h
    src/capture.cpp
    src/input.h
    src/input.cpp
    src/shader_pipeline.h
    src/shader_pipeline.cpp
    src/overlay_window.h
    src/overlay_window.cpp
    src/main_window.h
    src/main_window.cpp
)

target_include_directories(magpie-overlay PRIVATE
    src
    ${X11_INCLUDE_DIRS}
)

target_link_libraries(magpie-overlay PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::DBus
    ${X11_LIBRARIES}
)

set_target_properties(magpie-overlay PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
