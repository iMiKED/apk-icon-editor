#include "updater.h"
#include "globals.h"
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QThread>
#include <QVersionNumber>

// Updater

void Updater::check() const
{
    QThread *thread = new QThread();
    UpdateWorker *worker = new UpdateWorker();
    worker->moveToThread(thread);

    connect(thread, SIGNAL(started()), worker, SLOT(check()));
    connect(worker, SIGNAL(version(QString)), this, SIGNAL(version(QString)));
    connect(worker, SIGNAL(checked(QString,bool,QString)), this, SIGNAL(checked(QString,bool,QString)));
    connect(worker, SIGNAL(finished()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
}

void Updater::download() const
{
    QDesktopServices::openUrl(QUrl(Url::UPDATE));
}

bool Updater::compare(QString v1, QString v2)
{
    static const QRegularExpression version("^v?([0-9]+(?:\\.[0-9]+)*)(?:-([^+]+))?(?:\\+.*)?$",
                                             QRegularExpression::CaseInsensitiveOption);
    const auto first = version.match(v1.trimmed());
    const auto second = version.match(v2.trimmed());
    if (!first.hasMatch() || !second.hasMatch()) return false;
    const int base = QVersionNumber::compare(QVersionNumber::fromString(first.captured(1)).normalized(),
                                             QVersionNumber::fromString(second.captured(1)).normalized());
    if (base != 0) return base > 0;

    const QString pre1 = first.captured(2).toLower();
    const QString pre2 = second.captured(2).toLower();
    // A final release is newer than every prerelease of the same base version.
    if (pre1.isEmpty() || pre2.isEmpty()) return !pre2.isEmpty() && pre1.isEmpty();
    const auto parts = [](const QString &text) {
        QStringList result;
        auto matches = QRegularExpression("[0-9]+|[a-z]+").globalMatch(text);
        while (matches.hasNext()) result << matches.next().captured();
        return result;
    };
    const QStringList a = parts(pre1);
    const QStringList b = parts(pre2);
    for (qsizetype i = 0; i < qMin(a.size(), b.size()); ++i) {
        if (a.at(i) == b.at(i)) continue;
        const bool numericA = a.at(i).at(0).isDigit();
        const bool numericB = b.at(i).at(0).isDigit();
        if (numericA != numericB) return !numericA;
        if (numericA) {
            QString na = a.at(i), nb = b.at(i);
            while (na.size() > 1 && na.startsWith('0')) na.remove(0, 1);
            while (nb.size() > 1 && nb.startsWith('0')) nb.remove(0, 1);
            if (na.size() != nb.size()) return na.size() > nb.size();
            if (na == nb) continue;
            return na > nb;
        }
        return a.at(i) > b.at(i);
    }
    return a.size() > b.size();
}

// UpdateWorker

UpdateWorker::UpdateWorker(QObject *parent) : QObject(parent)
{
    http = new QNetworkAccessManager(this);
    connect(http, SIGNAL(finished(QNetworkReply*)), this, SLOT(catchReply(QNetworkReply*)));
}

void UpdateWorker::check() const
{
    QNetworkRequest request(Url::VERSION);
    http->get(request);
}

void UpdateWorker::catchReply(QNetworkReply *reply)
{
    QString latest;
    QString error;
    bool updateAvailable = false;

    if (reply->error() == QNetworkReply::NoError) {

        const int CODE = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (CODE >= 200 && CODE < 300) {
            const QString URL = reply->url().toString();
            if (URL.indexOf(Url::VERSION) != -1 || URL.indexOf("api.github.com/repos/iMiKED/apk-icon-editor/releases") != -1) {

                const QString JSON = reply->readAll().trimmed();
                latest = parse(JSON);
                updateAvailable = Updater::compare(latest, VER);
                if (updateAvailable) {
                    emit version(latest);
                }
                if (latest.isEmpty()) {
                    error = tr("Could not find release version in the update response.");
                }
            }
        } else {
            error = tr("Update server returned HTTP %1.").arg(CODE);
        }
    } else {
        error = reply->errorString();
    }

    emit checked(latest, updateAvailable, error);
    reply->deleteLater();
    emit finished();
}

QString UpdateWorker::parse(QString json) const
{
#if defined(Q_OS_WIN)
    const QString OS = "windows";
#elif defined(Q_OS_OSX)
    const QString OS = "osx";
#elif defined(Q_OS_LINUX)
    const QString OS = "linux";
#else
    const QString OS = "windows";
#endif
    Q_UNUSED(OS);
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    const QJsonObject object = document.isArray()
                               ? document.array().at(0).toObject()
                               : document.object();
    QString LATEST = object["tag_name"].toString();
    if (LATEST.isEmpty()) {
        LATEST = object["name"].toString();
    }
    LATEST.remove(QRegularExpression("^v", QRegularExpression::CaseInsensitiveOption));

    if (!LATEST.isEmpty()) {
        qDebug() << qPrintable(QString("Updater: v%1\n").arg(LATEST));
    } else {
        qDebug() << "Updater: no release version found\n";
    }
    return LATEST;
}
