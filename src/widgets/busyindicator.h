///
/// \file
/// This file contains a small busy indicator widget declaration.
///

#ifndef BUSYINDICATOR_H
#define BUSYINDICATOR_H

#include <QWidget>

class QTimer;

///
/// Circular busy indicator.
///

class BusyIndicator : public QWidget {
    Q_OBJECT

public:
    explicit BusyIndicator(QWidget *parent = 0);
    QSize sizeHint() const;

protected:
    void paintEvent(QPaintEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

private:
    QTimer *timer;
    int step;
};

#endif // BUSYINDICATOR_H
