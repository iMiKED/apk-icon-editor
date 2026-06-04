#include "decorationdelegate.h"
#include <QPainter>

DecorationDelegate::DecorationDelegate(const QSize &size, QObject *parent) : QStyledItemDelegate(parent)
{
    decorationSize = size;
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

    QRect textRect = content.adjusted(decorationSize.width() + margin * 2, 0, 0, 0);
    painter->setPen(text);
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                      opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, textRect.width()));
    painter->restore();
}

void DecorationDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->decorationSize = decorationSize;
}
