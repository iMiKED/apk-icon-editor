#include "busyindicator.h"
#include <QPainter>
#include <QTimer>

BusyIndicator::BusyIndicator(QWidget *parent) : QWidget(parent)
{
    step = 0;
    timer = new QTimer(this);
    timer->setInterval(80);
    connect(timer, &QTimer::timeout, [this]() {
        step = (step + 1) % 12;
        update();
    });
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize BusyIndicator::sizeHint() const
{
    return QSize(32, 32);
}

void BusyIndicator::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(QRectF(rect()).center());

    const int segmentCount = 12;
    const qreal size = qMin(width(), height());
    const qreal segmentWidth = qMax<qreal>(2, size * 0.09);
    const qreal segmentHeight = qMax<qreal>(5, size * 0.21);
    const qreal outerRadius = qMax<qreal>(segmentHeight, size / 2 - segmentWidth);
    const qreal radius = outerRadius - segmentHeight;
    const QColor color = palette().color(QPalette::Text);

    for (int i = 0; i < segmentCount; ++i) {
        QColor faded(color);
        const int age = (i - step + segmentCount) % segmentCount;
        faded.setAlpha(35 + ((segmentCount - age) * 220 / segmentCount));
        painter.setPen(Qt::NoPen);
        painter.setBrush(faded);
        painter.save();
        painter.rotate(i * 360.0 / segmentCount);
        painter.drawRoundedRect(QRectF(-segmentWidth / 2,
                                       -radius - segmentHeight,
                                       segmentWidth,
                                       segmentHeight),
                                segmentWidth / 2,
                                segmentWidth / 2);
        painter.restore();
    }
}

void BusyIndicator::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    timer->start();
}

void BusyIndicator::hideEvent(QHideEvent *event)
{
    timer->stop();
    QWidget::hideEvent(event);
}
