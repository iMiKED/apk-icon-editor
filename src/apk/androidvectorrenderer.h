#ifndef ANDROIDVECTORRENDERER_H
#define ANDROIDVECTORRENDERER_H

#include "resourceresolver.h"
#include <QBrush>
#include <QDomElement>
#include <QPainterPath>
#include <QPixmap>

class AndroidVectorRenderer
{
public:
    static QPixmap render(const ResourceResolver &resolver, const QString &filePath, const QSize &size);
    static QPixmap render(const ResourceResolver &resolver, const QDomElement &root, const QSize &size);

private:
    struct Parser {
        QString data;
        int pos = 0;
        QPointF current;
        QPointF subpathStart;
        QPointF lastControl;
        QChar previousCommand;
        QPainterPath path;

        explicit Parser(const QString &pathData);
        QPainterPath parse();
        bool hasMore() const;
        void skipSeparators();
        bool nextIsCommand();
        QChar readCommand();
        bool readNumber(qreal *value);
    };

    static void renderNode(const ResourceResolver &resolver, QPainter *painter, const QDomElement &node);
    static void renderGroup(const ResourceResolver &resolver, QPainter *painter, const QDomElement &node);
    static void renderPath(const ResourceResolver &resolver, QPainter *painter, const QDomElement &node);
    static QBrush resolveBrush(const ResourceResolver &resolver, const QString &value, const QRectF &bounds);
    static QBrush gradientBrush(const QString &filePath, const QRectF &bounds);
    static QColor parseColor(const QString &value);
    static qreal numberAttr(const QDomElement &node, const QString &name, qreal fallback = 0);
    static QString stringAttr(const QDomElement &node, const QString &name);
    static void arcTo(Parser *parser, const QPointF &from, qreal rx, qreal ry, qreal rotation, bool largeArc, bool sweep, const QPointF &to);
};

#endif // ANDROIDVECTORRENDERER_H
