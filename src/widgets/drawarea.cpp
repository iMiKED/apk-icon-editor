#include "drawarea.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QDir>

DrawArea::DrawArea(QWidget *parent) : QLabel(parent)
{
#ifndef Q_OS_OSX
    setFont(QFont("Open Sans Light", 12));
#else
    setFont(QFont("Open Sans", 15, QFont::Light));
#endif
    setAlignment(Qt::AlignCenter);
    setAllowHover(true);

    setMouseTracking(true);
    setAutoFillBackground(true);

    icon = NULL;
    bounds = QSize(0, 0);
    background = palette().color(QPalette::Window);
    previewShape = PreviewShapeNone;
    adaptivePreviewMode = AdaptivePreviewNormal;
}

void DrawArea::setIcon(Icon *icon)
{
    this->icon = icon;
    setAllowHover(!icon);
    repaint();
}

void DrawArea::mousePressEvent(QMouseEvent *event)
{
    if (!icon && event->button() == Qt::LeftButton) {
        emit clicked();
    }
    event->accept();
}

void DrawArea::paintEvent(QPaintEvent *event)
{
    if (icon) {

        // Border bounds:
        const int bx = width() / 2 - bounds.width() / 2;
        const int by = height() / 2 - bounds.height() / 2;
        const int bw = bounds.width();
        const int bh = bounds.height();

        // Paint border and icon:
        QPainter painter(this);
        painter.fillRect(bx + 1, by + 1, bw - 1, bh - 1, background);
        if (background == palette().color(QPalette::Window)) {
            painter.fillRect(bx + 1, by + 1, bw - 1, bh - 1, Qt::Dense7Pattern);
        }
        const QPixmap preview = adaptivePreviewMode == AdaptivePreviewThemed
                ? icon->getThemedPixmap()
                : icon->getPixmap();
        const QRectF iconBounds(width() / 2 - preview.width() / 2,
                                height() / 2 - preview.height() / 2,
                                preview.width(),
                                preview.height());
        if (icon->isAdaptiveIcon() && previewShape != PreviewShapeNone) {
            QPainterPath clip;
            const qreal size = qMin(iconBounds.width(), iconBounds.height());
            const QRectF square(iconBounds.center().x() - size / 2,
                                iconBounds.center().y() - size / 2,
                                size,
                                size);
            if (previewShape == PreviewShapeCircle) {
                clip.addEllipse(square);
            } else if (previewShape == PreviewShapeRoundedSquare) {
                clip.addRoundedRect(square, size * 0.22, size * 0.22);
            } else {
                const qreal soft = size * 0.16;
                clip.moveTo(square.center().x(), square.top());
                clip.cubicTo(square.right() - soft, square.top(),
                             square.right(), square.top() + soft,
                             square.right(), square.center().y());
                clip.cubicTo(square.right(), square.bottom() - soft,
                             square.right() - soft, square.bottom(),
                             square.center().x(), square.bottom());
                clip.cubicTo(square.left() + soft, square.bottom(),
                             square.left(), square.bottom() - soft,
                             square.left(), square.center().y());
                clip.cubicTo(square.left(), square.top() + soft,
                             square.left() + soft, square.top(),
                             square.center().x(), square.top());
                clip.closeSubpath();
            }
            painter.save();
            painter.setClipPath(clip);
            painter.drawPixmap(iconBounds.topLeft(), preview);
            painter.restore();
        } else {
            painter.drawPixmap(iconBounds.topLeft(), preview);
        }
        painter.drawRect(bx, by, bw, bh);
        painter.setPen(Qt::lightGray);
        painter.drawRect(bx - 1, by - 1, bw + 2, bh + 2);
    }
    else {
        QLabel::paintEvent(event);
    }
}

void DrawArea::setAllowHover(bool allow)
{
    QString style =
        "DrawArea { background: url(:/gfx/background.png)"
        "no-repeat bottom left;"
        "border: 1px solid gray; }"
    ;
    if (allow) style += "DrawArea::hover { background-color: white; }";
    setStyleSheet(style);
    setCursor(allow ? Qt::PointingHandCursor : Qt::ArrowCursor);
}
