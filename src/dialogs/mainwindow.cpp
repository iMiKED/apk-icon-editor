#include "mainwindow.h"
#include "globals.h"
#include "settings.h"
#include "apkfile.h"
#include "decorationdelegate.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QContextMenuEvent>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QFileDialog>
#include <QMimeData>
#include <QPaintEvent>
#include <QRegularExpression>
#include <QDesktopServices>
#include <QEvent>
#include <QPainterPath>
#include <QProcess>
#include <QDebug>
#include <QPainter>
#include <QProxyStyle>
#include <QPointer>
#include <QResizeEvent>
#include <QRegion>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QStyleOption>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <QtConcurrent/QtConcurrentRun>
#include <QApplication>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <windowsx.h>
#endif

static const char THEME_SYSTEM[] = "system";
static const char THEME_LIGHT[] = "light";
static const char THEME_DARK[] = "dark";

static QPalette defaultPalette()
{
    static const QPalette palette = qApp->palette();
    return palette;
}

static QString defaultStyleName()
{
    static const QString styleName = qApp->style()->objectName();
    return styleName;
}

static bool isSystemDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

static QColor defaultAccentColor()
{
    return QColor(0, 120, 215);
}

static QColor systemAccentColor()
{
#ifdef Q_OS_WIN
    const QStringList keys = QStringList()
        << "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM"
        << "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent";
    const QStringList values = QStringList() << "AccentColor" << "AccentColorMenu";

    foreach (const QString &key, keys) {
        QSettings registry(key, QSettings::NativeFormat);
        foreach (const QString &name, values) {
            if (!registry.contains(name)) {
                continue;
            }

            bool ok = false;
            const QVariant rawValue = registry.value(name);
            qulonglong wideValue = rawValue.toULongLong(&ok);
            if (!ok) {
                const qlonglong signedValue = rawValue.toLongLong(&ok);
                wideValue = quint32(signedValue);
            }
            if (!ok) {
                continue;
            }

            const quint32 value = quint32(wideValue);
            const QColor color(value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff);
            if (color.isValid()) {
                return color;
            }
        }
    }
#endif
    return defaultAccentColor();
}

static QColor titleButtonHoverColor(bool dark)
{
    QColor color = systemAccentColor();
    color.setAlpha(dark ? 96 : 44);
    return color;
}

static QColor titleButtonPressedColor()
{
    return systemAccentColor().darker(115);
}

static QString colorName(const QColor &color)
{
    return color.name(QColor::HexRgb);
}

static bool systemAccentWindowBordersEnabled()
{
#ifdef Q_OS_WIN
    QSettings registry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\DWM", QSettings::NativeFormat);
    return registry.value("ColorPrevalence", 0).toInt() != 0;
#else
    return false;
#endif
}

static QColor windowBorderColor(bool dark)
{
    if (systemAccentWindowBordersEnabled()) {
        return systemAccentColor();
    }

    return dark ? QColor(85, 85, 90) : QColor(204, 204, 204);
}

enum class TitleButtonKind {
    Minimize,
    Maximize,
    Restore,
    Close
};

class TitleBarButton : public QToolButton
{
public:
    explicit TitleBarButton(TitleButtonKind kind, QWidget *parent = nullptr)
        : QToolButton(parent), kind(kind)
    {
        setObjectName("customTitleButton");
        setAutoRaise(true);
        setFixedSize(46, 32);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::ArrowCursor);
        setAttribute(Qt::WA_Hover, true);
        setMouseTracking(true);
    }

    void setKind(TitleButtonKind newKind)
    {
        if (kind == newKind) {
            return;
        }
        kind = newKind;
        update();
    }

protected:
    bool event(QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverEnter:
            hovered = true;
            update();
            break;
        case QEvent::Leave:
        case QEvent::HoverLeave:
            hovered = false;
            update();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            update();
            break;
        default:
            break;
        }
        return QToolButton::event(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const bool pressed = isDown();
        const bool dark = parentWidget() && parentWidget()->property("darkTitleBar").toBool();
        const bool closeButton = kind == TitleButtonKind::Close;
        if (closeButton && hovered) {
            painter.fillRect(rect(), pressed ? QColor(153, 31, 23) : QColor(196, 43, 28));
        } else if (pressed) {
            painter.fillRect(rect(), titleButtonPressedColor());
        } else if (hovered) {
            painter.fillRect(rect(), titleButtonHoverColor(dark));
        }

        const QColor color = closeButton && hovered ? QColor(255, 255, 255)
            : palette().color(QPalette::WindowText);
        QPen pen(color, 1.0);
        pen.setCosmetic(true);
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        const qreal cx = width() / 2.0;
        const qreal cy = height() / 2.0;
        switch (kind) {
        case TitleButtonKind::Minimize:
            painter.drawLine(QPointF(cx - 4.5, cy + 4.0), QPointF(cx + 4.5, cy + 4.0));
            break;
        case TitleButtonKind::Maximize:
            painter.drawRect(QRectF(cx - 4.5, cy - 4.5, 9.0, 9.0));
            break;
        case TitleButtonKind::Restore:
            painter.drawRect(QRectF(cx - 2.5, cy - 5.5, 7.5, 7.5));
            painter.fillRect(QRectF(cx - 5.0, cy - 3.0, 7.5, 7.5),
                             dark ? QColor(45, 45, 48) : QColor(255, 255, 255));
            painter.drawRect(QRectF(cx - 5.0, cy - 3.0, 7.5, 7.5));
            break;
        case TitleButtonKind::Close:
            painter.drawLine(QPointF(cx - 4.5, cy - 4.5), QPointF(cx + 4.5, cy + 4.5));
            painter.drawLine(QPointF(cx + 4.5, cy - 4.5), QPointF(cx - 4.5, cy + 4.5));
            break;
        }
    }

private:
    TitleButtonKind kind;
    bool hovered = false;
};

class AccentBorderOverlay : public QWidget
{
public:
    explicit AccentBorderOverlay(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName("accentBorderOverlay");
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFocusPolicy(Qt::NoFocus);
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        QRegion border(rect());
        const QRect inner = rect().adjusted(1, 1, -1, -1);
        if (inner.isValid()) {
            border -= QRegion(inner);
        }
        setMask(border);
    }

    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)

        QWidget *window = parentWidget();
        if (window && (window->isMaximized() || window->isFullScreen())) {
            return;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        const bool dark = qApp->palette().color(QPalette::Window).lightness() < 128;
        painter.setPen(QPen(windowBorderColor(dark), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
};

class FramelessTitleBar : public QWidget
{
public:
    explicit FramelessTitleBar(QWidget *targetWindow, QWidget *parent = nullptr)
        : QWidget(parent), targetWindow(targetWindow)
    {
        setObjectName("customTitleBar");
        setFixedHeight(32);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        titleLabel = new QLabel(this);
        titleLabel->setObjectName("customTitleLabel");
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        btnIcon = new QToolButton(this);
        btnIcon->setObjectName("customTitleIconButton");
        btnIcon->setAutoRaise(true);
        btnIcon->setFixedSize(32, 32);
        btnIcon->setIconSize(QSize(16, 16));
        btnIcon->setFocusPolicy(Qt::NoFocus);
        btnIcon->setCursor(Qt::ArrowCursor);

        btnMinimize = new TitleBarButton(TitleButtonKind::Minimize, this);
        btnMaximize = new TitleBarButton(TitleButtonKind::Maximize, this);
        btnClose = new TitleBarButton(TitleButtonKind::Close, this);
        btnClose->setObjectName("customTitleCloseButton");

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(btnIcon);
        layout->addWidget(titleLabel);
        layout->addWidget(btnMinimize);
        layout->addWidget(btnMaximize);
        layout->addWidget(btnClose);

        connect(targetWindow, &QWidget::windowTitleChanged, this, &FramelessTitleBar::syncTitle);
        connect(btnIcon, &QToolButton::clicked, [this]() {
            showWindowMenu(btnIcon->mapToGlobal(QPoint(0, btnIcon->height())));
        });
        connect(btnMinimize, &QToolButton::clicked, targetWindow, &QWidget::showMinimized);
        connect(btnMaximize, &QToolButton::clicked, this, &FramelessTitleBar::toggleMaximized);
        connect(btnClose, &QToolButton::clicked, targetWindow, &QWidget::close);
        titleLabel->installEventFilter(this);
        targetWindow->installEventFilter(this);
        syncIcon();
        syncTitle();
        syncMaximizeButton();
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == titleLabel && event->type() == QEvent::MouseButtonDblClick) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                toggleMaximized();
                mouseEvent->accept();
                return true;
            }
        }
        if (object == targetWindow && event->type() == QEvent::WindowStateChange) {
            syncMaximizeButton();
        }
        if (object == targetWindow && event->type() == QEvent::WindowIconChange) {
            syncIcon();
        }
        return QWidget::eventFilter(object, event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        showWindowMenu(event->globalPos());
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            toggleMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && targetWindow) {
            if (targetWindow->isMaximized()) {
                QWidget::mousePressEvent(event);
                return;
            }
            if (QWindow *handle = targetWindow->windowHandle()) {
                handle->startSystemMove();
                event->accept();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }

private:
    void showWindowMenu(const QPoint &globalPos)
    {
        QMenu menu(this);
        QAction *restore = menu.addAction("Restore");
        QAction *minimize = menu.addAction("Minimize");
        QAction *maximize = menu.addAction("Maximize");
        menu.addSeparator();
        QAction *close = menu.addAction("Close");

        const bool maximized = targetWindow && targetWindow->isMaximized();
        restore->setEnabled(maximized);
        maximize->setEnabled(!maximized);

        QAction *selected = menu.exec(globalPos);
        if (!selected || !targetWindow) {
            return;
        }
        if (selected == restore) {
            targetWindow->showNormal();
        } else if (selected == minimize) {
            targetWindow->showMinimized();
        } else if (selected == maximize) {
            targetWindow->showMaximized();
        } else if (selected == close) {
            targetWindow->close();
        }
        syncMaximizeButton();
    }

    void syncIcon()
    {
        QIcon icon = targetWindow ? targetWindow->windowIcon() : QIcon();
        if (icon.isNull()) {
            icon = qApp->windowIcon();
        }
        if (icon.isNull()) {
            icon.addPixmap(QPixmap(":/gfx/icon/16.png"));
            icon.addPixmap(QPixmap(":/gfx/icon/24.png"));
            icon.addPixmap(QPixmap(":/gfx/icon/32.png"));
        }
        btnIcon->setIcon(icon);
    }

    void syncTitle()
    {
        QString title = targetWindow ? targetWindow->windowTitle().remove("[*]") : QString();
        if (title.isEmpty()) {
            title = qApp->applicationDisplayName();
        }
        titleLabel->setText(title);
    }

    void syncMaximizeButton()
    {
        btnMaximize->setKind(targetWindow && targetWindow->isMaximized()
            ? TitleButtonKind::Restore
            : TitleButtonKind::Maximize);
    }

    void toggleMaximized()
    {
        if (!targetWindow) {
            return;
        }
        if (targetWindow->isMaximized()) {
            targetWindow->showNormal();
        } else {
            targetWindow->showMaximized();
        }
        syncMaximizeButton();
    }

    QWidget *targetWindow;
    QToolButton *btnIcon;
    QLabel *titleLabel;
    TitleBarButton *btnMinimize;
    TitleBarButton *btnMaximize;
    TitleBarButton *btnClose;
};

class DarkProxyStyle : public QProxyStyle
{
public:
    explicit DarkProxyStyle(QStyle *style) : QProxyStyle(style) {}

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override
    {
        if ((element == PE_IndicatorCheckBox || element == PE_IndicatorRadioButton) && option) {
            drawChoiceIndicator(element == PE_IndicatorRadioButton, option, painter);
            return;
        }

        if ((element == PE_IndicatorSpinUp || element == PE_IndicatorSpinDown) && option) {
            drawSpinArrow(element == PE_IndicatorSpinUp, option, painter);
            return;
        }

        if ((element == PE_IndicatorArrowRight || element == PE_IndicatorArrowDown) && option) {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(241, 241, 241));
            const QRect r = option->rect;
            const QPoint center = r.center();
            QPolygon arrow;
            if (element == PE_IndicatorArrowRight) {
                arrow << QPoint(center.x() - 2, center.y() - 5)
                      << QPoint(center.x() - 2, center.y() + 5)
                      << QPoint(center.x() + 4, center.y());
            } else {
                arrow << QPoint(center.x() - 5, center.y() - 2)
                      << QPoint(center.x() + 5, center.y() - 2)
                      << QPoint(center.x(), center.y() + 4);
            }
            painter->drawPolygon(arrow);
            painter->restore();
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override
    {
        QStyleOptionMenuItem menuCopy;
        const QStyleOptionMenuItem *menuItem = nullptr;
        const bool checkedMenuItem = element == CE_MenuItem
            && (menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option))
            && menuItem->checked
            && menuItem->checkType != QStyleOptionMenuItem::NotCheckable;
        if (element == CE_MenuItem) {
            if (checkedMenuItem) {
                menuCopy = *menuItem;
                menuCopy.checked = false;
                menuCopy.checkType = QStyleOptionMenuItem::NotCheckable;
                option = &menuCopy;
            }
        }

        if (checkedMenuItem && menuItem->checkType != QStyleOptionMenuItem::Exclusive && !menuItem->icon.isNull()) {
            painter->save();
            painter->setPen(Qt::NoPen);
            painter->setBrush(systemAccentColor());
            painter->drawRect(menuIndicatorFrameRect(menuItem));
            painter->restore();
        }

        QProxyStyle::drawControl(element, option, painter, widget);

        if (element != CE_MenuItem) {
            return;
        }

        if (!menuItem) {
            menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
        }
        if (!menuItem || !menuItem->checked || menuItem->checkType == QStyleOptionMenuItem::NotCheckable) {
            return;
        }

        const bool exclusiveLike = menuItem->checkType == QStyleOptionMenuItem::Exclusive;
        if (!exclusiveLike && !menuItem->icon.isNull()) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = menuIndicatorRect(menuItem, QSizeF(12.0, 12.0));
        painter->setPen(QPen(QColor(241, 241, 241), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);

        if (exclusiveLike) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(241, 241, 241));
            painter->drawEllipse(QRectF(r.center().x() - 3.0, r.center().y() - 3.0, 6.0, 6.0));
        } else {
            QPainterPath path;
            path.moveTo(r.left() + 1.0, r.center().y());
            path.lineTo(r.center().x() - 1.0, r.bottom() - 1.5);
            path.lineTo(r.right() - 0.5, r.top() + 1.5);
            painter->drawPath(path);
        }
        painter->restore();
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget = nullptr) const override
    {
        QProxyStyle::drawComplexControl(control, option, painter, widget);

        if (control != CC_SpinBox) {
            return;
        }

        const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option);
        if (!spinBox) {
            return;
        }

        drawSpinButton(spinBox, SC_SpinBoxUp, painter, widget);
        drawSpinButton(spinBox, SC_SpinBoxDown, painter, widget);
    }

private:
    static QRectF menuIndicatorRect(const QStyleOptionMenuItem *menuItem, const QSizeF &size)
    {
        const qreal columnWidth = 28.0;
        const qreal columnLeft = menuItem->direction == Qt::RightToLeft
            ? menuItem->rect.right() + 1.0 - columnWidth
            : menuItem->rect.left();
        return QRectF(columnLeft + (columnWidth - size.width()) / 2.0,
                      menuItem->rect.top() + (menuItem->rect.height() - size.height()) / 2.0,
                      size.width(),
                      size.height());
    }

    static QRectF menuIndicatorFrameRect(const QStyleOptionMenuItem *menuItem)
    {
        const qreal width = qMin<qreal>(26.0, qMax<qreal>(22.0, menuItem->maxIconWidth + 8.0));
        const qreal height = qMin<qreal>(24.0, qMax<qreal>(20.0, menuItem->rect.height() - 2.0));
        return menuIndicatorRect(menuItem, QSizeF(width, height));
    }

    static void drawChoiceIndicator(bool radio, const QStyleOption *option, QPainter *painter)
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool checked = option->state & State_On;
        const bool enabled = option->state & State_Enabled;
        const bool hover = option->state & State_MouseOver;
        QRect r = option->rect.adjusted(1, 1, -1, -1);

        QColor border = enabled ? QColor(154, 154, 160) : QColor(85, 85, 90);
        if (hover && enabled) {
            border = QColor(200, 200, 200);
        }

        painter->setPen(QPen(border, 1.2));
        painter->setBrush(QColor(37, 37, 38));
        if (radio) {
            painter->drawEllipse(r);
        } else {
            painter->drawRoundedRect(r, 2, 2);
        }

        if (checked) {
            painter->setPen(QPen(enabled ? QColor(241, 241, 241) : QColor(140, 140, 140), 1.7,
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            if (radio) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(enabled ? QColor(241, 241, 241) : QColor(140, 140, 140));
                const QPointF center = QRectF(r).center();
                painter->drawEllipse(QRectF(center.x() - 3.0, center.y() - 3.0, 6.0, 6.0));
            } else {
                QPainterPath path;
                path.moveTo(r.left() + 3, r.center().y());
                path.lineTo(r.center().x() - 1, r.bottom() - 3);
                path.lineTo(r.right() - 2, r.top() + 3);
                painter->drawPath(path);
            }
        }

        painter->restore();
    }

    static void drawSpinArrow(bool up, const QStyleOption *option, QPainter *painter)
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(option->state & State_Enabled ? QColor(241, 241, 241) : QColor(140, 140, 140));
        const QPoint c = option->rect.center();
        QPolygon arrow;
        if (up) {
            arrow << QPoint(c.x() - 4, c.y() + 2)
                  << QPoint(c.x() + 4, c.y() + 2)
                  << QPoint(c.x(), c.y() - 3);
        } else {
            arrow << QPoint(c.x() - 4, c.y() - 2)
                  << QPoint(c.x() + 4, c.y() - 2)
                  << QPoint(c.x(), c.y() + 3);
        }
        painter->drawPolygon(arrow);
        painter->restore();
    }

    void drawSpinButton(const QStyleOptionSpinBox *spinBox, SubControl control, QPainter *painter,
                        const QWidget *widget) const
    {
        const QRect buttonRect = subControlRect(CC_SpinBox, spinBox, control, widget);
        if (buttonRect.isEmpty()) {
            return;
        }

        const bool enabled = spinBox->state & State_Enabled;
        const bool active = spinBox->activeSubControls & control;
        const bool pressed = active && (spinBox->state & State_Sunken);

        painter->save();
        painter->setPen(QPen(QColor(85, 85, 90), 1));
        painter->setBrush(pressed ? systemAccentColor() : active ? QColor(75, 75, 82) : QColor(63, 63, 70));
        painter->drawRect(buttonRect.adjusted(0, 0, -1, -1));

        QStyleOption arrowOption;
        arrowOption.rect = buttonRect.adjusted(3, 2, -3, -2);
        arrowOption.state = enabled ? State_Enabled : State_None;
        drawSpinArrow(control == SC_SpinBoxUp, &arrowOption, painter);
        painter->restore();
    }
};

class IconSizePresetComboBox : public QComboBox
{
public:
    explicit IconSizePresetComboBox(QWidget *parent = nullptr) : QComboBox(parent)
    {
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        const bool dark = palette().color(QPalette::Base).lightness() < 64;
        if (!dark) {
            QComboBox::paintEvent(event);
            return;
        }

        Q_UNUSED(event)
        const bool enabled = isEnabled();
        const QColor base = palette().color(QPalette::Base);
        const QColor text = enabled ? palette().color(QPalette::Text) : QColor(140, 140, 140);
        const QColor border = hasFocus() || underMouse() ? QColor(102, 102, 109) : QColor(85, 85, 90);
        const QColor arrow = enabled ? QColor(241, 241, 241) : QColor(140, 140, 140);

        QPainter painter(this);
        painter.fillRect(rect(), base);
        painter.setPen(border);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        const int margin = 5;
        const int arrowWidth = 24;
        int left = margin;
        const QIcon icon = itemIcon(currentIndex());
        if (!icon.isNull()) {
            const int size = qMin(16, qMax(0, height() - 8));
            const QRect iconRect(left, (height() - size) / 2, size, size);
            icon.paint(&painter, iconRect, Qt::AlignCenter, enabled ? QIcon::Normal : QIcon::Disabled);
            left += size + margin;
        }

        const QRect textRect(left, 0, qMax(0, width() - left - arrowWidth - margin), height());
        painter.setPen(text);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                         fontMetrics().elidedText(currentText(), Qt::ElideRight, textRect.width()));

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(arrow);
        const QPoint c(width() - arrowWidth / 2, height() / 2);
        QPolygonF polygon;
        polygon << QPointF(c.x() - 4.0, c.y() - 2.0)
                << QPointF(c.x() + 4.0, c.y() - 2.0)
                << QPointF(c.x(), c.y() + 3.0);
        painter.drawPolygon(polygon);
    }
};

static QPalette lightPalette()
{
    QPalette palette = defaultPalette();
    palette.setColor(QPalette::Highlight, systemAccentColor());
    palette.setColor(QPalette::HighlightedText, Qt::white);
    return palette;
}

static QPalette darkPalette()
{
    const QColor accent = systemAccentColor();
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(45, 45, 48));
    palette.setColor(QPalette::WindowText, QColor(241, 241, 241));
    palette.setColor(QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::AlternateBase, QColor(37, 37, 38));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 48));
    palette.setColor(QPalette::ToolTipText, QColor(241, 241, 241));
    palette.setColor(QPalette::Text, QColor(241, 241, 241));
    palette.setColor(QPalette::Button, QColor(63, 63, 70));
    palette.setColor(QPalette::ButtonText, QColor(241, 241, 241));
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, QColor(88, 166, 255));
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(140, 140, 140));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(140, 140, 140));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(140, 140, 140));
    return palette;
}

static QString darkStyleSheet()
{
    const QColor accent = systemAccentColor();
    const QColor accentBorder = accent.lighter(135);
    const QString accentName = colorName(accent);
    const QString accentBorderName = colorName(accentBorder);
    return QString(
        "QMainWindow, QDialog, QMessageBox, QColorDialog { color: #f1f1f1; background-color: #2d2d30; selection-background-color: %1; selection-color: #ffffff; }"
        "QMenuBar { background-color: #2d2d30; color: #f1f1f1; }"
        "QMenuBar::item { background: transparent; padding: 3px 8px; }"
        "QMenuBar::item:selected, QMenuBar::item:pressed { background-color: #3f3f46; }"
        "QMenuBar QToolButton { background: transparent; border: none; border-radius: 0px; padding: 1px 6px; margin: 0px; }"
        "QMenuBar QToolButton:hover { background-color: #3f3f46; }"
        "QTabWidget::pane { border: 1px solid #55555a; background-color: #2d2d30; }"
        "QTabBar::tab { background-color: #3f3f46; color: #f1f1f1; border: 1px solid #55555a; padding: 5px 10px; }"
        "QTabBar::tab:selected { background-color: #2d2d30; border-bottom-color: #2d2d30; }"
        "QTabBar::tab:!selected { background-color: #252526; }"
        "QHeaderView::section { background-color: #3f3f46; color: #f1f1f1; border: 1px solid #55555a; padding: 4px; }"
        "QTableView, QListView, QTreeView, QPlainTextEdit, QTextEdit, QLineEdit, QSpinBox, QComboBox {"
        " background-color: #1e1e1e; color: #f1f1f1; border: 1px solid #55555a; selection-background-color: %1; selection-color: #ffffff; }"
        "QListView#iconsList { background-color: #1e1e1e; alternate-background-color: #1e1e1e; outline: 0px; }"
        "QListView#iconsList::item { background-color: #1e1e1e; color: #f1f1f1; padding: 1px; }"
        "QListView#iconsList::item:hover:!selected { background-color: #252526; color: #f1f1f1; }"
        "QListView#iconsList::item:selected, QListView#iconsList::item:selected:active, QListView#iconsList::item:selected:!active {"
        " background-color: %1; color: #ffffff; }"
        "QComboBox { min-height: 22px; padding: 3px 28px 3px 5px; }"
        "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; top: 0px; right: 0px; bottom: 0px; width: 24px; border: none; background: transparent; }"
        "QComboBox::drop-down:on, QComboBox::drop-down:open { background: transparent; border: none; }"
        "QComboBox::down-arrow { image: url(:/gfx/actions/combo-arrow-down-light.xpm); width: 9px; height: 5px; }"
        "QComboBox::down-arrow:on, QComboBox::down-arrow:open { image: url(:/gfx/actions/combo-arrow-down-light.xpm); width: 9px; height: 5px; }"
        "QComboBox::down-arrow:disabled { image: url(:/gfx/actions/combo-arrow-down-light.xpm); }"
        "QComboBox QAbstractItemView { background-color: #1e1e1e; color: #f1f1f1; border: 1px solid #55555a; outline: 0px; selection-background-color: %1; }"
        "QPushButton, QToolButton { background-color: #3f3f46; color: #f1f1f1; border: 1px solid #66666d; border-radius: 3px; padding: 4px 8px; }"
        "QPushButton:hover, QToolButton:hover { background-color: #4b4b52; }"
        "QPushButton:pressed, QToolButton:pressed, QToolButton:checked { background-color: %1; border-color: %2; }"
        "QPushButton:disabled, QToolButton:disabled { background-color: #333337; color: #8c8c8c; border-color: #44444a; }"
        "QCheckBox, QRadioButton, QGroupBox, QLabel { color: #f1f1f1; background-color: transparent; }"
        "QGroupBox { border: 1px solid #55555a; border-radius: 3px; margin-top: 8px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 3px; background-color: #2d2d30; color: #f1f1f1; }"
        "QProgressBar { border: 1px solid #55555a; background-color: #1e1e1e; color: #f1f1f1; text-align: center; }"
        "QProgressBar::chunk { background-color: %1; }"
        "QSlider::groove:horizontal { height: 4px; background: #55555a; }"
        "QSlider::handle:horizontal { background: #f1f1f1; border: 1px solid #77777d; width: 12px; margin: -5px 0; border-radius: 6px; }"
        "QDialogButtonBox QPushButton { min-width: 72px; }"
        "QToolTip { color: #f1f1f1; background-color: #2d2d30; border: 1px solid #767676; }"
    ).arg(accentName, accentBorderName);
}

static QString framelessTitleBarStyleSheet(bool dark)
{
    const QString accentName = colorName(systemAccentColor());
    if (dark) {
        return QString(
            "#customTitleBar { background-color: #2d2d30; border: none; }"
            "#customTitleLabel { color: #f1f1f1; padding-left: 4px; }"
            "#customTitleIconButton, #customTitleButton, #customTitleCloseButton {"
            " background: transparent; color: #f1f1f1; border: none; border-radius: 0px; padding: 0px; margin: 0px; }"
            "#customTitleIconButton:hover { background-color: #3f3f46; }"
            "#customTitleIconButton:pressed { background-color: %1; }"
        ).arg(accentName);
    }

    return QString(
        "#customTitleBar { background-color: #ffffff; border: none; }"
        "#customTitleLabel { color: #202020; padding-left: 4px; }"
        "#customTitleIconButton, #customTitleButton, #customTitleCloseButton {"
        " background: transparent; color: #202020; border: none; border-radius: 0px; padding: 0px; margin: 0px; }"
        "#customTitleIconButton:hover { background-color: #e5e5e5; }"
        "#customTitleIconButton:pressed { background-color: %1; color: #ffffff; }"
    ).arg(accentName);
}

#ifdef Q_OS_WIN
struct DwmMargins
{
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
};

static DWORD windowsColorRef(const QColor &color)
{
    return (DWORD(color.blue()) << 16) | (DWORD(color.green()) << 8) | DWORD(color.red());
}

static HWND hwndForWidget(QWidget *widget)
{
    return widget ? reinterpret_cast<HWND>(widget->winId()) : nullptr;
}

static void setWindowsFramelessFrame(QWidget *widget, bool forceNativeFrameRefresh = false)
{
    if (!widget || !widget->isWindow() || !widget->windowFlags().testFlag(Qt::FramelessWindowHint)) {
        return;
    }

    HWND hwnd = hwndForWidget(widget);
    if (!hwnd) {
        return;
    }

    using GetWindowLongPtrFunc = LONG_PTR (WINAPI *)(HWND, int);
    using SetWindowLongPtrFunc = LONG_PTR (WINAPI *)(HWND, int, LONG_PTR);
    using SetWindowPosFunc = BOOL (WINAPI *)(HWND, HWND, int, int, int, int, UINT);
    using GetWindowRectFunc = BOOL (WINAPI *)(HWND, LPRECT);
    HMODULE user32 = LoadLibraryW(L"user32.dll");
    if (!user32) {
        return;
    }

    auto getWindowLongPtr = reinterpret_cast<GetWindowLongPtrFunc>(GetProcAddress(user32, "GetWindowLongPtrW"));
    auto setWindowLongPtr = reinterpret_cast<SetWindowLongPtrFunc>(GetProcAddress(user32, "SetWindowLongPtrW"));
    auto setWindowPos = reinterpret_cast<SetWindowPosFunc>(GetProcAddress(user32, "SetWindowPos"));
    auto getWindowRect = reinterpret_cast<GetWindowRectFunc>(GetProcAddress(user32, "GetWindowRect"));
    if (!getWindowLongPtr || !setWindowLongPtr || !setWindowPos) {
        FreeLibrary(user32);
        return;
    }

    LONG_PTR style = getWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    setWindowLongPtr(hwnd, GWL_STYLE, style);

    HMODULE module = LoadLibraryW(L"dwmapi.dll");
    if (module) {
        using DwmExtendFrameIntoClientAreaFunc = HRESULT (WINAPI *)(HWND, const DwmMargins *);
        auto extendFrame = reinterpret_cast<DwmExtendFrameIntoClientAreaFunc>(
                    GetProcAddress(module, "DwmExtendFrameIntoClientArea"));
        if (extendFrame) {
            const DwmMargins margins = { 1, 1, 1, 1 };
            extendFrame(hwnd, &margins);
        }
        FreeLibrary(module);
    }

    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    setWindowPos(hwnd, nullptr, 0, 0, 0, 0, flags);
    if (forceNativeFrameRefresh && getWindowRect) {
        RECT rect;
        if (getWindowRect(hwnd, &rect)) {
            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;
            setWindowPos(hwnd, nullptr, 0, 0, width, height + 1,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            setWindowPos(hwnd, nullptr, 0, 0, width, height,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }
    FreeLibrary(user32);
}
#endif

static void setWindowsDarkTitleBar(QWidget *widget, bool dark, bool forceNativeFrameRefresh = false)
{
#ifdef Q_OS_WIN
    if (!widget || !widget->isWindow()) {
        return;
    }

    setWindowsFramelessFrame(widget, forceNativeFrameRefresh);

    using DwmSetWindowAttributeFunc = HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE module = LoadLibraryW(L"dwmapi.dll");
    if (!module) {
        return;
    }

    auto setAttribute = reinterpret_cast<DwmSetWindowAttributeFunc>(GetProcAddress(module, "DwmSetWindowAttribute"));
    if (setAttribute) {
        const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_CURRENT = 20;
        const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_OLD = 19;
        const DWORD DWMWA_BORDER_COLOR = 34;
        const DWORD DWMWA_CAPTION_COLOR = 35;
        const DWORD DWMWA_TEXT_COLOR = 36;
        const DWORD DWMWA_COLOR_DEFAULT = 0xFFFFFFFF;
        const DWORD darkCaptionColor = 0x00302d2d;
        const DWORD darkTextColor = 0x00f1f1f1;
        HWND hwnd = reinterpret_cast<HWND>(widget->winId());
        const bool frameless = widget->windowFlags().testFlag(Qt::FramelessWindowHint);
        const DWORD captionColor = dark ? darkCaptionColor : DWMWA_COLOR_DEFAULT;
        const DWORD borderColor = frameless ? windowsColorRef(windowBorderColor(dark)) : captionColor;
        const DWORD textColor = dark ? darkTextColor : DWMWA_COLOR_DEFAULT;
        const BOOL systemDarkCaption = dark ? TRUE : FALSE;
        if (FAILED(setAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_CURRENT,
                                &systemDarkCaption, sizeof(systemDarkCaption)))) {
            setAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, &systemDarkCaption, sizeof(systemDarkCaption));
        }

        auto applyCaptionColors = [&]() {
            setAttribute(hwnd, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
            setAttribute(hwnd, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
            setAttribute(hwnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
        };
        applyCaptionColors();
        using SetWindowPosFunc = BOOL (WINAPI *)(HWND, HWND, int, int, int, int, UINT);
        using GetWindowRectFunc = BOOL (WINAPI *)(HWND, LPRECT);
        HMODULE user32 = LoadLibraryW(L"user32.dll");
        if (user32) {
            auto setWindowPos = reinterpret_cast<SetWindowPosFunc>(GetProcAddress(user32, "SetWindowPos"));
            auto getWindowRect = reinterpret_cast<GetWindowRectFunc>(GetProcAddress(user32, "GetWindowRect"));
            if (setWindowPos) {
                setWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                if (forceNativeFrameRefresh && getWindowRect) {
                    RECT rect;
                    if (getWindowRect(hwnd, &rect)) {
                        const int width = rect.right - rect.left;
                        const int height = rect.bottom - rect.top;
                        setWindowPos(hwnd, nullptr, 0, 0, width, height + 1,
                                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                        setWindowPos(hwnd, nullptr, 0, 0, width, height,
                                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
                    }
                }
            }
            applyCaptionColors();
            using RedrawWindowFunc = BOOL (WINAPI *)(HWND, const RECT *, HRGN, UINT);
            auto redrawWindow = reinterpret_cast<RedrawWindowFunc>(GetProcAddress(user32, "RedrawWindow"));
            if (redrawWindow) {
                redrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
            }
            FreeLibrary(user32);
        }
    }
    FreeLibrary(module);
#else
    Q_UNUSED(widget)
    Q_UNUSED(dark)
    Q_UNUSED(forceNativeFrameRefresh)
#endif
}

static void setWindowsDarkTitleBars(bool dark, bool forceNativeFrameRefresh = false)
{
    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        setWindowsDarkTitleBar(widget, dark, forceNativeFrameRefresh);
    }
}

static void scheduleWindowsDarkTitleBars(bool dark)
{
    setWindowsDarkTitleBars(dark, true);
    foreach (int delay, QList<int>() << 0 << 100 << 300 << 700) {
        const bool forceNativeFrameRefresh = delay <= 100;
        QTimer::singleShot(delay, qApp, [dark, forceNativeFrameRefresh]() {
            setWindowsDarkTitleBars(dark, forceNativeFrameRefresh);
        });
    }
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    init_core();
    init_gui();
    init_languages();
    init_devices();
    init_slots();
#ifdef Q_OS_WIN
    qApp->installEventFilter(this);
#endif

    apk_close();

    Settings::init();
    settings_load();

    if (actAutoUpdate->isChecked()) {
        // Update check is delayed in order to handle uninitialized GUI occuring with some firewalls.
        QTimer *delay = new QTimer(this);
        connect(delay, SIGNAL(timeout()), updater, SLOT(check()));
        connect(delay, SIGNAL(timeout()), delay, SLOT(deleteLater()));
        delay->setSingleShot(true);
        delay->start(1200);
    }
}

void MainWindow::checkReqs()
{
    QtConcurrent::run([=]() {
        resetApktool();

        QString version_jre;
        QString version_jdk;
        QString version_apktool = Apk::getApktoolVersion();
        const QString JRE = Apk::getJreVersion();
        const QString JDK = Apk::getJdkVersion();

        QRegularExpression rx;
        QRegularExpressionMatch match;

        rx.setPattern("version \"(.+)\"");
        match = rx.match(JRE);
        if (match.hasMatch()) {
            version_jre = match.captured(1);
            Settings::set_java_version(version_jre);
            toolDialog->reset();
        }

        rx.setPattern("javac (.+)");
        match = rx.match(JDK);
        if (match.hasMatch()) {
            version_jdk = match.captured(1);
        }

        qDebug() << "JRE version:" << qPrintable(!JRE.isNull() ? version_jre : "---");
        qDebug() << "JDK version:" << qPrintable(!JDK.isNull() ? version_jdk : "---");
        qDebug() << "Apktool version:" << qPrintable(!version_apktool.isNull() ? version_apktool : "---") << '\n';

        if (!JRE.isNull()) qDebug().nospace() << "java -version\n" << qPrintable(JRE) << '\n';
        if (!JDK.isNull()) qDebug().nospace() << "javac -version\n" << qPrintable(JDK) << '\n';

        emit reqsChecked(version_jre, version_jdk, version_apktool);
    });
}

void MainWindow::init_core()
{
    apk = NULL;
    apkManager = new ApkManager(this);
    updater = new Updater(this);
    recent = NULL;
    manualUpdateCheck = false;
    framelessResizeCursorActive = false;
    windowsThemePrimed = false;
    windowsInitialShowHandled = false;
    windowsSkipInitialActivationRefresh = false;
    accentBorderOverlay = NULL;
    customTitleBar = NULL;
    mainMenuBar = NULL;

    dropbox = new Dropbox(this);
    gdrive = new GoogleDrive(this);
    onedrive = new OneDrive(this);
    dropbox->setIcon(QPixmap(":/gfx/clouds/dropbox.png"));
    gdrive->setIcon(QPixmap(":/gfx/clouds/gdrive.png"));
    onedrive->setIcon(QPixmap(":/gfx/clouds/onedrive.png"));
}

void MainWindow::init_gui()
{
    qDebug() << "Screen DPI:" << Gui::Screen::dpi();

    // Dialogs:

    effects = new EffectsDialog(this);
    toolDialog = new ToolDialog(this);
    keyManager = new KeyManager(this);
    about = new About(this);

    // Main Window:

#ifdef Q_OS_WIN
    setWindowFlag(Qt::FramelessWindowHint, true);
    setMouseTracking(true);
#endif
    splitter = new QSplitter(this);
    setCentralWidget(splitter);
    setAcceptDrops(true);

#ifdef Q_OS_WIN
    accentBorderOverlay = new AccentBorderOverlay(this);

    customTitleBar = new FramelessTitleBar(this, this);
    mainMenuBar = new QMenuBar(this);
    QWidget *topBar = new QWidget(this);
    QVBoxLayout *topBarLayout = new QVBoxLayout(topBar);
    topBarLayout->setContentsMargins(0, 0, 0, 0);
    topBarLayout->setSpacing(0);
    topBarLayout->addWidget(customTitleBar);
    topBarLayout->addWidget(mainMenuBar);
    setMenuWidget(topBar);
#else
    mainMenuBar = new QMenuBar(this);
    setMenuBar(mainMenuBar);
#endif

    QMenuBar *menu = mainMenuBar;
    menuFile = new QMenu(this);
    menuIcon = new QMenu(this);
    menuView = new QMenu(this);
    menuAdaptivePreviewMode = new QMenu(this);
    menuPreviewShape = new QMenu(this);
    menuSett = new QMenu(this);
    menuHelp = new QMenu(this);
    btnDonate = new QToolButton(this);
    btnDonate->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btnDonate->setIcon(QPixmap(":/gfx/actions/donate.png"));
    menu->addMenu(menuFile);
    menu->addMenu(menuIcon);
    menu->addMenu(menuView);
    menu->addMenu(menuSett);
    menu->addMenu(menuHelp);
    menu->setCornerWidget(btnDonate);
    connect(btnDonate, SIGNAL(clicked()), this, SLOT(donate()));

    actApkOpen = new QAction(this);
    actApkSave = new QAction(this);
    menuRecent = new QMenu(this);
    actApkExplore = new QAction(this);
    actApkClose = new QAction(this);
    actExit = new QAction(this);
    actRecentClear = new QAction(this);
    actNoRecent = new QAction(this);
    iconActions = new QActionGroup(this);
    actIconOpen = new QAction(iconActions);
    actIconSave = new QAction(iconActions);
    menuIconAdd = new QMenu(this);
    actIconRemove = new QAction(iconActions);
    actIconScale = new QAction(iconActions);
    actIconResize = new QAction(iconActions);
    actIconRevert = new QAction(iconActions);
    actIconEffect = new QAction(iconActions);
    actIconClone = new QAction(iconActions);
    actIconBackground = new QAction(this);
    actViewActivities = new QAction(this);
    actViewActivities->setCheckable(true);
    adaptivePreviewModeActions = new QActionGroup(this);
    adaptivePreviewModeActions->setExclusive(true);
    actAdaptivePreviewNormal = new QAction(adaptivePreviewModeActions);
    actAdaptivePreviewThemed = new QAction(adaptivePreviewModeActions);
    foreach (QAction *action, adaptivePreviewModeActions->actions()) {
        action->setCheckable(true);
        menuAdaptivePreviewMode->addAction(action);
    }
    actAdaptivePreviewNormal->setChecked(true);
    previewShapeActions = new QActionGroup(this);
    previewShapeActions->setExclusive(true);
    actPreviewShapeNone = new QAction(previewShapeActions);
    actPreviewShapeCircle = new QAction(previewShapeActions);
    actPreviewShapeRoundedSquare = new QAction(previewShapeActions);
    actPreviewShapeSquircle = new QAction(previewShapeActions);
    foreach (QAction *action, previewShapeActions->actions()) {
        action->setCheckable(true);
        menuPreviewShape->addAction(action);
    }
    actPreviewShapeNone->setChecked(true);
    actPacking = new QAction(this);
    actKeys = new QAction(this);
    menuLang = new QMenu(this);
    menuTheme = new QMenu(this);
    themeActions = new QActionGroup(this);
    themeActions->setExclusive(true);
    actThemeSystem = new QAction(themeActions);
    actThemeLight = new QAction(themeActions);
    actThemeDark = new QAction(themeActions);
    foreach (QAction *action, themeActions->actions()) {
        action->setCheckable(true);
        menuTheme->addAction(action);
    }
    actThemeSystem->setChecked(true);
    actAutoUpdate = new QAction(this);
    actAssoc = new QAction(this);
    actReset = new QAction(this);
    actFaq = new QAction(this);
    actWebsite = new QAction(this);
    actReport = new QAction(this);
    actDonate = new QAction(this);
    menuLogs = new QMenu(this);
    actLogFile = new QAction(this);
    actLogPath = new QAction(this);
    actUpdate = new QAction(this);
    actAboutQt = new QAction(this);
    actAbout = new QAction(this);

    menuFile->addAction(actApkOpen);
    menuFile->addMenu(menuRecent);
    menuFile->addSeparator();
    menuFile->addAction(actApkExplore);
    menuFile->addSeparator();
    menuFile->addAction(actApkSave);
    menuFile->addSeparator();
    menuFile->addAction(actApkClose);
    menuFile->addSeparator();
    menuFile->addAction(actExit);
    menuIcon->addActions(iconActions->actions());
    menuIcon->addSeparator();
    menuIcon->addMenu(menuIconAdd);
    menuIcon->addSeparator();
    menuIcon->addAction(actIconBackground);
    menuView->addAction(actViewActivities);
    menuView->addMenu(menuAdaptivePreviewMode);
    menuView->addMenu(menuPreviewShape);
    menuSett->addAction(actPacking);
    menuSett->addAction(actKeys);
    menuSett->addSeparator();
    menuSett->addMenu(menuLang);
#ifdef Q_OS_WIN
    menuSett->addMenu(menuTheme);
#endif
    menuSett->addAction(actAutoUpdate);
    menuSett->addSeparator();
#ifndef Q_OS_UNIX
    menuSett->addAction(actAssoc);
#endif
    menuSett->addAction(actReset);
    menuHelp->addAction(actFaq);
    menuHelp->addSeparator();
    menuHelp->addAction(actWebsite);
    menuHelp->addAction(actReport);
    menuHelp->addAction(actDonate);
    menuHelp->addSeparator();
    menuHelp->addMenu(menuLogs);
    menuLogs->addAction(actLogFile);
    menuLogs->addAction(actLogPath);
    menuHelp->addSeparator();
    menuHelp->addAction(actUpdate);
    menuHelp->addSeparator();
    menuHelp->addAction(actAboutQt);
    menuHelp->addAction(actAbout);
    actNoRecent->setEnabled(false);
    actAutoUpdate->setCheckable(true);

    actApkOpen->setShortcut(QKeySequence::Open);
    actApkExplore->setShortcut(QKeySequence("Ctrl+D"));
    actApkSave->setShortcut(QKeySequence("Ctrl+E"));
    actIconOpen->setShortcut(QKeySequence("Ctrl+R"));
    actIconSave->setShortcut(QKeySequence::Save);
    actIconRemove->setShortcut(QKeySequence::Delete);
    actIconScale->setShortcut(QKeySequence("Ctrl+W"));
    actIconResize->setShortcut(QKeySequence("Ctrl+I"));
    actIconRevert->setShortcut(QKeySequence::Undo);
    actIconEffect->setShortcut(QKeySequence("Ctrl+F"));
    actIconClone->setShortcut(QKeySequence("Ctrl+C"));
    actPacking->setShortcut(QKeySequence("Ctrl+P"));
    actKeys->setShortcut(QKeySequence("Ctrl+K"));
    actFaq->setShortcut(QKeySequence::HelpContents);
    actLogPath->setShortcut(QKeySequence("Ctrl+L"));
    actExit->setShortcut(QKeySequence("Ctrl+Q"));

    actApkOpen->setIcon(QIcon(":/gfx/actions/open.png"));
    menuRecent->setIcon(QIcon(":/gfx/actions/open-list.png"));
    actRecentClear->setIcon(QIcon(":/gfx/actions/close.png"));
    actApkExplore->setIcon(QIcon(":/gfx/actions/explore.png"));
    actApkSave->setIcon(QIcon(":/gfx/actions/pack.png"));
    actApkClose->setIcon(QIcon(":/gfx/actions/remove.png"));
    actIconOpen->setIcon(QIcon(":/gfx/actions/open-icon.png"));
    actIconSave->setIcon(QIcon(":/gfx/actions/save.png"));
    actIconRemove->setIcon(QIcon(":/gfx/actions/remove.png"));
    actIconScale->setIcon(QIcon(":/gfx/actions/scale.png"));
    actIconResize->setIcon(QIcon(":/gfx/actions/resize.png"));
    actIconRevert->setIcon(QIcon(":/gfx/actions/undo.png"));
    actIconEffect->setIcon(QIcon(":/gfx/actions/effects.png"));
    actIconClone->setIcon(QIcon((":/gfx/actions/copy-icon.png")));
    actPacking->setIcon(QIcon(":/gfx/actions/box.png"));
    actKeys->setIcon(QIcon(":/gfx/actions/key.png"));
    actAutoUpdate->setIcon(QIcon(":/gfx/actions/update.png"));
    actAssoc->setIcon(QIcon(":/gfx/actions/associate.png"));
    actReset->setIcon(QIcon(":/gfx/actions/reset.png"));
    actFaq->setIcon(QIcon(":/gfx/actions/help.png"));
    actWebsite->setIcon(QIcon(":/gfx/actions/world.png"));
    actReport->setIcon(QIcon(":/gfx/actions/bug.png"));
    actDonate->setIcon(QIcon(":/gfx/actions/donate.png"));
    menuLogs->setIcon(QIcon(":/gfx/actions/file.png"));
    actLogFile->setIcon(QIcon(":/gfx/actions/file.png"));
    actLogPath->setIcon(QIcon(":/gfx/actions/open-list.png"));
    actUpdate->setIcon(QIcon(":/gfx/actions/update.png"));
    actAbout->setIcon(QIcon(":/gfx/actions/logo.png"));
    actAboutQt->setIcon(QIcon(":/gfx/actions/qt.png"));
    actExit->setIcon(QIcon(":/gfx/actions/close.png"));

    drawArea = new DrawArea(this);
    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    drawArea->setContextMenuPolicy(Qt::ActionsContextMenu);
    drawArea->addAction(actApkOpen);
    drawArea->addAction(separator);
    drawArea->addActions(menuIcon->actions());

    QPushButton *btnStudio = new QPushButton(this);
    btnStudio->setText("APK Editor Studio");
    btnStudio->setIcon(QPixmap(":/gfx/apk-editor-studio.png"));
    btnStudio->setFixedHeight(32);
    btnStudio->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    connect(btnStudio, &QPushButton::clicked, []() {
        QDesktopServices::openUrl(QUrl("https://qwertycube.com/apk-editor-studio/#utm_source=apk-icon-editor&utm_medium=application"));
    });

    loadingDialog = new ProgressDialog(this);
    loadingDialog->setIcon(QPixmap(":/gfx/actions/box.png"));

    uploadDialog = new ProgressDialog(this);

    tabIcons = new QWidget(this);
    QHBoxLayout *layoutDevices = new QHBoxLayout;
    QVBoxLayout *layoutIcons = new QVBoxLayout(tabIcons);
    devicesLabel = new QLabel(this);
    devices = new IconSizePresetComboBox(this);
    devices->setObjectName("iconSizePresetCombo");
    devices->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layoutDevices->addWidget(devicesLabel);
    layoutDevices->addWidget(devices);

    listIcons = new QListView(this);
    listIcons->setObjectName("iconsList");
    listIcons->addActions(menuIcon->actions());
    listIcons->setContextMenuPolicy(Qt::ActionsContextMenu);
    listIcons->setAlternatingRowColors(false);
    listIcons->viewport()->setAutoFillBackground(true);
    listIcons->setItemDelegate(new DecorationDelegate(QSize(32, 32), this));
    iconsProxy = new IconsProxy(this);
    listIcons->setModel(iconsProxy);
    setCurrentIcon(QModelIndex());

    QWidget *iconsLoading = new QWidget(this);
    QVBoxLayout *layoutIconsLoading = new QVBoxLayout(iconsLoading);
    layoutIconsLoading->addStretch();
    iconsLoadingIndicator = new BusyIndicator(this);
    iconsLoadingLabel = new QLabel(this);
    iconsLoadingLabel->setAlignment(Qt::AlignCenter);
    layoutIconsLoading->addWidget(iconsLoadingIndicator, 0, Qt::AlignHCenter);
    layoutIconsLoading->addWidget(iconsLoadingLabel);
    layoutIconsLoading->addStretch();

    iconsStack = new QStackedWidget(this);
    iconsStack->addWidget(listIcons);
    iconsStack->addWidget(iconsLoading);

    actAddIconLdpi = new QAction(this);
    actAddIconMdpi = new QAction(this);
    actAddIconHdpi = new QAction(this);
    actAddIconTvdpi = new QAction(this);
    actAddIconXhdpi = new QAction(this);
    actAddIconXxhdpi = new QAction(this);
    actAddIconXxxhdpi = new QAction(this);
    actAddIconTv = new QAction(this);
    actAddIconTv->setIcon(QIcon(":/gfx/dpi/tv.png"));
    menuIconAdd->setIcon(QIcon(":/gfx/actions/add.png"));
    menuIconAdd->addAction(actAddIconLdpi);
    menuIconAdd->addAction(actAddIconMdpi);
    menuIconAdd->addAction(actAddIconHdpi);
    menuIconAdd->addAction(actAddIconTvdpi);
    menuIconAdd->addAction(actAddIconXhdpi);
    menuIconAdd->addAction(actAddIconXxhdpi);
    menuIconAdd->addAction(actAddIconXxxhdpi);
    menuIconAdd->addAction(actAddIconTv);
    connect(menuIconAdd, &QMenu::aboutToShow, [this]() {
        const int row = devices->currentIndex();
        Device *device = static_cast<Device *>(devices->model()->index(row, 0).internalPointer());
        actAddIconLdpi->setText(device->getIconTitle(Icon(QString(), Icon::Ldpi)));
        actAddIconMdpi->setText(device->getIconTitle(Icon(QString(), Icon::Mdpi)));
        actAddIconHdpi->setText(device->getIconTitle(Icon(QString(), Icon::Hdpi)));
        actAddIconTvdpi->setText(device->getIconTitle(Icon(QString(), Icon::Tvdpi)));
        actAddIconXhdpi->setText(device->getIconTitle(Icon(QString(), Icon::Xhdpi)));
        actAddIconXxhdpi->setText(device->getIconTitle(Icon(QString(), Icon::Xxhdpi)));
        actAddIconXxxhdpi->setText(device->getIconTitle(Icon(QString(), Icon::Xxxhdpi)));
        actAddIconTv->setText(device->getIconTitle(Icon(QString(), Icon::TvBanner)));
    });
    btnAddIcon = new QToolButton(this);
    btnAddIcon->setShortcut(QKeySequence("+"));
    btnAddIcon->setIcon(QIcon(":/gfx/actions/add.png"));
    btnAddIcon->setMenu(menuIconAdd);
    btnAddIcon->setPopupMode(QToolButton::InstantPopup);
    btnAddIcon->setStyleSheet("QToolButton::menu-indicator { image: none; width: 0; }");
    btnRemoveIcon = new QToolButton(this);
    btnOpenIcon = new QToolButton(this);
    btnSaveIcon = new QToolButton(this);
    btnScaleIcon = new QToolButton(this);
    btnResizeIcon = new QToolButton(this);
    btnRevertIcon = new QToolButton(this);
    btnEffectIcon = new QToolButton(this);
    btnCloneIcons = new QToolButton(this);
    btnRemoveIcon->setDefaultAction(actIconRemove);
    btnOpenIcon->setDefaultAction(actIconOpen);
    btnSaveIcon->setDefaultAction(actIconSave);
    btnScaleIcon->setDefaultAction(actIconScale);
    btnResizeIcon->setDefaultAction(actIconResize);
    btnRevertIcon->setDefaultAction(actIconRevert);
    btnEffectIcon->setDefaultAction(actIconEffect);
    btnCloneIcons->setDefaultAction(actIconClone);
    btnAddIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnRemoveIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnOpenIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnSaveIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnScaleIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnResizeIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnRevertIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnEffectIcon->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    btnCloneIcons->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    QHBoxLayout *layoutIconsButtons = new QHBoxLayout;
    layoutIconsButtons->addWidget(btnAddIcon);
    layoutIconsButtons->addWidget(btnRemoveIcon);
    layoutIconsButtons->addWidget(btnOpenIcon);
    layoutIconsButtons->addWidget(btnSaveIcon);
    layoutIconsButtons->addWidget(btnScaleIcon);
    layoutIconsButtons->addWidget(btnResizeIcon);
    layoutIconsButtons->addWidget(btnRevertIcon);
    layoutIconsButtons->addWidget(btnEffectIcon);
    layoutIconsButtons->addWidget(btnCloneIcons);
    layoutIconsButtons->setSpacing(2);

    layoutIcons->addLayout(layoutDevices);
    layoutIcons->addWidget(iconsStack);
    layoutIcons->addLayout(layoutIconsButtons);
    layoutIcons->setContentsMargins(4, 4, 4, 4);
    layoutIcons->setSpacing(6);

    tabTranslations = new QWidget(this);
    QVBoxLayout *layoutTranslations = new QVBoxLayout(tabTranslations);
    tableTitles = new QTableView(this);
    tableTitles->verticalHeader()->setVisible(false);
    tableTitles->setSelectionMode(QTableView::SingleSelection);
    tableTitles->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableTitles->setHorizontalScrollMode(QTableView::ScrollPerPixel);
    tableTitles->setVerticalScrollMode(QTableView::ScrollPerPixel);
    btnApplyAppName = new QPushButton(this);
    layoutTranslations->addWidget(tableTitles);
    layoutTranslations->addWidget(btnApplyAppName);
    layoutTranslations->setContentsMargins(4, 4, 4, 4);

    tabProperties = new QWidget(this);
    QVBoxLayout *layoutProperties = new QVBoxLayout(tabProperties);
    tableManifest = new QTableView(this);
    tableManifest->horizontalHeader()->hide();
    tableManifest->horizontalHeader()->setStretchLastSection(true);
    tableManifest->setSelectionMode(QTableView::SingleSelection);
    tableManifest->setHorizontalScrollMode(QTableView::ScrollPerPixel);
    tableManifest->setVerticalScrollMode(QTableView::ScrollPerPixel);
    layoutProperties->addWidget(tableManifest);
    layoutProperties->setContentsMargins(4, 4, 4, 4);

    tabs = new QTabWidget(this);
    tabs->addTab(tabIcons, NULL);
    tabs->addTab(tabTranslations, NULL);
    tabs->addTab(tabProperties, NULL);

    checkDropbox = new QCheckBox(this);
    checkDropbox->setIcon(dropbox->getIcon());
    checkGDrive = new QCheckBox(this);
    checkGDrive->setIcon(gdrive->getIcon());
    checkOneDrive = new QCheckBox(this);
    checkOneDrive->setIcon(onedrive->getIcon());
    checkUpload = new QCheckBox(this);
    checkUpload->setChecked(true);
    checkUpload->setIcon(QPixmap(":/gfx/clouds/upload.png"));
    btnPack = new QPushButton(this);
    btnPack->setFixedHeight(64);

    QWidget *sidebar = new QWidget(this);
    QVBoxLayout *layoutSide = new QVBoxLayout(sidebar);
    layoutSide->setContentsMargins(0, 0, 0, 0);
    layoutSide->addWidget(tabs);
    layoutSide->addWidget(checkDropbox);
    layoutSide->addWidget(checkGDrive);
    layoutSide->addWidget(checkOneDrive);
    layoutSide->addWidget(checkUpload);
    layoutSide->addWidget(btnStudio);
    layoutSide->addWidget(btnPack);

    splitter->addWidget(drawArea);
    splitter->addWidget(sidebar);
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);
    splitter->setStyleSheet("QSplitter {padding: 8px;}");

    setInitialSize();
    updateAccentBorderOverlay();
}

void MainWindow::init_languages()
{
    translator = new QTranslator(this);
    translatorQt = new QTranslator(this);

    // Add default English:

    QStringList langs;
    langs << "apk-icon-editor.en";

    // Load language list:

    const QDir LANGPATH(Path::Data::shared() + "lang");
    langs << LANGPATH.entryList(QStringList() << "apk-icon-editor.*");

    for (int i = 0; i < langs.size(); ++i) {

        // Get native language title:

        QString locale = langs[i].split('.')[1];
        QString title = QLocale(locale).nativeLanguageName();
        if (title.size() > 1) {
            title[0] = title[0].toUpper();
        }

        // Set up menu action:

        QAction *actLang = new QAction(this);
        actLang->setText(title);
        actLang->setIcon(QIcon(QString("%1/flag.%2.png").arg(LANGPATH.absolutePath(), locale)));
        connect(actLang, &QAction::triggered, [=]() { setLanguage(locale); });
        menuLang->addAction(actLang);
    }

    // Add "Help Translate" action:

    menuLang->addSeparator();
    actTranslate = new QAction(this);
    actTranslate->setIcon(QPixmap(":/gfx/actions/world.png"));
    connect(actTranslate, SIGNAL(triggered()), this, SLOT(browseTranslate()));
    menuLang->addAction(actTranslate);
}

void MainWindow::init_devices()
{
    devices->setModel(&deviceModel);
    Device *firstDevice = static_cast<Device *>(deviceModel.index(0, 0).internalPointer());
    iconsProxy->setDevice(firstDevice);
}

void MainWindow::init_slots()
{
    connect(drawArea, SIGNAL(clicked()), this, SLOT(apk_open()));
    connect(checkUpload, SIGNAL(toggled(bool)), this, SLOT(enableUpload(bool)));
    connect(actApkOpen, SIGNAL(triggered()), this, SLOT(apk_open()));
    connect(actApkSave, SIGNAL(triggered()), this, SLOT(apk_save()));
    connect(actApkExplore, SIGNAL(triggered()), this, SLOT(apk_explore()));
    connect(actApkClose, SIGNAL(triggered()), this, SLOT(apk_close()));
    connect(actExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(actRecentClear, SIGNAL(triggered()), this, SLOT(recent_clear()));
    connect(actIconOpen, SIGNAL(triggered()), this, SLOT(icon_open()));
    connect(actIconSave, SIGNAL(triggered()), this, SLOT(icon_save()));
    connect(actIconRemove, SIGNAL(triggered()), this, SLOT(removeIcon()));
    connect(actIconScale, SIGNAL(triggered()), this, SLOT(icon_scale()));
    connect(actIconResize, SIGNAL(triggered()), this, SLOT(icon_resize()));
    connect(actIconRevert, SIGNAL(triggered()), this, SLOT(icon_revert()));
    connect(actIconEffect, SIGNAL(triggered()), this, SLOT(showEffectsDialog()));
    connect(actIconClone, SIGNAL(triggered()), this, SLOT(cloneIcons()));
    connect(actIconBackground, SIGNAL(triggered()), this, SLOT(setPreviewColor()));
    connect(actViewActivities, &QAction::toggled, iconsProxy, &IconsProxy::setShowActivities);
    connect(actAdaptivePreviewNormal, &QAction::triggered, [=]() { drawArea->setAdaptivePreviewMode(DrawArea::AdaptivePreviewNormal); });
    connect(actAdaptivePreviewThemed, &QAction::triggered, [=]() { drawArea->setAdaptivePreviewMode(DrawArea::AdaptivePreviewThemed); });
    connect(actPreviewShapeNone, &QAction::triggered, [=]() { drawArea->setPreviewShape(DrawArea::PreviewShapeNone); });
    connect(actPreviewShapeCircle, &QAction::triggered, [=]() { drawArea->setPreviewShape(DrawArea::PreviewShapeCircle); });
    connect(actPreviewShapeRoundedSquare, &QAction::triggered, [=]() { drawArea->setPreviewShape(DrawArea::PreviewShapeRoundedSquare); });
    connect(actPreviewShapeSquircle, &QAction::triggered, [=]() { drawArea->setPreviewShape(DrawArea::PreviewShapeSquircle); });
    connect(actPacking, SIGNAL(triggered()), toolDialog, SLOT(open()));
    connect(actKeys, SIGNAL(triggered()), keyManager, SLOT(open()));
    connect(actThemeSystem, &QAction::triggered, [=]() { setTheme(THEME_SYSTEM); });
    connect(actThemeLight, &QAction::triggered, [=]() { setTheme(THEME_LIGHT); });
    connect(actThemeDark, &QAction::triggered, [=]() { setTheme(THEME_DARK); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, [=]() {
        if (currentTheme == THEME_SYSTEM) {
            applyTheme(currentTheme);
        }
    });
#endif
    connect(actAssoc, SIGNAL(triggered()), this, SLOT(associate()));
    connect(actReset, SIGNAL(triggered()), this, SLOT(settings_reset()));
    connect(actWebsite, SIGNAL(triggered()), this, SLOT(browseSite()));
    connect(actReport, SIGNAL(triggered()), this, SLOT(browseBugs()));
    connect(actDonate, SIGNAL(triggered()), this, SLOT(donate()));
    connect(actFaq, SIGNAL(triggered()), this, SLOT(browseFaq()));
    connect(actLogFile, SIGNAL(triggered()), this, SLOT(openLogFile()));
    connect(actLogPath, SIGNAL(triggered()), this, SLOT(openLogPath()));
    connect(actUpdate, SIGNAL(triggered()), this, SLOT(checkUpdates()));
    connect(actAbout, SIGNAL(triggered()), about, SLOT(exec()));
    connect(actAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(btnPack, SIGNAL(clicked()), this, SLOT(apk_save()));
    connect(actAddIconLdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Ldpi); });
    connect(actAddIconMdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Mdpi); });
    connect(actAddIconHdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Hdpi); });
    connect(actAddIconTvdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Tvdpi); });
    connect(actAddIconXhdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Xhdpi); });
    connect(actAddIconXxhdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Xxhdpi); });
    connect(actAddIconXxxhdpi, &QAction::triggered, [=]() { apk->addIcon(Icon::Xxxhdpi); });
    connect(actAddIconTv, &QAction::triggered, [=]() { apk->addIcon(Icon::TvBanner); });
    void (QComboBox::*devicesIndexChanged)(int row) = &QComboBox::currentIndexChanged;
    connect(devices, devicesIndexChanged, [=](int row) {
        Device *device = static_cast<Device *>(devices->model()->index(row, 0).internalPointer());
        iconsProxy->setDevice(device);
        setCurrentIcon(listIcons->currentIndex());
    });
    connect(iconsProxy, &IconsProxy::dataChanged, [=]() { setCurrentIcon(listIcons->currentIndex()); });
    connect(iconsProxy, &IconsProxy::rowsInserted, [=]() { setCurrentIcon(listIcons->currentIndex()); });
    connect(iconsProxy, &IconsProxy::rowsRemoved, [=]() { setCurrentIcon(listIcons->currentIndex()); });
    connect(listIcons->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::setCurrentIcon);
    connect(btnApplyAppName, SIGNAL(clicked()), this, SLOT(applyAppName()));
    connect(apkManager, SIGNAL(loading(short, QString)), loadingDialog, SLOT(setProgress(short, QString)));
    connect(apkManager, SIGNAL(error(QString, QString, QString)), this, SLOT(error(QString, QString, QString)));
    connect(apkManager, SIGNAL(error(QString, QString, QString)), loadingDialog, SLOT(accept()));
    connect(apkManager, SIGNAL(packed(Apk::File*, bool, QString, QString)), this, SLOT(apk_packed(Apk::File*, bool, QString, QString)));
    connect(apkManager, SIGNAL(unpacked(Apk::File*)), this, SLOT(apk_unpacked(Apk::File*)));
    connect(loadingDialog, SIGNAL(rejected()), apkManager, SLOT(cancel()));
    connect(toolDialog, &ToolDialog::accepted, [=]() { checkReqs(); });
    connect(keyManager, SIGNAL(success(QString, QString)), this, SLOT(success(QString, QString)));
    connect(keyManager, SIGNAL(warning(QString, QString)), this, SLOT(warning(QString, QString)));
    connect(keyManager, SIGNAL(error(QString, QString)), this, SLOT(error(QString, QString)));
    connect(dropbox, SIGNAL(auth_required()), this, SLOT(authCloud()));
    connect(dropbox, SIGNAL(progress(short, QString)), uploadDialog, SLOT(setProgress(short, QString)));
    connect(dropbox, SIGNAL(error(QString, QString)), this, SLOT(error(QString, QString)));
    connect(gdrive, SIGNAL(auth_required()), this, SLOT(authCloud()));
    connect(gdrive, SIGNAL(progress(short, QString)), uploadDialog, SLOT(setProgress(short, QString)));
    connect(gdrive, SIGNAL(error(QString, QString)), this, SLOT(error(QString, QString)));
    connect(onedrive, SIGNAL(auth_required()), this, SLOT(authCloud()));
    connect(onedrive, SIGNAL(progress(short, QString)), uploadDialog, SLOT(setProgress(short, QString)));
    connect(onedrive, SIGNAL(error(QString, QString)), this, SLOT(error(QString, QString)));
    connect(uploadDialog, SIGNAL(rejected()), dropbox, SLOT(cancel()));
    connect(uploadDialog, SIGNAL(rejected()), gdrive, SLOT(cancel()));
    connect(uploadDialog, SIGNAL(rejected()), onedrive, SLOT(cancel()));
    connect(updater, SIGNAL(version(QString)), this, SLOT(newVersion(QString)));
    connect(updater, SIGNAL(checked(QString,bool,QString)), this, SLOT(updateChecked(QString,bool,QString)));
    connect(this, SIGNAL(reqsChecked(QString, QString, QString)), about, SLOT(setVersions(QString, QString, QString)));
}

void MainWindow::settings_load()
{
    checkReqs();

    // Global:

    restoreGeometry(Settings::get_geometry());
    splitter->restoreState(Settings::get_splitter());
    setLanguage(Settings::get_language());
    setTheme(Settings::get_theme());
    currentPath = Settings::get_last_path();
    devices->setCurrentText(Settings::get_device());
    actAutoUpdate->setChecked(Settings::get_update());
    actViewActivities->setChecked(Settings::get_activities());

    // Recent List:

    if (recent) { delete recent; }
    recent = new Recent(Settings::get_recent());
    recent_update();

    // APK:

    toolDialog->reset();

    // Keys:

    keyManager->reset();

    // Cloud Services:

    checkUpload->setChecked(Settings::get_upload());
    checkDropbox->setChecked(Settings::get_dropbox());
    checkGDrive->setChecked(Settings::get_gdrive());
    checkOneDrive->setChecked(Settings::get_onedrive());
    dropbox->setToken(Settings::get_dropbox_token());
    gdrive->setToken(Settings::get_gdrive_token());
    onedrive->setToken(Settings::get_onedrive_token());
}

bool MainWindow::resetApktool()
{
    const QString FRAMEWORK = Settings::get_temp() + "/apk-icon-editor-reborn/framework/1.apk";
    return QFile::remove(FRAMEWORK);
}

void MainWindow::settings_reset()
{
    if (QMessageBox::question(this, tr("Reset?"), tr("Reset settings to default?")) == QMessageBox::Yes) {
        Settings::reset();
        settings_load();
        resetApktool();
        setInitialSize();
    }
}

void MainWindow::setInitialSize()
{
#ifndef Q_OS_OSX
    resize(Gui::Screen::scaled(800, 540));
#else
    resize(Gui::Screen::scaled(880, 540));
#endif
    splitter->setSizes(QList<int>() << Gui::Screen::scaled(492)
                                    << Gui::Screen::scaled(280));
}

void MainWindow::setLanguage(QString lang)
{
    qDebug() << "Language set to" << lang << "\n";
    const QString LANGPATH(Path::Data::shared() + "lang");
    QApplication::removeTranslator(translator);
    QApplication::removeTranslator(translatorQt);
    if (translator->load(QString("apk-icon-editor.%1").arg(lang), LANGPATH)) {
        translatorQt->load(QString("qt.%1").arg(lang), LANGPATH);
        QApplication::installTranslator(translator);
        QApplication::installTranslator(translatorQt);
    }
    else {
        lang = "en";
    }

    currentLang = lang;
    QPixmap flag;
    if (flag.load(QString("%1/flag.%2.png").arg(LANGPATH, lang)) == false) {
        lang = lang.left(2);
        flag = QString("%1/flag.%2.png").arg(LANGPATH, lang);
    }
    menuLang->setIcon(flag);

    // Retranslate strings:

    drawArea->setText(tr("CLICK HERE\n(or drag APK and icons)"));
    tabs->setTabText(0, tr("Icons"));
    tabs->setTabText(1, tr("Translations"));
    tabs->setTabText(2, tr("Properties")); // tr("Details")
    devicesLabel->setText(tr("Icon size preset:"));
    iconsLoadingLabel->setText(tr("Building icon list..."));
    btnApplyAppName->setText(tr("Apply to All"));
    checkDropbox->setText(tr("Upload to %1").arg(dropbox->getTitle()));
    checkGDrive->setText(tr("Upload to %1").arg(gdrive->getTitle()));
    checkOneDrive->setText(tr("Upload to %1").arg(onedrive->getTitle()));
    checkUpload->setText(tr("Enable Upload to Cloud Storages"));
    btnPack->setText(tr("Pack APK"));
    menuFile->setTitle(tr("&File"));
    menuIcon->setTitle(tr("&Icon"));
    menuView->setTitle(tr("&View"));
    menuSett->setTitle(tr("&Settings"));
    menuHelp->setTitle(tr("&Help"));
    actApkOpen->setText(tr("&Open APK"));
    actApkSave->setText(tr("&Export (Pack) APK"));
    menuRecent->setTitle(tr("&Recent APKs"));
    actApkExplore->setText(tr("Explore APK &Contents"));
    actApkClose->setText(tr("&Close APK"));
    actExit->setText(tr("E&xit"));
    actRecentClear->setText(tr("&Clear List"));
    actNoRecent->setText(tr("No Recent Files"));
    actIconOpen->setText(tr("Replace &Icon"));
    actIconSave->setText(tr("&Save Icon"));
    actIconRemove->setText(tr("Remove Icon"));
    actIconScale->setText(tr("Scale to &Fit"));
    actIconResize->setText(tr("&Resize Icon"));
    actIconRevert->setText(tr("Revert &Original"));
    actIconEffect->setText(tr("E&ffects"));
    actIconClone->setText(tr("Apply to All"));
    menuIconAdd->setTitle(tr("&Add Icon"));
    btnAddIcon->setToolTip(tr("&Add Icon").remove('&'));
    actViewActivities->setText("Android Activities");
    menuAdaptivePreviewMode->setTitle(tr("Adaptive Icon Preview Mode"));
    actAdaptivePreviewNormal->setText(tr("Normal"));
    actAdaptivePreviewThemed->setText(tr("Themed Monochrome"));
    menuPreviewShape->setTitle(tr("Adaptive Icon Preview Shape"));
    actPreviewShapeNone->setText(tr("No Mask"));
    actPreviewShapeCircle->setText(tr("Circle"));
    actPreviewShapeRoundedSquare->setText(tr("Rounded Square"));
    actPreviewShapeSquircle->setText(tr("Squircle"));
    actIconBackground->setText(tr("Preview Background &Color"));
    actPacking->setText(tr("&Repacking"));
    actKeys->setText(tr("Key Manager"));
    menuLang->setTitle(tr("&Language"));
    actTranslate->setText(tr("Help Translate"));
    menuTheme->setTitle(tr("&Theme"));
    actThemeSystem->setText(tr("System"));
    actThemeLight->setText(tr("Light"));
    actThemeDark->setText(tr("Dark"));
    actAutoUpdate->setText(tr("Auto-check for Updates"));
    actAssoc->setText(tr("Associate .APK"));
    actReset->setText(tr("Reset Settings"));
    actFaq->setText(tr("FAQ"));
    actWebsite->setText(tr("Visit Website"));
    actReport->setText(tr("Report a Bug"));
    actDonate->setText(tr("Donate"));
    menuLogs->setTitle(tr("Logs"));
    actLogFile->setText(tr("Open Log File"));
    actLogPath->setText(tr("Open Log Directory"));
    actUpdate->setText(tr("Check for &Updates"));
    actAboutQt->setText(tr("About Qt"));
    actAbout->setText(tr("About %1").arg(APP));
    btnDonate->setText(tr("Donate"));
    loadingDialog->setWindowTitle(tr("Processing"));
    uploadDialog->setWindowTitle(tr("Uploading"));
    mainMenuBar->resize(0, 0); // "Repaint" menu bar

    effects->retranslate();
    toolDialog->retranslate();
    keyManager->retranslate();
    about->retranslate();
}

void MainWindow::setTheme(QString theme)
{
#ifndef Q_OS_WIN
    Q_UNUSED(theme)
    theme = THEME_SYSTEM;
#endif
    if (theme != THEME_LIGHT && theme != THEME_DARK && theme != THEME_SYSTEM) {
        theme = THEME_SYSTEM;
    }

    currentTheme = theme;
    actThemeSystem->setChecked(theme == THEME_SYSTEM);
    actThemeLight->setChecked(theme == THEME_LIGHT);
    actThemeDark->setChecked(theme == THEME_DARK);
    Settings::set_theme(theme);
    applyTheme(theme);
    qDebug() << "Theme set to" << theme;
}

void MainWindow::applyTheme(QString theme)
{
    const QString styleName = defaultStyleName();
#ifdef Q_OS_WIN
    const QPalette nativePalette = defaultPalette();
    Q_UNUSED(nativePalette)
    const bool dark = theme == THEME_DARK || (theme == THEME_SYSTEM && isSystemDark());
    const bool initialThemeApply = !windowsThemePrimed;
    if (dark) {
        if (QStyle *style = QStyleFactory::create("Fusion")) {
            qApp->setStyle(new DarkProxyStyle(style));
        }
    } else if (!styleName.isEmpty()) {
        if (QStyle *style = QStyleFactory::create(styleName)) {
            qApp->setStyle(style);
        }
    }
    qApp->setPalette(dark ? darkPalette() : lightPalette());
    qApp->setStyleSheet(dark ? darkStyleSheet() : QString());
    if (listIcons) {
        QPalette listPalette = dark ? darkPalette() : lightPalette();
        listIcons->setPalette(listPalette);
        listIcons->viewport()->setPalette(listPalette);
        listIcons->viewport()->setAutoFillBackground(true);
        listIcons->viewport()->update();
    }
    if (devices) {
        devices->update();
    }
    if (customTitleBar) {
        customTitleBar->setProperty("darkTitleBar", dark);
        customTitleBar->setStyleSheet(framelessTitleBarStyleSheet(dark));
        for (QToolButton *button : customTitleBar->findChildren<QToolButton *>()) {
            button->update();
        }
        customTitleBar->update();
    }
    if (initialThemeApply) {
        setWindowsDarkTitleBars(dark);
    } else {
        scheduleWindowsDarkTitleBars(dark);
    }
    windowsThemePrimed = true;
    updateAccentBorderOverlay();
#else
    Q_UNUSED(theme)
    if (!styleName.isEmpty()) {
        if (QStyle *style = QStyleFactory::create(styleName)) {
            qApp->setStyle(style);
        }
    }
    qApp->setPalette(defaultPalette());
    qApp->setStyleSheet(QString());
    if (listIcons) {
        const QPalette nativePalette = defaultPalette();
        listIcons->setPalette(nativePalette);
        listIcons->viewport()->setPalette(nativePalette);
        listIcons->viewport()->setAutoFillBackground(true);
        listIcons->viewport()->update();
    }
    if (devices) {
        devices->update();
    }
#endif
    drawArea->syncPaletteBackground();
}

void MainWindow::recent_add(QString filename)
{
    recent->add(filename, apk->getThumbnail().pixmap(32, 32));
    recent_update();
}

void MainWindow::recent_update()
{
    menuRecent->clear();

    if (!recent->empty()) {
        for (int i = 0; i < recent->size(); ++i) {
            const RecentFile RECENT = recent->at(i);
            QAction *actRecent = new QAction(RECENT.filename, menuRecent);
            actRecent->setIcon(RECENT.icon);
            menuRecent->addAction(actRecent);
            connect(actRecent, &QAction::triggered, [=]() { apk_open(RECENT.filename); });
        }
        menuRecent->addSeparator();
        menuRecent->addAction(actRecentClear);
    }
    else {
        menuRecent->addAction(actNoRecent);
    }
}

void MainWindow::recent_clear()
{
    recent->clear();
    recent_update();
}

void MainWindow::cloneIcons()
{
    Icon *newIcon = drawArea->getIcon();
    if (newIcon) {
        if (QMessageBox::question(this, NULL, tr("Apply the current icon to all sizes?")) == QMessageBox::Yes) {
            apk->iconsModel.clone(newIcon);
            setWindowModified(true);
        }
    }
}

void MainWindow::applyAppName()
{
    QModelIndex index = tableTitles->currentIndex();
    if (index.isValid()) {
        if (QMessageBox::question(this, NULL, tr("Apply the current application name to all translations?")) == QMessageBox::Yes) {
            const QString title = index.sibling(index.row(), 0).data().toString();
            static_cast<TitlesModel *>(tableTitles->model())->applyToAll(title);
        }
    }
}

void MainWindow::setCurrentIcon(const QModelIndex &index)
{
    Icon *icon = nullptr;

    if (apk && index.isValid()) {

        icon = static_cast<Icon *>(iconsProxy->mapToSource(index).internalPointer());
        drawArea->setIcon(icon);
        if (icon && icon->isAdaptiveIcon()) {
            qDebug().noquote() << "Selected adaptive icon:\n" + icon->getToolTip();
        }

        const Device *device = static_cast<Device *>(devices->model()->index(devices->currentIndex(), 0).internalPointer());
        const QSize size = device->getIconSize(*icon).size;
        drawArea->setBounds(size.width(), size.height());

        disconnect(effects, 0, 0, 0);
        connect(effects, SIGNAL(colorActivated(bool)), icon,     SLOT(setColorize(bool)), Qt::DirectConnection);
        connect(effects, SIGNAL(rotate(int)),          icon,     SLOT(setAngle(int)),     Qt::DirectConnection);
        connect(effects, SIGNAL(flipX(bool)),          icon,     SLOT(setFlipX(bool)),    Qt::DirectConnection);
        connect(effects, SIGNAL(flipY(bool)),          icon,     SLOT(setFlipY(bool)),    Qt::DirectConnection);
        connect(effects, SIGNAL(colorize(QColor)),     icon,     SLOT(setColor(QColor)),  Qt::DirectConnection);
        connect(effects, SIGNAL(colorDepth(qreal)),    icon,     SLOT(setDepth(qreal)),   Qt::DirectConnection);
        connect(effects, SIGNAL(blur(qreal)),          icon,     SLOT(setBlur(qreal)),    Qt::DirectConnection);
        connect(effects, SIGNAL(round(qreal)),         icon,     SLOT(setCorners(qreal)), Qt::DirectConnection);
        connect(effects, SIGNAL(colorActivated(bool)), drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(blurActivated(bool)),  drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(rotate(int)),          drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(flipX(bool)),          drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(flipY(bool)),          drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(colorize(QColor)),     drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(colorDepth(qreal)),    drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(blur(qreal)),          drawArea, SLOT(repaint()));
        connect(effects, SIGNAL(round(qreal)),         drawArea, SLOT(repaint()));
    }

    drawArea->setIcon(icon);
    actIconRemove->setEnabled(icon);
    actIconOpen->setEnabled(icon);
    actIconSave->setEnabled(icon);
    actIconScale->setEnabled(icon);
    actIconResize->setEnabled(icon);
    actIconRevert->setEnabled(icon);
    actIconEffect->setEnabled(icon);
    actIconClone->setEnabled(icon);
}

void MainWindow::setActiveApk(QString filename)
{
    currentApk = filename;
    setWindowModified(false);
    setWindowTitle(QFileInfo(filename).fileName() + "[*]");
    recent_add(filename);
}

void MainWindow::setIconsLoading(bool loading)
{
    iconsStack->setCurrentWidget(loading ? iconsStack->widget(1) : listIcons);
    devices->setEnabled(!loading);
    if (loading) {
        menuIconAdd->setEnabled(false);
        btnAddIcon->setEnabled(false);
        setCurrentIcon(QModelIndex());
    }
}

void MainWindow::enableUpload(bool enable)
{
    checkDropbox->setVisible(enable);
    checkGDrive->setVisible(enable);
    checkOneDrive->setVisible(enable);
}

void MainWindow::upload(Cloud *uploader, QString filename)
{
    QEventLoop loop;
    connect(uploader, SIGNAL(finished(bool)), &loop, SLOT(quit()), Qt::QueuedConnection);
    uploadDialog->setIcon(uploader->getIcon());
    uploader->upload(filename);
    loop.exec(); // Block execution until cloud upload is finished.
}

void MainWindow::apk_packed(Apk::File *apk, bool isSuccess, QString text, QString details)
{
    loadingDialog->accept();
    const QString FILENAME = apk->getFilePath();
    setActiveApk(FILENAME);

    const bool UPLOAD_TO_DROPBOX  = checkDropbox->isChecked();
    const bool UPLOAD_TO_GDRIVE   = checkGDrive->isChecked();
    const bool UPLOAD_TO_ONEDRIVE = checkOneDrive->isChecked();

    if (isSuccess) {
        if (checkUpload->isChecked()) {
            if (UPLOAD_TO_DROPBOX)  upload(dropbox, FILENAME);
            if (UPLOAD_TO_GDRIVE)   upload(gdrive, FILENAME);
            if (UPLOAD_TO_ONEDRIVE) upload(onedrive, FILENAME);
        }
        uploadDialog->accept();
        success(NULL, text, details);
    }
    else {
        warning(NULL, text, details);
        if (checkUpload->isChecked()) {
            if (UPLOAD_TO_DROPBOX)  upload(dropbox, FILENAME);
            if (UPLOAD_TO_GDRIVE)   upload(gdrive, FILENAME);
            if (UPLOAD_TO_ONEDRIVE) upload(onedrive, FILENAME);
        }
        uploadDialog->accept();
    }
}

void MainWindow::apk_unpacked(Apk::File *apk)
{
    this->apk = apk;
    setActiveApk(apk->getFilePath());

    // Set models:

    iconsProxy->setSourceModel(&apk->iconsModel);
    tableManifest->setModel(&apk->manifestModel);
    tableTitles->setModel(&apk->titlesModel);
    tableTitles->resizeColumnsToContents();
    listIcons->setCurrentIndex(listIcons->model()->index(0, 0));
    setIconsLoading(false);
    connect(&apk->manifestModel, &ManifestModel::dataChanged, [=]() { setWindowModified(true); });
    connect(&apk->titlesModel, &TitlesModel::dataChanged, [=]() { setWindowModified(true); });

    // Enable operations with APK and icons:

    tabIcons->setEnabled(true);
    tabTranslations->setEnabled(true);
    tabProperties->setEnabled(true);
    actApkSave->setEnabled(true);
    actApkExplore->setEnabled(true);
    actApkClose->setEnabled(true);
    iconActions->setEnabled(true);
    menuIconAdd->setEnabled(true);
    btnAddIcon->setEnabled(true);
    btnPack->setEnabled(true);

    loadingDialog->accept();
    setWindowModified(false);
}

bool MainWindow::icon_open(QString filename)
{
    Icon *icon = drawArea->getIcon();
    if (!icon) {
        return false;
    }

    if (icon->isAdaptiveIcon() && icon->getAdaptiveDescriptor().usesReadOnlySplitResources) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(tr("Split APK Resources"));
        box.setText(tr("This adaptive icon uses resources from split APK files. Split APK resources are supported for preview only; replacing and repacking split APK sets is not supported yet."));
        box.setDetailedText(icon->getToolTip());
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
        qDebug().noquote() << "Adaptive icon replacement blocked: split APK resources are read-only.";
        return false;
    }

    if (icon->isXmlDrawableIcon()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("XML Drawable Icon"));
        box.setText(tr("This XML/vector launcher icon can be previewed and exported, but replacement is not supported yet."));
        box.setDetailedText(icon->getToolTip());
        box.setStandardButtons(QMessageBox::Ok);
        box.exec();
        qDebug().noquote() << "XML drawable icon replacement blocked: write-back is not supported yet.";
        return false;
    }

    if (filename.isEmpty()) {
        filename = QFileDialog::getOpenFileName(this, tr("Import Icon"), NULL, Image::Formats::openDialogFilter());
        if (filename.isEmpty()) {
            return false;
        }
    }

    QPixmap imported(filename);
    if (imported.isNull()) {
        warning(tr("Can't Load Icon"), tr("You are trying to load invalid or unsupported icon."));
        return false;
    }

    if (icon->isAdaptiveIcon()) {
        const QString text = tr("This is an adaptive XML icon. Replace Icon will update the foreground layer only; the background layer from the APK will be preserved.");
        const QString details = icon->getToolTip();
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("Adaptive Icon Replacement"));
        box.setText(text);
        box.setDetailedText(details);
        box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Ok);
        if (box.exec() != QMessageBox::Ok) {
            qDebug().noquote() << "Adaptive icon replacement cancelled:" << Path::display(icon->getAdaptiveXmlPath());
            return false;
        }
        qDebug().noquote() << "Adaptive icon replacement confirmed: foreground-only; background layer is preserved.";
    }

    QPixmap backup = icon->getPixmap();

    if (icon->replace(imported)) {
        repaint();
        const Device *device = static_cast<Device *>(devices->model()->index(devices->currentIndex(), 0).internalPointer());
        const QSize size = device->getIconSize(*icon).size;

        if (icon->width() != size.width() || icon->height() != size.height()) {
            int result = QMessageBox::warning(this, tr("Resize?"),
                                              tr("Icon you are trying to load is off-size.\nResize automatically?"),
                                              QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
            switch (result) {
            case QMessageBox::Yes:
                icon->resize(size);
                break;
            case QMessageBox::No:
                break;
            default:
                // Restore previous icon:
                icon->replace(backup);
                return false;
            }
        }
        apk->iconsModel.updateAdaptiveFamily(icon);
        setWindowModified(true);
        return true;
    }
    else {
        warning(tr("Can't Load Icon"), tr("You are trying to load invalid or unsupported icon."));
        return false;
    }
}

bool MainWindow::icon_save(QString filename)
{
    Icon *icon = drawArea->getIcon();
    if (!icon) {
        return false;
    }

    if (filename.isEmpty()) {
        const Device *device = static_cast<Device *>(devices->model()->index(devices->currentIndex(), 0).internalPointer());
        const QSize size = device->getIconSize(*icon).size;
        filename = QString("%1-%2x%3").arg(QFileInfo(currentApk).completeBaseName()).arg(size.width()).arg(size.height());
        filename = QFileDialog::getSaveFileName(this, tr("Save Icon"), filename, Image::Formats::saveDialogFilter());
        if (filename.isEmpty()) {
            return false;
        }
    }
    return icon->save(filename);
}

bool MainWindow::icon_scale()
{
    Icon *icon = drawArea->getIcon();
    if (!icon) {
        return false;
    }

    const Device *device = static_cast<Device *>(devices->model()->index(devices->currentIndex(), 0).internalPointer());
    const QSize size = device->getIconSize(*icon).size;
    return icon_resize(size);
}

bool MainWindow::icon_resize(QSize size)
{
    Icon *icon = drawArea->getIcon();
    if (!icon) {
        return false;
    }

    if (!size.isValid()) {
        const int WIDTH = icon->width();
        const int HEIGHT = icon->height();
        size = Dialogs::getSize(tr("Resize Icon"), WIDTH, HEIGHT, this);
        if (!size.isValid()) {
            return false;
        }
    }

    setWindowModified(true);
    bool result = icon->resize(size);
    drawArea->repaint();
    return result;
}

bool MainWindow::icon_revert()
{
    Icon *icon = drawArea->getIcon();
    if (!icon) {
        return false;
    }

    bool result = icon->revert();
    drawArea->repaint();
    return result;
}

bool MainWindow::setPreviewColor()
{
    const QColor DEFAULT = drawArea->palette().color(QPalette::Window);
    const QColor COLOR = QColorDialog::getColor(DEFAULT, this);
    if (COLOR.isValid()) {
        drawArea->setBackground(COLOR);
        QPixmap icon(32, 32);
        icon.fill(COLOR);
        actIconBackground->setIcon(QIcon(icon));
        return true;
    }
    else {
        return false;
    }
}

void MainWindow::showEffectsDialog()
{
    Icon *icon = drawArea->getIcon();
    if (icon) {
        const int TEMP_ANGLE = icon->getAngle();
        const bool TEMP_FLIP_X = icon->getFlipX();
        const bool TEMP_FLIP_Y = icon->getFlipY();
        const bool TEMP_IS_COLOR = icon->getColorEnabled();
        const QColor TEMP_COLOR = icon->getColor();
        const qreal TEMP_DEPTH = icon->getDepth();
        const qreal TEMP_BLUR = icon->getBlur();
        const qreal TEMP_ROUND = icon->getCorners();
        effects->setRotation(TEMP_ANGLE);
        effects->setFlipX(TEMP_FLIP_X);
        effects->setFlipY(TEMP_FLIP_Y);
        effects->setColorize(TEMP_IS_COLOR);
        effects->setColor(TEMP_COLOR);
        effects->setColorDepth(TEMP_DEPTH * 100);
        effects->setBlur(TEMP_BLUR * 10);
        effects->setCorners(TEMP_ROUND);
        if (effects->exec() == QDialog::Accepted) {
            setWindowModified(true);
        }
        else {
            icon->setAngle(TEMP_ANGLE);
            icon->setFlipX(TEMP_FLIP_X);
            icon->setFlipY(TEMP_FLIP_Y);
            icon->setColorize(TEMP_IS_COLOR);
            icon->setColor(TEMP_COLOR);
            icon->setDepth(TEMP_DEPTH);
            icon->setBlur(TEMP_BLUR);
            icon->setCorners(TEMP_ROUND);
        }
    }
}

bool MainWindow::apk_open(QString filename)
{
    if (confirmExit()) {
        return false;
    }

    // "Open" dialog:

    if (filename.isEmpty()) {
        filename = QFileDialog::getOpenFileName(this, tr("Open APK"), currentPath, "APK (*.apk);;" + tr("All Files"));
        if (filename.isEmpty()) {
            return false;
        }
    }

    // Open APK:

    if (!QFile::exists(filename)) {
        error(tr("File not found"), tr("Could not find APK:\n%1").arg(filename));
        recent->remove(filename);
        recent_update();
        return false;
    }

    currentPath = QFileInfo(filename).absolutePath();

    // Unpacking:

    loadingDialog->setProgress(20, QApplication::translate("Apk::Unpacker", "Unpacking APK..."));
    QApplication::processEvents();

    const QString destination = Settings::get_temp() + "/apk-icon-editor-reborn/";
    const QString apktool = Settings::get_apktool();
    const bool smali = Settings::get_smali();
    apk_close();
    setIconsLoading(true);
    tabIcons->setEnabled(true);
    apkManager->unpack(filename, destination, apktool, smali);

    return true;
}

bool MainWindow::apk_save(QString filename)
{
    // "Save" dialog:

    if (filename.isEmpty()) {
        filename = QFileDialog::getSaveFileName(this, tr("Pack APK"), currentPath, "APK (*.apk)");
        if (filename.isEmpty()) {
            return false;
        }
    }

    // Fetch KeyStore properties:

    QString alias = Settings::get_alias();
    QString pass_store = Settings::get_keystore_pass();
    QString pass_alias = Settings::get_alias_pass();

    const bool USING_KEYSTORE = Settings::get_use_keystore();
    if (USING_KEYSTORE) {
        const QPixmap PIXMAP_KEY(":/gfx/actions/key.png");
        if (pass_store.isEmpty()) {
            pass_store = Dialogs::getPassword(tr("Enter the KeyStore password:"), "", PIXMAP_KEY, this);
        }
        if (alias.isEmpty()) {
            alias = Dialogs::getString(tr("Enter the alias:"), "", PIXMAP_KEY, this);
        }
        if (pass_alias.isEmpty()) {
            pass_alias = Dialogs::getPassword(tr("Enter the alias password:"), "", PIXMAP_KEY, this);
        }
    }

    // Saving APK:

    currentPath = QFileInfo(filename).absolutePath();
    loadingDialog->setProgress(0);

    apk->setFilePath(filename);
    apk->setApktool(Settings::get_apktool());
    apk->setSmali(Settings::get_smali());
    apk->setSign(Settings::get_sign());
    apk->setZipalign(Settings::get_zipalign());
    apk->setApksigner(Settings::get_use_apksigner());
    apk->setFilePemPk8(Settings::get_pem(), Settings::get_pk8());
    apk->setFileKeystore(Settings::get_keystore(), alias, pass_store, pass_alias);
    apk->setKeystore(USING_KEYSTORE);

    apkManager->pack(apk, Settings::get_temp() + "/apk-icon-editor-reborn/");
    return true;
}

void MainWindow::apk_explore()
{
    const QString TEMPDIR = Settings::get_temp() + "/apk-icon-editor-reborn/apk";
    QDesktopServices::openUrl(QUrl::fromLocalFile(TEMPDIR));
}

void MainWindow::apk_close()
{
    apkManager->close();

    setWindowTitle(QString());
    setWindowModified(false);
    setCurrentIcon(QModelIndex());

    iconsProxy->setSourceModel(NULL);
    tableManifest->setModel(NULL);
    tableTitles->setModel(NULL);

    tabIcons->setEnabled(false);
    tabTranslations->setEnabled(false);
    tabProperties->setEnabled(false);
    actApkSave->setEnabled(false);
    actApkExplore->setEnabled(false);
    actApkClose->setEnabled(false);
    iconActions->setEnabled(false);
    menuIconAdd->setEnabled(false);
    btnPack->setEnabled(false);
    setIconsLoading(false);
}

void MainWindow::associate() const
{
#ifdef Q_OS_WIN
    QString exe = QDir::toNativeSeparators(QApplication::applicationFilePath());
    QSettings reg("HKEY_CURRENT_USER\\Software\\Classes", QSettings::NativeFormat);
    reg.setValue("apk-icon-editor-reborn.apk/DefaultIcon/Default", exe + ",1");
    reg.setValue("apk-icon-editor-reborn.apk/Shell/Open/Command/Default", exe + " \"%1\"");
    reg.setValue(".apk/Default", "apk-icon-editor-reborn.apk");
#endif
}

void MainWindow::browseSite() const
{
    QDesktopServices::openUrl(Url::WEBSITE);
}

void MainWindow::browseBugs() const
{
    QDesktopServices::openUrl(Url::CONTACT);
}

void MainWindow::browseFaq()
{
    const QString path = Path::Data::shared() + "faq.txt";
    if (!QFile::exists(path)) {
        warning(tr("FAQ"), tr("Could not find FAQ file:\n%1").arg(path));
        return;
    }

    QString program;
    QStringList args;
#if defined(Q_OS_WIN)
    program = "notepad.exe";
    args << QDir::toNativeSeparators(path);
#elif defined(Q_OS_MAC) || defined(Q_OS_MACOS) || defined(Q_OS_OSX)
    program = "open";
    args << path;
#else
    program = "xdg-open";
    args << path;
#endif

    if (!QProcess::startDetached(program, args)) {
        warning(tr("FAQ"), tr("Could not open FAQ file:\n%1").arg(path));
    }
}

void MainWindow::openLogFile() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(Path::Log::file()));
}

void MainWindow::openLogPath() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(Path::Log::dir()));
}

void MainWindow::donate()
{
    QDesktopServices::openUrl(Url::DONATE);
}

void MainWindow::browseTranslate() const
{
    QDesktopServices::openUrl(Url::TRANSLATE);
}

void MainWindow::checkUpdates()
{
    manualUpdateCheck = true;
    updater->check();
}

bool MainWindow::newVersion(QString version)
{
    QMessageBox msgBox(
                QMessageBox::Information,
                tr("Update"),
                tr("%1 v%2 is available.\nDownload?").arg(APP, version),
                QMessageBox::Yes | QMessageBox::No,
                this);

    if (msgBox.exec() == QMessageBox::Yes) {
        updater->download();
        return true;
    }
    else {
        return false;
    }
}

void MainWindow::updateChecked(QString version, bool updateAvailable, QString error)
{
    if (!manualUpdateCheck) {
        return;
    }
    manualUpdateCheck = false;

    if (updateAvailable) {
        return;
    }

    if (!error.isEmpty()) {
        warning(tr("Update"), tr("Could not check for updates."), error);
        return;
    }

    if (version.isEmpty()) {
        success(tr("Update"), tr("No release version was found."));
    } else {
        success(tr("Update"), tr("%1 is up to date.").arg(APP), tr("Latest version: %1").arg(version));
    }
}

void MainWindow::authCloud()
{
    Cloud *cloud = dynamic_cast<Cloud*>(sender());
    QApplication::alert(this);
    QString code = Dialogs::getPassword(tr("Allow access to %1 in your browser and paste the provided code here:").arg(cloud->getTitle()),
                                        tr("Authorization"),
                                        cloud->getIcon(), this);
    if (!code.isEmpty()) {
        cloud->login(code);
        uploadDialog->activateWindow();
    }
    else {
        cloud->cancel();
    }
}

void MainWindow::message(QString title, QString text, QString details, QMessageBox::Icon type)
{
    if (type == QMessageBox::Warning) {
        qDebug() << qPrintable(QString("Warning (%1): %2").arg(title).arg(text));
    } else if (type == QMessageBox::Critical) {
        qDebug() << qPrintable(QString("Error (%1): %2").arg(title).arg(text));
    }
    QApplication::alert(this);
    QMessageBox box(this);
    box.setWindowTitle(title);
    box.setText(text);
    box.setIcon(type);
    box.setStandardButtons(QMessageBox::Ok);
    box.setDetailedText(details);
    if (!details.isEmpty()) {
        QSpacerItem *spacer = new QSpacerItem(320, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout *layout = static_cast<QGridLayout *>(box.layout());
        layout->addItem(spacer, layout->rowCount(), 0, 1, layout->columnCount());
    }
    box.exec();
}

void MainWindow::success(QString title, QString text, QString details)
{
    message(title, text, details, QMessageBox::Information);
}

void MainWindow::warning(QString title, QString text, QString details)
{
    message(title, text, details, QMessageBox::Warning);
}

void MainWindow::error(QString title, QString text, QString details)
{
    message(title, text, details, QMessageBox::Critical);
}

void MainWindow::removeIcon()
{
    Icon *icon = drawArea->getIcon();
    if (icon && QMessageBox::question(this, QString(), tr("Are you sure you want to delete this icon?")) == QMessageBox::Yes) {
        apk->removeIcon(icon);
        setWindowModified(true);
    }
}

bool MainWindow::confirmExit()
{
    if (isWindowModified()) {
        return (QMessageBox::question(this, tr("Discard Changes?"),
                                      tr("APK has been modified. Discard changes?"),
                                      QMessageBox::Discard, QMessageBox::Cancel)
                == QMessageBox::Cancel);
    }
    else {
        return false;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    event->accept();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        const QString filename = mimeData->urls().at(0).toLocalFile();
        const QString ext = QFileInfo(filename).suffix().toLower();
        if (Image::Formats::supported().contains(ext)) {
            icon_open(filename);
        }
        else if (ext == "apk") {
            apk_open(filename);
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateAccentBorderOverlay();
}

void MainWindow::updateAccentBorderOverlay()
{
    if (!accentBorderOverlay) {
        return;
    }

    accentBorderOverlay->setGeometry(rect());
    accentBorderOverlay->raise();
    accentBorderOverlay->setVisible(!isMaximized() && !isFullScreen());
    accentBorderOverlay->update();
}

Qt::Edges MainWindow::framelessResizeEdgesAt(const QPoint &globalPos) const
{
    if (!isVisible() || isMaximized() || isFullScreen()) {
        return {};
    }

    const int border = 8;
    const QPoint pos = mapFromGlobal(globalPos);
    const QRect windowRect = rect();
    if (!windowRect.adjusted(-border, -border, border, border).contains(pos)) {
        return {};
    }

    Qt::Edges edges;
    if (pos.x() >= windowRect.left() && pos.x() < windowRect.left() + border) {
        edges |= Qt::LeftEdge;
    }
    if (pos.x() <= windowRect.right() && pos.x() > windowRect.right() - border) {
        edges |= Qt::RightEdge;
    }
    if (pos.y() >= windowRect.top() && pos.y() < windowRect.top() + border) {
        edges |= Qt::TopEdge;
    }
    if (pos.y() <= windowRect.bottom() && pos.y() > windowRect.bottom() - border) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

static Qt::CursorShape resizeCursorShape(Qt::Edges edges)
{
    const bool left = edges.testFlag(Qt::LeftEdge);
    const bool right = edges.testFlag(Qt::RightEdge);
    const bool top = edges.testFlag(Qt::TopEdge);
    const bool bottom = edges.testFlag(Qt::BottomEdge);

    if ((top && left) || (bottom && right)) {
        return Qt::SizeFDiagCursor;
    }
    if ((top && right) || (bottom && left)) {
        return Qt::SizeBDiagCursor;
    }
    if (left || right) {
        return Qt::SizeHorCursor;
    }
    if (top || bottom) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}

void MainWindow::updateFramelessResizeCursor(const QPoint &globalPos)
{
    const Qt::Edges edges = framelessResizeEdgesAt(globalPos);
    if (!edges) {
        clearFramelessResizeCursor();
        return;
    }

    const QCursor cursor(resizeCursorShape(edges));
    if (framelessResizeCursorActive && QApplication::overrideCursor()) {
        QApplication::changeOverrideCursor(cursor);
    } else {
        QApplication::setOverrideCursor(cursor);
        framelessResizeCursorActive = true;
    }
}

void MainWindow::clearFramelessResizeCursor()
{
    if (framelessResizeCursorActive && QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }
    framelessResizeCursorActive = false;
}

bool MainWindow::startFramelessResize(const QPoint &globalPos)
{
    const Qt::Edges edges = framelessResizeEdgesAt(globalPos);
    if (!edges) {
        return false;
    }
    if (QWindow *handle = windowHandle()) {
        return handle->startSystemResize(edges);
    }
    return false;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    QWidget *eventWidget = qobject_cast<QWidget *>(object);
    if (eventWidget && (eventWidget == this || isAncestorOf(eventWidget))) {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::HoverMove:
        case QEvent::MouseMove:
            updateFramelessResizeCursor(QCursor::pos());
            break;
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && startFramelessResize(QCursor::pos())) {
                mouseEvent->accept();
                return true;
            }
            break;
        }
        case QEvent::Leave:
            if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
                clearFramelessResizeCursor();
            }
            break;
        case QEvent::Hide:
        case QEvent::Close:
        case QEvent::WindowDeactivate:
            clearFramelessResizeCursor();
            break;
        default:
            break;
        }
    }

    if (event->type() == QEvent::Show ||
        event->type() == QEvent::WindowStateChange ||
        event->type() == QEvent::Hide ||
        event->type() == QEvent::Close ||
        event->type() == QEvent::WindowActivate ||
        event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::ActivationChange) {
        QWidget *widget = qobject_cast<QWidget *>(object);
        if (widget && widget->isWindow()) {
            const bool dark = currentTheme == THEME_DARK || (currentTheme == THEME_SYSTEM && isSystemDark());
            const bool initialMainWindowShow = widget == this
                && event->type() == QEvent::Show
                && !windowsInitialShowHandled;
            const bool initialMainWindowActivation = widget == this
                && windowsSkipInitialActivationRefresh
                && (event->type() == QEvent::WindowActivate
                    || event->type() == QEvent::ActivationChange);
            if (initialMainWindowShow || initialMainWindowActivation) {
                setWindowsDarkTitleBar(widget, dark);
                if (initialMainWindowShow) {
                    windowsInitialShowHandled = true;
                    windowsSkipInitialActivationRefresh = true;
                    QTimer::singleShot(750, this, [this]() {
                        windowsSkipInitialActivationRefresh = false;
                    });
                }
                return QMainWindow::eventFilter(object, event);
            }
            const bool forceNativeFrameRefresh = event->type() == QEvent::Show
                || event->type() == QEvent::Hide
                || event->type() == QEvent::Close
                || event->type() == QEvent::WindowActivate;
            setWindowsDarkTitleBar(widget, dark, forceNativeFrameRefresh);
            const QPointer<QWidget> guardedWidget(widget);
            QTimer::singleShot(0, qApp, [guardedWidget, dark, forceNativeFrameRefresh]() {
                setWindowsDarkTitleBar(guardedWidget, dark, forceNativeFrameRefresh);
            });
            QTimer::singleShot(100, qApp, [guardedWidget, dark, forceNativeFrameRefresh]() {
                setWindowsDarkTitleBar(guardedWidget, dark, forceNativeFrameRefresh);
            });
            QTimer::singleShot(300, qApp, [dark, forceNativeFrameRefresh]() {
                setWindowsDarkTitleBars(dark, forceNativeFrameRefresh);
            });
            QTimer::singleShot(700, qApp, [dark]() { setWindowsDarkTitleBars(dark); });
        }
    }
    if (object == this && event->type() == QEvent::WindowStateChange) {
        updateAccentBorderOverlay();
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
#endif
{
    Q_UNUSED(eventType)
    MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_NCCALCSIZE && msg->wParam) {
        *result = 0;
        return true;
    }

    if (msg && msg->message == WM_NCHITTEST) {
        const QPoint globalPos(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        const Qt::Edges edges = framelessResizeEdgesAt(globalPos);
        if (edges) {
            updateFramelessResizeCursor(globalPos);

            if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::LeftEdge)) {
                *result = HTTOPLEFT;
                return true;
            }
            if (edges.testFlag(Qt::TopEdge) && edges.testFlag(Qt::RightEdge)) {
                *result = HTTOPRIGHT;
                return true;
            }
            if (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::LeftEdge)) {
                *result = HTBOTTOMLEFT;
                return true;
            }
            if (edges.testFlag(Qt::BottomEdge) && edges.testFlag(Qt::RightEdge)) {
                *result = HTBOTTOMRIGHT;
                return true;
            }
            if (edges.testFlag(Qt::LeftEdge)) {
                *result = HTLEFT;
                return true;
            }
            if (edges.testFlag(Qt::RightEdge)) {
                *result = HTRIGHT;
                return true;
            }
            if (edges.testFlag(Qt::TopEdge)) {
                *result = HTTOP;
                return true;
            }
            if (edges.testFlag(Qt::BottomEdge)) {
                *result = HTBOTTOM;
                return true;
            }
        }

        if (customTitleBar) {
            const QPoint titlePos = customTitleBar->mapFromGlobal(globalPos);
            if (customTitleBar->rect().contains(titlePos)) {
                QWidget *child = customTitleBar->childAt(titlePos);
                if (!qobject_cast<QToolButton *>(child)) {
                    *result = HTCAPTION;
                    return true;
                }
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Confirm exit:

    if (confirmExit()) {
        event->ignore();
        return;
    }

    // Save Settings:

    Settings::set_version(VER);
    Settings::set_device(devices->currentText());
    Settings::set_language(currentLang);
    Settings::set_theme(currentTheme);
    Settings::set_update(actAutoUpdate->isChecked());
    Settings::set_activities(actViewActivities->isChecked());
    Settings::set_path(currentPath);
    Settings::set_geometry(saveGeometry());
    Settings::set_splitter(splitter->saveState());
    Settings::set_recent(recent->files());

    Settings::set_upload(checkUpload->isChecked());
    Settings::set_dropbox(checkDropbox->isChecked());
    Settings::set_gdrive(checkGDrive->isChecked());
    Settings::set_onedrive(checkOneDrive->isChecked());
    Settings::set_dropbox_token(dropbox->getToken());
    Settings::set_gdrive_token(gdrive->getToken());
    Settings::set_onedrive_token(onedrive->getToken());

    Settings::kill();

    // Close window:

    event->accept();
}

MainWindow::~MainWindow()
{
    qDebug() << "Exiting...";
    delete recent;
    qInstallMessageHandler(0);
}
