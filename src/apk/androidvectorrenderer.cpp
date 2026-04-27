#include "androidvectorrenderer.h"
#include "resourceref.h"
#include <QFile>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QTextStream>
#include <QtMath>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

static void setUtf8Encoding(QTextStream &stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif
}

AndroidVectorRenderer::Parser::Parser(const QString &pathData)
    : data(pathData)
{
}

bool AndroidVectorRenderer::Parser::hasMore() const
{
    return pos < data.length();
}

void AndroidVectorRenderer::Parser::skipSeparators()
{
    while (hasMore()) {
        const QChar ch = data.at(pos);
        if (!ch.isSpace() && ch != ',') {
            break;
        }
        ++pos;
    }
}

bool AndroidVectorRenderer::Parser::nextIsCommand()
{
    skipSeparators();
    return hasMore() && data.at(pos).isLetter();
}

QChar AndroidVectorRenderer::Parser::readCommand()
{
    skipSeparators();
    return hasMore() ? data.at(pos++) : QChar();
}

bool AndroidVectorRenderer::Parser::readNumber(qreal *value)
{
    skipSeparators();
    static const QRegularExpression numberPattern(QStringLiteral(R"(^[+-]?(?:(?:\d+\.\d*)|(?:\.\d+)|(?:\d+))(?:[eE][+-]?\d+)?)"));
    const QRegularExpressionMatch match = numberPattern.match(data.mid(pos));
    if (!match.hasMatch()) {
        return false;
    }

    *value = match.captured(0).toDouble();
    pos += match.capturedLength(0);
    skipSeparators();
    return true;
}

QPainterPath AndroidVectorRenderer::Parser::parse()
{
    QChar command;
    while (hasMore()) {
        if (nextIsCommand()) {
            command = readCommand();
        } else if (command.isNull()) {
            break;
        }

        const bool relative = command.isLower();
        const QChar op = command.toUpper();
        if (op == 'Z') {
            path.closeSubpath();
            current = subpathStart;
            previousCommand = op;
            command = QChar();
            continue;
        }

        bool firstMove = true;
        while (hasMore() && !nextIsCommand()) {
            qreal x = 0, y = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, rx = 0, ry = 0, angle = 0, large = 0, sweep = 0;
            if (op == 'M') {
                if (!readNumber(&x) || !readNumber(&y)) break;
                QPointF p(x, y);
                if (relative) p += current;
                if (firstMove) {
                    path.moveTo(p);
                    subpathStart = p;
                    firstMove = false;
                } else {
                    path.lineTo(p);
                }
                current = p;
            } else if (op == 'L') {
                if (!readNumber(&x) || !readNumber(&y)) break;
                QPointF p(x, y);
                if (relative) p += current;
                path.lineTo(p);
                current = p;
            } else if (op == 'H') {
                if (!readNumber(&x)) break;
                if (relative) x += current.x();
                current.setX(x);
                path.lineTo(current);
            } else if (op == 'V') {
                if (!readNumber(&y)) break;
                if (relative) y += current.y();
                current.setY(y);
                path.lineTo(current);
            } else if (op == 'C') {
                if (!readNumber(&x1) || !readNumber(&y1) || !readNumber(&x2) || !readNumber(&y2) || !readNumber(&x) || !readNumber(&y)) break;
                QPointF c1(x1, y1), c2(x2, y2), p(x, y);
                if (relative) {
                    c1 += current; c2 += current; p += current;
                }
                path.cubicTo(c1, c2, p);
                lastControl = c2;
                current = p;
            } else if (op == 'S') {
                if (!readNumber(&x2) || !readNumber(&y2) || !readNumber(&x) || !readNumber(&y)) break;
                QPointF c1 = (previousCommand == 'C' || previousCommand == 'S') ? current * 2 - lastControl : current;
                QPointF c2(x2, y2), p(x, y);
                if (relative) {
                    c2 += current; p += current;
                }
                path.cubicTo(c1, c2, p);
                lastControl = c2;
                current = p;
            } else if (op == 'Q') {
                if (!readNumber(&x1) || !readNumber(&y1) || !readNumber(&x) || !readNumber(&y)) break;
                QPointF c(x1, y1), p(x, y);
                if (relative) {
                    c += current; p += current;
                }
                path.quadTo(c, p);
                lastControl = c;
                current = p;
            } else if (op == 'T') {
                if (!readNumber(&x) || !readNumber(&y)) break;
                QPointF c = (previousCommand == 'Q' || previousCommand == 'T') ? current * 2 - lastControl : current;
                QPointF p(x, y);
                if (relative) p += current;
                path.quadTo(c, p);
                lastControl = c;
                current = p;
            } else if (op == 'A') {
                if (!readNumber(&rx) || !readNumber(&ry) || !readNumber(&angle) || !readNumber(&large) || !readNumber(&sweep) || !readNumber(&x) || !readNumber(&y)) break;
                QPointF p(x, y);
                if (relative) p += current;
                AndroidVectorRenderer::arcTo(this, current, rx, ry, angle, large != 0, sweep != 0, p);
                current = p;
            } else {
                break;
            }
            previousCommand = op;
        }
    }
    return path;
}

QPixmap AndroidVectorRenderer::render(const ResourceResolver &resolver, const QString &filePath, const QSize &size)
{
    QFile file(filePath);
    if (!size.isValid() || !file.open(QFile::ReadOnly | QFile::Text)) {
        return QPixmap();
    }

    QTextStream stream(&file);
    setUtf8Encoding(stream);
    QDomDocument doc;
    if (!doc.setContent(stream.readAll())) {
        return QPixmap();
    }

    const QDomElement root = doc.documentElement();
    if (root.tagName() != "vector") {
        return QPixmap();
    }

    const qreal viewportWidth = numberAttr(root, "viewportWidth", size.width());
    const qreal viewportHeight = numberAttr(root, "viewportHeight", size.height());
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return QPixmap();
    }

    QImage canvas(size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size.width() / viewportWidth, size.height() / viewportHeight);

    QDomElement child = root.firstChildElement();
    while (!child.isNull()) {
        renderNode(resolver, &painter, child);
        child = child.nextSiblingElement();
    }
    painter.end();
    return QPixmap::fromImage(canvas);
}

void AndroidVectorRenderer::renderNode(const ResourceResolver &resolver, QPainter *painter, const QDomElement &node)
{
    if (node.tagName() == "group") {
        renderGroup(resolver, painter, node);
    } else if (node.tagName() == "path") {
        renderPath(resolver, painter, node);
    } else if (node.tagName() == "clip-path") {
        Parser parser(stringAttr(node, "pathData"));
        painter->setClipPath(parser.parse(), Qt::IntersectClip);
    }
}

void AndroidVectorRenderer::renderGroup(const ResourceResolver &resolver, QPainter *painter, const QDomElement &node)
{
    painter->save();
    const qreal pivotX = numberAttr(node, "pivotX");
    const qreal pivotY = numberAttr(node, "pivotY");
    painter->translate(numberAttr(node, "translateX"), numberAttr(node, "translateY"));
    painter->translate(pivotX, pivotY);
    const qreal rotation = numberAttr(node, "rotation");
    if (!qFuzzyIsNull(rotation)) {
        painter->rotate(rotation);
    }
    painter->scale(numberAttr(node, "scaleX", 1), numberAttr(node, "scaleY", 1));
    painter->translate(-pivotX, -pivotY);

    QDomElement child = node.firstChildElement();
    while (!child.isNull()) {
        renderNode(resolver, painter, child);
        child = child.nextSiblingElement();
    }
    painter->restore();
}

void AndroidVectorRenderer::renderPath(const ResourceResolver &resolver, QPainter *painter, const QDomElement &node)
{
    Parser parser(stringAttr(node, "pathData"));
    QPainterPath path = parser.parse();
    if (path.isEmpty()) {
        return;
    }

    const QString fillType = stringAttr(node, "fillType");
    if (fillType == "evenOdd") {
        path.setFillRule(Qt::OddEvenFill);
    }

    const QString fillColor = stringAttr(node, "fillColor");
    if (!fillColor.isEmpty() && fillColor != "@android:color/transparent" && fillColor != "#00000000") {
        painter->fillPath(path, resolveBrush(resolver, fillColor, path.boundingRect()));
    }
}

QBrush AndroidVectorRenderer::resolveBrush(const ResourceResolver &resolver, const QString &value, const QRectF &bounds)
{
    if (value.startsWith('@')) {
        ResourceResolver::Value color = resolver.resolveColor(ResourceRef(value));
        if (color.found && color.color.isValid()) {
            return QBrush(color.color);
        }

        ResourceResolver::Value xml = resolver.resolveXml(ResourceRef(value));
        if (xml.found) {
            return gradientBrush(xml.filePath, bounds);
        }
    }
    return QBrush(parseColor(value));
}

QBrush AndroidVectorRenderer::gradientBrush(const QString &filePath, const QRectF &bounds)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QBrush(Qt::transparent);
    }

    QTextStream stream(&file);
    setUtf8Encoding(stream);
    QDomDocument doc;
    if (!doc.setContent(stream.readAll())) {
        return QBrush(Qt::transparent);
    }

    const QDomElement root = doc.documentElement();
    if (root.tagName() != "gradient") {
        return QBrush(Qt::transparent);
    }

    QGradient *gradient = nullptr;
    const QString type = stringAttr(root, "type");
    if (type == "radial") {
        gradient = new QRadialGradient(numberAttr(root, "centerX", bounds.center().x()), numberAttr(root, "centerY", bounds.center().y()), numberAttr(root, "gradientRadius", qMax(bounds.width(), bounds.height()) / 2));
    } else {
        gradient = new QLinearGradient(numberAttr(root, "startX", bounds.left()), numberAttr(root, "startY", bounds.top()), numberAttr(root, "endX", bounds.left()), numberAttr(root, "endY", bounds.bottom()));
    }

    QDomElement item = root.firstChildElement("item");
    while (!item.isNull()) {
        gradient->setColorAt(numberAttr(item, "offset"), parseColor(stringAttr(item, "color")));
        item = item.nextSiblingElement("item");
    }

    QBrush brush(*gradient);
    delete gradient;
    return brush;
}

QColor AndroidVectorRenderer::parseColor(const QString &value)
{
    QString text = value.trimmed();
    if (text.length() == 9 && text.startsWith('#')) {
        bool ok = false;
        const uint argb = text.mid(1).toUInt(&ok, 16);
        if (ok) {
            return QColor((argb >> 16) & 0xff, (argb >> 8) & 0xff, argb & 0xff, (argb >> 24) & 0xff);
        }
    }
    return QColor(text);
}

qreal AndroidVectorRenderer::numberAttr(const QDomElement &node, const QString &name, qreal fallback)
{
    const QString value = stringAttr(node, name);
    if (value.isEmpty()) {
        return fallback;
    }

    QString number = value;
    number.remove(QStringLiteral("dp"));
    bool ok = false;
    const qreal result = number.toDouble(&ok);
    return ok ? result : fallback;
}

QString AndroidVectorRenderer::stringAttr(const QDomElement &node, const QString &name)
{
    QString value = node.attribute("android:" + name);
    if (value.isEmpty()) {
        value = node.attribute(name);
    }
    return value.trimmed();
}

void AndroidVectorRenderer::arcTo(Parser *parser, const QPointF &from, qreal rx, qreal ry, qreal rotation, bool largeArc, bool sweep, const QPointF &to)
{
    if (qFuzzyIsNull(rx) || qFuzzyIsNull(ry) || from == to) {
        parser->path.lineTo(to);
        return;
    }

    rx = qAbs(rx);
    ry = qAbs(ry);
    const qreal phi = qDegreesToRadians(rotation);
    const qreal cosPhi = qCos(phi);
    const qreal sinPhi = qSin(phi);
    const qreal dx = (from.x() - to.x()) / 2.0;
    const qreal dy = (from.y() - to.y()) / 2.0;
    const qreal x1p = cosPhi * dx + sinPhi * dy;
    const qreal y1p = -sinPhi * dx + cosPhi * dy;

    const qreal lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1) {
        const qreal scale = qSqrt(lambda);
        rx *= scale;
        ry *= scale;
    }

    const qreal rx2 = rx * rx;
    const qreal ry2 = ry * ry;
    const qreal x1p2 = x1p * x1p;
    const qreal y1p2 = y1p * y1p;
    const qreal sign = (largeArc == sweep) ? -1.0 : 1.0;
    const qreal coef = sign * qSqrt(qMax<qreal>(0, (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / (rx2 * y1p2 + ry2 * x1p2)));
    const qreal cxp = coef * (rx * y1p / ry);
    const qreal cyp = coef * (-ry * x1p / rx);
    const qreal cx = cosPhi * cxp - sinPhi * cyp + (from.x() + to.x()) / 2.0;
    const qreal cy = sinPhi * cxp + cosPhi * cyp + (from.y() + to.y()) / 2.0;

    auto angleBetween = [](qreal ux, qreal uy, qreal vx, qreal vy) {
        const qreal dot = ux * vx + uy * vy;
        const qreal len = qSqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        qreal angle = qAcos(qBound<qreal>(-1, dot / len, 1));
        if (ux * vy - uy * vx < 0) {
            angle = -angle;
        }
        return angle;
    };

    const qreal ux = (x1p - cxp) / rx;
    const qreal uy = (y1p - cyp) / ry;
    const qreal vx = (-x1p - cxp) / rx;
    const qreal vy = (-y1p - cyp) / ry;
    qreal start = angleBetween(1, 0, ux, uy);
    qreal delta = angleBetween(ux, uy, vx, vy);
    if (!sweep && delta > 0) delta -= 2 * M_PI;
    if (sweep && delta < 0) delta += 2 * M_PI;

    const int segments = qCeil(qAbs(delta) / (M_PI / 2.0));
    const qreal step = delta / segments;
    for (int i = 0; i < segments; ++i) {
        const qreal t1 = start + i * step;
        const qreal t2 = t1 + step;
        const qreal alpha = 4.0 / 3.0 * qTan((t2 - t1) / 4.0);
        QPointF p1(rx * (qCos(t1) - alpha * qSin(t1)), ry * (qSin(t1) + alpha * qCos(t1)));
        QPointF p2(rx * (qCos(t2) + alpha * qSin(t2)), ry * (qSin(t2) - alpha * qCos(t2)));
        QPointF p(rx * qCos(t2), ry * qSin(t2));
        auto map = [&](const QPointF &pt) {
            return QPointF(cx + cosPhi * pt.x() - sinPhi * pt.y(), cy + sinPhi * pt.x() + cosPhi * pt.y());
        };
        parser->path.cubicTo(map(p1), map(p2), map(p));
    }
}
