cmake_minimum_required(VERSION 3.29)
project(bemu)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bemugb_lib STATIC
        src/io/curses.cpp
        src/io/x11.cpp
        src/gb/external.cpp
        src/gb/cartridge.cpp
        src/gb/bus.cpp
        src/gb/cpu.cpp
        src/gb/emulator.cpp
        src/gb/joypad.cpp
        src/gb/lcd.cpp
        src/gb/memory.cpp
        src/gb/ppu.cpp
        src/gb/timer.cpp
)
target_link_libraries(bemugb_lib PUBLIC spdlog::spdlog magic_enum::magic_enum)
target_include_directories(bemugb_lib PUBLIC include)

add_executable(bemugb src/gb/app/gui.cpp)
target_include_directories(bemugb PRIVATE third_party/olcPixelGameEngine)
target_link_libraries(bemugb PRIVATE bemugb_lib)
if (WIN32)
    target_link_libraries(bemugb PRIVATE -luser32 -lgdi32 -lopengl32 -lgdiplus -lShlwapi -ldwmapi)
else ()
    target_link_libraries(bemugb PRIVATE -lX11 -lGL -lpthread -lpng)
endif ()

if (UNIX)
    find_package(Curses REQUIRED)
    find_package(X11 REQUIRED)

    add_executable(bemugb_console src/gb/app/console.cpp)
    target_include_directories(bemugb_console PRIVATE ${CURSES_INCLUDE_DIR})
    target_link_libraries(bemugb_console PRIVATE bemugb_lib ncursesw ${X11_LIBRARIES})
endif ()

add_executable(test_bemugb test/gb/instruction_timings.cpp)
target_link_libraries(test_bemugb PRIVATE bemugb_lib)

add_executable(test_bemugb_mooneye test/gb/mooneye.cpp)
target_link_libraries(test_bemugb_mooneye PRIVATE bemugb_lib)

add_subdirectory(third_party)
