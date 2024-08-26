cmake_minimum_required(VERSION 3.29)
project(bemu)


add_executable(bemugb
        src/gb/cartridge.cpp
        src/gb/bus.cpp
        src/gb/cpu.cpp
        src/gb/cpu_cb.cpp
        src/gb/emulator.cpp
        src/gb/io.cpp
        src/gb/lcd.cpp
        src/gb/main.cpp
        src/gb/ppu.cpp
        src/gb/timer.cpp
)

target_link_libraries(bemugb PRIVATE spdlog::spdlog magic_enum::magic_enum)
target_include_directories(bemugb PRIVATE include)


add_subdirectory(third_party)