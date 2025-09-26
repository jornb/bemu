cmake_minimum_required(VERSION 3.29)
project(bemu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#find_package(Qt6 REQUIRED COMPONENTS Core Qml Gui Quick)
#
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTORCC ON)

#find_package(OpenGL REQUIRED)

add_executable(bemugb
        src/gb/gui.cpp
        src/gb/cartridge.cpp
        src/gb/bus.cpp
        src/gb/cpu.cpp
        src/gb/cpu_cb.cpp
        src/gb/emulator.cpp
        src/gb/joypad.cpp
        src/gb/lcd.cpp
#        src/gb/main.cpp
        src/gb/ppu.cpp
        src/gb/timer.cpp

        src/gb/gui/emulator.hpp
        src/gb/gui/screen.hpp
)

target_include_directories(bemugb PRIVATE
        include
        third_party/olcPixelGameEngine
)
target_link_libraries(bemugb PRIVATE spdlog::spdlog magic_enum::magic_enum  -luser32 -lgdi32 -lopengl32 -lgdiplus -lShlwapi -ldwmapi)


add_subdirectory(third_party)
