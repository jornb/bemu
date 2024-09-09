#include <QPainter>
#include <QQuickPaintedItem>

class Screen : public QQuickPaintedItem {
    Q_OBJECT

   public:
    explicit Screen(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) {
      setMipmap(false);
    }

    void paint(QPainter *painter) override {
        const auto w = static_cast<int>(this->width());
        const auto h = static_cast<int>(this->height());
        painter->fillRect(0, 0, w, h, Qt::black);

        painter->setPen(Qt::white);
        painter->drawEllipse(w / 2, h / 2, w / 2, h / 2);
    }
};
