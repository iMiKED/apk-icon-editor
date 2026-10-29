#include "adaptivereplacementdialog.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

AdaptiveReplacementDialog::AdaptiveReplacementDialog(Icon *icon, const QPixmap &replacement,
        int densityCount, const QList<Icon *> &legacy, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Adaptive Icon Replacement"));
    setMinimumWidth(380);
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *entry = new QLabel(icon->getEntryRoleTitle() + "\n" + icon->getResourceRef(), this);
    entry->setTextFormat(Qt::PlainText);
    entry->setWordWrap(true);
    layout->addWidget(entry);

    QHBoxLayout *previews = new QHBoxLayout;
    const QList<QPixmap> images = { icon->getPixmap(), replacement };
    const QStringList captions = { tr("Current icon"), tr("New foreground") };
    for (qsizetype i = 0; i < images.size(); ++i) {
        QVBoxLayout *column = new QVBoxLayout;
        QLabel *preview = new QLabel(this);
        preview->setFixedSize(128, 128);
        preview->setAlignment(Qt::AlignCenter);
        preview->setPixmap(images.at(i).scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QLabel *caption = new QLabel(captions.at(i), this);
        caption->setAlignment(Qt::AlignCenter);
        caption->setWordWrap(true);
        column->addWidget(preview, 0, Qt::AlignHCenter);
        column->addWidget(caption);
        previews->addLayout(column);
    }
    layout->addLayout(previews);
    QFormLayout *details = new QFormLayout;
    details->addRow(tr("Background:"), new QLabel(tr("Keep original"), this));
    details->addRow(tr("Densities:"), new QLabel(tr("%1 variants").arg(densityCount), this));
    layout->addLayout(details);

    QSet<QString> paths;
    bool writable = true;
    for (Icon *alternative : legacy) {
        paths.insert(alternative->getFilename());
        writable = writable && !alternative->isReadOnly();
    }
    legacyCheck = new QCheckBox(tr("Also update legacy bitmap alternatives"), this);
    legacyCheck->setObjectName("updateLegacyCheck");
    legacyCheck->setChecked(false);
    legacyCheck->setEnabled(!paths.isEmpty() && writable);
    layout->addWidget(legacyCheck);
    QLabel *status = new QLabel(this);
    status->setWordWrap(true);
    layout->addWidget(status);
    const int count = paths.size();
    const auto updateStatus = [this, status, count, writable](bool checked) {
        status->setText(count == 0 ? tr("No legacy alternatives")
                : !writable ? tr("Legacy alternatives include read-only split resources")
                : checked ? tr("Legacy: %1 files will be updated").arg(count)
                          : tr("Legacy: %1 files unchanged").arg(count));
    };
    connect(legacyCheck, &QCheckBox::toggled, this, updateStatus);
    updateStatus(false);
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Replace"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

bool AdaptiveReplacementDialog::updateLegacy() const
{
    return legacyCheck->isEnabled() && legacyCheck->isChecked();
}
