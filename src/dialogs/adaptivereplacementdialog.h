#ifndef ADAPTIVEREPLACEMENTDIALOG_H
#define ADAPTIVEREPLACEMENTDIALOG_H

#include <QDialog>
#include "icon.h"

class QCheckBox;

class AdaptiveReplacementDialog : public QDialog
{
    Q_OBJECT
public:
    AdaptiveReplacementDialog(Icon *icon, const QPixmap &replacement, int densityCount,
                              const QList<Icon *> &legacy, QWidget *parent = nullptr);
    bool updateLegacy() const;

private:
    QCheckBox *legacyCheck;
};

#endif
