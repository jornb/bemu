#pragma once
#include <QDebug>
#include <QImage>
#include <QObject>
#include <bemu/gb/emulator.hpp>
#include <mutex>

class GuiEmulator : public QObject {
    Q_OBJECT
    Q_PROPERTY(QImage image READ image NOTIFY imageChanged)

   public:
    explicit GuiEmulator(bemu::gb::Emulator &emulator, QObject *parent = nullptr)
        : QObject(parent), m_emulator(emulator) {
        emulator.m_callback_screen_rendered = [this] { on_screen_rendered(); };
    }

    void on_screen_rendered() {
        std::scoped_lock lock{m_mutex};

        m_image = QImage(bemu::gb::Screen::screen_width, bemu::gb::Screen::screen_height, QImage::Format_RGB888);

        static std::array colors = {qRgb(255, 255, 255), qRgb(170, 170, 170), qRgb(85, 85, 85), qRgb(0, 0, 0)};

        for (int y = 0; y < bemu::gb::Screen::screen_height; y++) {
            for (int x = 0; x < bemu::gb::Screen::screen_width; x++) {
                m_image.setPixel(x, y, colors[m_emulator.m_screen.get_pixel(x, y)]);
            }
        }

        emit imageChanged();
    }

    [[nodiscard]] QImage image() const {
        std::scoped_lock lock{m_mutex};
        return m_image.copy();
    }

   public slots:
    void key_pressed(Qt::Key key) {
        auto &joypad = m_emulator.m_bus.m_joypad;

        switch (key) {
            case Qt::Key_Q: return joypad.set_button(bemu::gb::Joypad::BUTTON_A, true);
            case Qt::Key_E: return joypad.set_button(bemu::gb::Joypad::BUTTON_A, true);
            case Qt::Key_W: return joypad.set_button(bemu::gb::Joypad::BUTTON_UP, true);
            case Qt::Key_S: return joypad.set_button(bemu::gb::Joypad::BUTTON_DOWN, true);
            case Qt::Key_A: return joypad.set_button(bemu::gb::Joypad::BUTTON_LEFT, true);
            case Qt::Key_D: return joypad.set_button(bemu::gb::Joypad::BUTTON_RIGHT, true);
            case Qt::Key_X: return joypad.set_button(bemu::gb::Joypad::BUTTON_START, true);
            case Qt::Key_Z: return joypad.set_button(bemu::gb::Joypad::BUTTON_SELECT, true);
        }
    }
    void key_released(Qt::Key key) {
        auto &joypad = m_emulator.m_bus.m_joypad;

        switch (key) {
            case Qt::Key_Q: return joypad.set_button(bemu::gb::Joypad::BUTTON_A, false);
            case Qt::Key_E: return joypad.set_button(bemu::gb::Joypad::BUTTON_A, false);
            case Qt::Key_W: return joypad.set_button(bemu::gb::Joypad::BUTTON_UP, false);
            case Qt::Key_S: return joypad.set_button(bemu::gb::Joypad::BUTTON_DOWN, false);
            case Qt::Key_A: return joypad.set_button(bemu::gb::Joypad::BUTTON_LEFT, false);
            case Qt::Key_D: return joypad.set_button(bemu::gb::Joypad::BUTTON_RIGHT, false);
            case Qt::Key_X: return joypad.set_button(bemu::gb::Joypad::BUTTON_START, false);
            case Qt::Key_Z: return joypad.set_button(bemu::gb::Joypad::BUTTON_SELECT, false);
        }
    }

   signals:
    void imageChanged();

   private:
    mutable std::mutex m_mutex;
    QImage m_image;
    bemu::gb::Emulator &m_emulator;
};
