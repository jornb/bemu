#define OLC_PGE_APPLICATION
#include <olcPixelGameEngine.h>
#include <spdlog/spdlog.h>

#include <../../../include/bemu/save/rewind.hpp>
#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/clock.hpp>
#include <bemu/gb/emulator.hpp>
#include <bemu/gb/screen.hpp>
#include <magic_enum/magic_enum.hpp>
#include <mutex>
#include <thread>

using namespace bemu;
using namespace bemu::gb;

namespace {
// std::array g_colors = {olc::Pixel{255, 255, 255}, olc::Pixel{170, 170, 170}, olc::Pixel{85, 85, 85},
//                        olc::Pixel{0, 0, 0}};

std::array g_colors = {olc::Pixel{155, 188, 15}, olc::Pixel{139, 172, 15}, olc::Pixel{48, 98, 48},
                       olc::Pixel{15, 56, 15}};
}  // namespace

struct Gui : olc::PixelGameEngine {
    explicit Gui(Emulator &emulator) : m_emulator(emulator) { sAppName = "Gui"; }

    bool OnUserCreate() override { return true; }

    bool OnUserUpdate(float) override {
        if (GetKey(olc::Key::BACK).bHeld && m_rewind.pop_state()) {
            draw();
            m_clock.sleep_frame(2.0);
            return true;
        }

        if (!m_emulator.run_to_next_frame()) return false;
        m_rewind.push_state();

        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_A] = GetKey(olc::Key::Q).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_B] = GetKey(olc::Key::E).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_UP] = GetKey(olc::Key::W).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_DOWN] = GetKey(olc::Key::S).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_LEFT] = GetKey(olc::Key::A).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_RIGHT] = GetKey(olc::Key::D).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_START] = GetKey(olc::Key::X).bHeld;
        m_emulator.m_external->m_pending_buttons[Joypad::BUTTON_SELECT] = GetKey(olc::Key::Z).bHeld;

        draw();
        m_clock.sleep_frame();

        return true;
    }

    void draw() {
        const auto &s = m_emulator.m_external->m_screen;
        for (int x = 0; x < std::min(ScreenWidth(), static_cast<int>(s.get_width())); x++) {
            for (int y = 0; y < std::min(ScreenHeight(), static_cast<int>(s.get_height())); y++) {
                Draw(x, y, g_colors[s.get_pixel(x, y)]);
            }
        }
    }

    Emulator &m_emulator;
    Rewind<Emulator> m_rewind{m_emulator};
    Clock m_clock;
};

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        spdlog::critical("Usage: ./bemugb <rom>");
        return -1;
    }
    try {
        spdlog::info("Loading ROM {}", argv[1]);
        auto cartridge = Cartridge::from_file(argv[1]);
        const auto &header = cartridge->header();

        spdlog::info("\tTitle          : {}", header.get_title());
        spdlog::info("\tCartridge type : {}", magic_enum::enum_name(header.cartridge_type));
        spdlog::info("\tRAM size       : {}", magic_enum::enum_name(header.ram_size));
        spdlog::info("\tROM size       : {}", magic_enum::enum_name(header.rom_size));
        spdlog::info("\tEntry          : {:02x} {:02x} {:02x} {:02x}", header.entry[0], header.entry[1],
                     header.entry[2], header.entry[3]);

        Emulator emulator{std::move(cartridge)};
        Gui gui{emulator};
        if (gui.Construct(emulator.get_screen().get_width(), emulator.get_screen().get_height(), 4, 4)) {
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
