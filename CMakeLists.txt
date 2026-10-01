cmake_minimum_required(VERSION 3.21)

project(OpenHub
    VERSION 0.2.2.1
    DESCRIPTION "Linux control center for Logitech gaming peripherals"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(openhub
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.hpp
    src/device/DeviceInfo.hpp
    src/device/DeviceKnowledge.cpp
    src/device/DeviceKnowledge.hpp
    src/device/DeviceScanner.cpp
    src/device/DeviceScanner.hpp
    src/hidpp/HidppProbe.cpp
    src/hidpp/HidppProbe.hpp
)

target_link_libraries(openhub PRIVATE Qt6::Widgets)

target_compile_definitions(openhub PRIVATE
    OPENHUB_VERSION="${PROJECT_VERSION}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(openhub PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS openhub
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
