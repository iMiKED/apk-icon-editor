#include "updater.h"
#include "globals.h"
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QThread>

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
    // Unit test is available for this function.

    QStringList segments1 = v1.split(QRegularExpression("\\D"));
    QStringList segments2 = v2.split(QRegularExpression("\\D"));

    for (short i = 0; i < segments1.size(); ++i) {

        if (segments2.size() <= i) {
            segments2.push_back("0");
        }

        const int SEGMENT1 = segments1.at(i).toInt();
        const int SEGMENT2 = segments2.at(i).toInt();

        if (SEGMENT1 == SEGMENT2) {
            continue;
        }
        else {
            return (SEGMENT1 > SEGMENT2);
        }
    }
    return false;
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
