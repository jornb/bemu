#pragma once
#include <QImage>
#include <QObject>
#include <bemu/gb/emulator.hpp>
#include <mutex>

class GuiEmulator : public QObject {
    Q_OBJECT
    Q_PROPERTY(QImage image READ image NOTIFY imageChanged)

   public:
    explicit GuiEmulator(QObject *parent = nullptr) : QObject(parent) {}

    void on_screen_rendered(const bemu::gb::Screen &screen) {
        std::scoped_lock lock{m_mutex};

        m_image = QImage(screen.screen_width, screen.screen_height, QImage::Format_RGB888);

        static std::array colors = {qRgb(0, 0, 0), qRgb(85, 85, 85), qRgb(170, 170, 170), qRgb(255, 255, 255)};

        for (int y = 0; y < screen.screen_height; y++) {
            for (int x = 0; x < screen.screen_width; x++) {
                m_image.setPixel(x, y, colors[screen.get_pixel(x, y)]);
            }
        }

        emit imageChanged();
    }

    [[nodiscard]] QImage image() const {
        std::scoped_lock lock{m_mutex};
        return m_image.copy();
    }

   signals:
    void imageChanged();

   private:
    mutable std::mutex m_mutex;
    QImage m_image;
};
