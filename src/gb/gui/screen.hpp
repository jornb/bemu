#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>

class Screen : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QImage image READ image WRITE setImage)

   public:
    explicit Screen(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) { setMipmap(false); }

    QImage image() const { return m_image; }
    void setImage(const QImage &image) {
        m_image = image;
        update();
    }

    void paint(QPainter *painter) override {
        const auto w = static_cast<int>(this->width());
        const auto h = static_cast<int>(this->height());
        painter->fillRect(0, 0, w, h, Qt::black);

        // painter->setPen(Qt::white);
        // painter->drawEllipse(w / 2, h / 2, w / 2, h / 2);

        painter->drawImage(0, 0, m_image);
    }

   private:
    QImage m_image;
};
