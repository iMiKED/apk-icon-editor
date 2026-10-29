#include "decorationdelegate.h"
#include <QPainter>

DecorationDelegate::DecorationDelegate(const QSize &size, QObject *parent) : QStyledItemDelegate(parent)
{
    decorationSize = size;
}

QSize DecorationDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    QSize size = QStyledItemDelegate::sizeHint(opt, index);
    const int textHeight = (opt.text.count('\n') + 1) * opt.fontMetrics.height() + 8;
    size.setHeight(qMax(textHeight, opt.icon.isNull() ? 24 : decorationSize.height() + 4));
    return size;
}

void DecorationDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const QColor fill = selected
        ? opt.palette.color(QPalette::Highlight)
        : hover
            ? opt.palette.color(QPalette::AlternateBase)
            : opt.palette.color(QPalette::Base);
    const QColor text = selected
        ? opt.palette.color(QPalette::HighlightedText)
        : opt.palette.color(QPalette::Text);

    painter->save();
    painter->fillRect(opt.rect, fill);
    painter->setFont(opt.font);

    const int margin = 4;
    QRect content = opt.rect.adjusted(margin, 0, -margin, 0);
    QRect iconRect(content.left(),
                   content.top() + (content.height() - decorationSize.height()) / 2,
                   decorationSize.width(),
                   decorationSize.height());
    const QIcon::Mode iconMode = opt.state & QStyle::State_Enabled
        ? selected ? QIcon::Selected : QIcon::Normal
        : QIcon::Disabled;
    opt.icon.paint(painter, iconRect, Qt::AlignCenter, iconMode, QIcon::Off);

    QRect textRect = content.adjusted(opt.icon.isNull() ? 0 : decorationSize.width() + margin * 2, 0, 0, 0);
    painter->setPen(text);
    const QStringList lines = opt.text.split('\n');
    int top = textRect.center().y() - lines.size() * opt.fontMetrics.height() / 2;
    for (const QString &line : lines) {
        painter->drawText(QRect(textRect.left(), top, textRect.width(), opt.fontMetrics.height()),
                         Qt::AlignVCenter | Qt::AlignLeft,
                         opt.fontMetrics.elidedText(line, Qt::ElideRight, textRect.width()));
        top += opt.fontMetrics.height();
    }
    painter->restore();
}

void DecorationDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->text.replace(QChar::LineSeparator, QChar::LineFeed);
    option->decorationSize = decorationSize;
}
