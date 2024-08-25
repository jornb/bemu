#include <ncurses.h>

#include <../../../include/bemu/save/file.hpp>
#include <bemu/gb/cartridge.hpp>
#include <bemu/gb/clock.hpp>
#include <bemu/gb/emulator.hpp>
#include <bemu/io/curses.hpp>
#include <bemu/io/keyboard.hpp>
#include <bemu/io/x11.hpp>
#include <bemu/save/rewind.hpp>
#include <clocale>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace bemu;
using namespace bemu::gb;

namespace {
std::unordered_map<Key, Joypad::Button> key_to_button = {
    {Key::Up, Joypad::BUTTON_UP},       {Key::W, Joypad::BUTTON_UP},        {Key::Down, Joypad::BUTTON_DOWN},
    {Key::S, Joypad::BUTTON_DOWN},      {Key::Left, Joypad::BUTTON_LEFT},   {Key::A, Joypad::BUTTON_LEFT},
    {Key::Right, Joypad::BUTTON_RIGHT}, {Key::D, Joypad::BUTTON_RIGHT},

    {Key::X, Joypad::BUTTON_START},     {Key::Space, Joypad::BUTTON_START}, {Key::Return, Joypad::BUTTON_START},
    {Key::Z, Joypad::BUTTON_SELECT},

    {Key::Q, Joypad::BUTTON_A},         {Key::E, Joypad::BUTTON_B},

    {Key::N, Joypad::BUTTON_A},         {Key::M, Joypad::BUTTON_B},

    {Key::Z, Joypad::BUTTON_A},         {Key::X, Joypad::BUTTON_B}};
}

struct App : IKeyReceiver {
    explicit App(Emulator &emulator) : m_emulator(emulator), m_keys(*this) {
        setlocale(LC_ALL, "");
        initscr();              // start curses mode
        noecho();               // don't echo keypresses
        cbreak();               // pass keys immediately (no Enter needed)
        curs_set(0);            // hide cursor
        nodelay(stdscr, TRUE);  // non-blocking getch
        keypad(stdscr, TRUE);   // enable special keys (arrows, etc.)

        if (has_colors()) {
            start_color();

            // init_color(1, 1000, 1000, 1000);  // white
            // init_color(2, 666, 666, 666);     // light gray
            // init_color(3, 333, 333, 333);     // dark gray
            // init_color(4, 0, 0, 0);           // black

            auto init_rgb = [](const int index, const int r, const int g, const int b) {
                init_color(index, r * 1000 / 255, g * 1000 / 255, b * 1000 / 255);
            };

            // Colors from https://www.color-hex.com/color-palette/45299
            init_rgb(1, 155, 188, 15);  // white
            init_rgb(2, 139, 172, 15);  // light gray
            init_rgb(3, 48, 98, 48);    // dark gray
            init_rgb(4, 15, 56, 15);    // black

            for (int top = 0; top <= 3; ++top) {
                for (int bottom = 0; bottom <= 3; ++bottom) {
                    const int pair_index = top * 4 + bottom + 1;
                    init_pair(pair_index, 1 + top, 1 + bottom);
                }
            }

            init_rgb(5, 0, 0, 0);        // black
            init_rgb(6, 255, 255, 255);  // white
            init_pair(25, 6, 5);         // inverted white on black
        }

        clear();
    }

    ~App() {
        endwin();  // restore terminal
    }

    void on_key_pressed(const Key key) {
        if (const auto it = key_to_button.find(key); it != key_to_button.end()) {
            m_emulator.m_external->m_pending_buttons[it->second] = true;
        } else if (key == Key::Number_1) {
            m_clock.m_speedup_factor = 1.0;
        } else if (key == Key::Number_2) {
            m_clock.m_speedup_factor = 2.0;
        } else if (key == Key::Number_3) {
            m_clock.m_speedup_factor = 3.0;
        } else if (key == Key::Number_4) {
            m_clock.m_speedup_factor = 4.0;
        } else if (key == Key::Number_5) {
            m_clock.m_speedup_factor = 5.0;
        } else if (key == Key::Number_6) {
            m_clock.m_speedup_factor = 6.0;
        } else if (key == Key::Number_7) {
            m_clock.m_speedup_factor = 7.0;
        } else if (key == Key::Number_8) {
            m_clock.m_speedup_factor = 8.0;
        } else if (key == Key::Number_9) {
            m_clock.m_speedup_factor = 9.0;
        } else if (key == Key::Number_0) {
            m_clock.m_speedup_factor = 1e10;
        } else if (key == Key::Plus) {
            save_state_to_file(m_emulator, "test.sav");

        } else if (key == Key::Backslash) {
            load_state_from_file(m_emulator, "test.sav");
        }
    }

    void on_key_released(const Key key) {
        if (const auto it = key_to_button.find(key); it != key_to_button.end()) {
            m_emulator.m_external->m_pending_buttons[it->second] = false;
        }
    }

    void draw() {
        const auto &screen = m_emulator.m_external->m_screen;
        std::optional<int> current_color_pair;

        for (int y = 0; y < screen.get_height() - 1; y += 2) {
            for (int x = 0; x < screen.get_width(); ++x) {
                const auto top = screen.get_pixel(x, y);
                const auto bottom = screen.get_pixel(x, y + 1);

                // Skip if unchanged from previous frame
                if (m_previous_screen && m_previous_screen->get_pixel(x, y) == top &&
                    m_previous_screen->get_pixel(x, y + 1) == bottom) {
                    continue;
                }

                // Draw the top and bottom pixels
                const int pair_index = top * 4 + bottom + 1;
                const wchar_t str[2] = {L'▀', L'\0'};
                if (current_color_pair != pair_index) {
                    current_color_pair = pair_index;
                    attron(COLOR_PAIR(pair_index));
                }
                move(y / 2, x);
                addwstr(str);
            }
        }

        // Draw status bar
        attron(COLOR_PAIR(25));
        const std::string status = std::format(
            "Keys: {}{}    Rewind: {:>3} MiB, {:>5} states, {:>5} seconds", m_keys.is_key_pressed(Key::W) ? 'W' : ' ',
            m_keys.is_key_pressed(Key::S) ? 'S' : ' ', m_rewind.get_used_bytes() / 1024 / 1024,
            m_rewind.get_num_states(), (m_emulator.m_external->m_ticks - m_rewind.get_first_ticks()) / 4194304);
        mvprintw(screen.get_height() / 2, 0, "%s", status.c_str());

        refresh();  // render to terminal

        m_previous_screen = screen;
    }

    bool update() {
        m_keys.update();

        if (m_keys.is_key_pressed(Key::Backspace) && m_rewind.pop_state()) {
            draw();
            m_clock.sleep_frame(2.0);
            return true;
        }

        if (!m_emulator.run_to_next_frame()) return false;
        m_rewind.push_state();

        draw();
        m_clock.sleep_frame();

        return true;
    }

   private:
    std::optional<Screen> m_previous_screen;
    Emulator &m_emulator;
    Rewind<Emulator> m_rewind{m_emulator};

    Clock m_clock;
    X11Keys m_keys;
};

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./bemugb <rom>" << std::endl;
        return -1;
    }

    try {
        auto cartridge = Cartridge::from_file(argv[1]);
        Emulator emulator{std::move(cartridge)};
        App app{emulator};
        while (app.update());
    } catch (const std::exception &ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown error" << std::endl;
        return -1;
    }

    return 0;
}
