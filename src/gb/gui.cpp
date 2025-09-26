#define OLC_PGE_APPLICATION
#include <olcPixelGameEngine.h>
#include <spdlog/spdlog.h>

#include <bemu/gb/emulator.hpp>
#include <cmath>
#include <mutex>
#include <thread>

using namespace bemu;
using namespace bemu::gb;

struct Gui : olc::PixelGameEngine {
    explicit Gui(Emulator &emulator) : m_emulator(emulator) {
        sAppName = "Gui";

        m_emulator.m_frame_callback = [this](const auto &s) {
            std::scoped_lock lock(m_mutex);
            m_screen = s;
        };
    }

    bool OnUserCreate() override { return true; }

    bool OnUserUpdate(float elapsed_time) override {
        std::unique_lock lock(m_mutex);
        const auto s = m_screen;
        lock.unlock();

        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_A, GetKey(olc::Key::Q).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_A, GetKey(olc::Key::E).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_UP, GetKey(olc::Key::W).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_DOWN, GetKey(olc::Key::S).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_LEFT, GetKey(olc::Key::A).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_RIGHT, GetKey(olc::Key::D).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_START, GetKey(olc::Key::X).bHeld);
        m_emulator.m_bus.m_joypad.set_button(Joypad::BUTTON_SELECT, GetKey(olc::Key::Z).bHeld);

        for (int x = 0; x < std::min(ScreenWidth(), Screen::screen_width); x++) {
            for (int y = 0; y < std::min(ScreenHeight(), Screen::screen_height); y++) {
                static std::array colors = {olc::Pixel{255, 255, 255}, olc::Pixel{170, 170, 170},
                                            olc::Pixel{85, 85, 85}, olc::Pixel{0, 0, 0}};
                Draw(x, y, colors[s.get_pixel(x, y)]);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        return true;
    }

    std::mutex m_mutex;
    Emulator &m_emulator;
    Screen m_screen;

    // float m_target_frame_time = 1.0 / 30.0f;
};

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        spdlog::critical("Usage: ./bemugb <rom>");
        return -1;
    }
    try {
        spdlog::info("Loading ROM {}", argv[1]);
        auto cartridge = Cartridge::from_file(argv[1]);
        const auto &header = cartridge.header();

        spdlog::info("\tTitle          : {}", header.get_title());
        spdlog::info("\tCartridge type : {}", magic_enum::enum_name(header.cartridge_type));
        spdlog::info("\tRAM size       : {}", magic_enum::enum_name(header.ram_size));
        spdlog::info("\tROM size       : {}", magic_enum::enum_name(header.rom_size));
        spdlog::info("\tEntry          : {:02x} {:02x} {:02x} {:02x}", header.entry[0], header.entry[1],
                     header.entry[2], header.entry[3]);

        Emulator emulator{cartridge};

        std::jthread t{[&] {
            try {
                emulator.run();
            } catch (const std::exception &ex) {
                spdlog::critical(ex.what());
            }
        }};

        Gui gui{emulator};
        if (gui.Construct(Screen::screen_width, Screen::screen_height, 4, 4)) {
            gui.Start();
        }
    } catch (const std::exception &e) {
        spdlog::critical(e.what());
        return -1;
    } catch (...) {
        spdlog::critical("Unknown error");
        return -1;
    }

    return 0;
}
