#include "apkunpacker.h"
#include "manifest.h"
#include "globals.h"
#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

using Apk::Unpacker;

namespace {

static QString manifestAttribute(const QString &manifestPath, const QString &name)
{
    QFile file(manifestPath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QString();
    }

    QDomDocument doc;
    if (!doc.setContent(file.readAll())) {
        return QString();
    }

    const QDomElement manifest = doc.firstChildElement("manifest");
    if (manifest.isNull()) {
        return QString();
    }

    return manifest.attribute(name);
}

static QString safeDirectoryName(const QString &fileName)
{
    QString result = QFileInfo(fileName).completeBaseName();
    result.replace(QRegularExpression("[^A-Za-z0-9._-]+"), "_");
    return result.isEmpty() ? "split" : result;
}

static bool unpackSplitApk(const QString &filepath, const QString &destination, const QString &apktoolPath, const QString &frameworks)
{
    QDir(destination).removeRecursively();

    QStringList args;
    args << "-jar";
    args << apktoolPath;
    args << "d" << filepath;
    args << "-f";
    args << "--res-resolve-mode" << "greedy";
    args << "-s";
    args << "-o" << destination;
    args << "-p" << frameworks;

    QProcess process;
    process.start("java", args);
    if (!process.waitForStarted(30000)) {
        qDebug().noquote() << "Split APK decode failed to start:" << QDir::toNativeSeparators(filepath);
        return false;
    }
    if (!process.waitForFinished(-1)) {
        qDebug().noquote() << "Split APK decode did not finish:" << QDir::toNativeSeparators(filepath);
        return false;
    }
    if (process.exitCode() != 0) {
        const QString errorText = QString::fromUtf8(process.readAllStandardError()).replace("\r\n", "\n").trimmed();
        qDebug().noquote() << "Split APK decode skipped:" << QDir::toNativeSeparators(filepath);
        if (!errorText.isEmpty()) {
            qDebug().noquote() << errorText;
        }
        return false;
    }
    return true;
}

static QStringList unpackCompatibleSplits(const QString &baseApkPath, const QString &baseContentsPath, const QString &apktoolPath, const QString &frameworks)
{
    QStringList result;
    const QString basePackage = manifestAttribute(QDir::cleanPath(baseContentsPath + "/AndroidManifest.xml"), "package");
    if (basePackage.isEmpty()) {
        return result;
    }

    const QFileInfo baseInfo(baseApkPath);
    QDir dir(baseInfo.dir());
    const QFileInfoList candidates = dir.entryInfoList(QStringList() << "*.apk", QDir::Files, QDir::Name);
    if (candidates.count() <= 1) {
        return result;
    }

    const QString splitsRoot = QDir::cleanPath(baseContentsPath + "/_splits");
    QDir().mkpath(splitsRoot);
    int checked = 0;
    int skipped = 0;
    foreach (const QFileInfo &candidate, candidates) {
        if (candidate.canonicalFilePath() == baseInfo.canonicalFilePath()) {
            continue;
        }

        ++checked;
        const QString splitDir = QDir::cleanPath(splitsRoot + "/" + safeDirectoryName(candidate.fileName()));
        qDebug().noquote() << "Checking sibling APK as possible split:" << QDir::toNativeSeparators(candidate.filePath());
        if (!unpackSplitApk(candidate.filePath(), splitDir, apktoolPath, frameworks)) {
            QDir(splitDir).removeRecursively();
            ++skipped;
            continue;
        }

        const QString manifestPath = QDir::cleanPath(splitDir + "/AndroidManifest.xml");
        const QString splitPackage = manifestAttribute(manifestPath, "package");
        const QString splitName = manifestAttribute(manifestPath, "split");
        if (splitPackage != basePackage || splitName.isEmpty()) {
            qDebug().noquote() << "Skipping sibling APK because it is not a compatible split:" << QDir::toNativeSeparators(candidate.filePath());
            QDir(splitDir).removeRecursively();
            ++skipped;
            continue;
        }

        qDebug().noquote() << QString("Detected split APK: %1 (%2)")
                              .arg(splitName, QDir::toNativeSeparators(candidate.filePath()));
        result << splitDir;
    }
    if (checked > 0) {
        qDebug().noquote() << QString("Split APK summary: %1 checked, %2 compatible, %3 skipped")
                              .arg(checked)
                              .arg(result.count())
                              .arg(skipped);
    }
    if (!result.isEmpty()) {
        qDebug().noquote() << QString("Loaded %1 read-only split APK resource roots").arg(result.count());
    }
    return result;
}

} // namespace

Unpacker::Unpacker(QObject *parent) : QObject(parent)
{
    apktool = new QProcess(this);
    apktool->kill();
}

void Unpacker::unpack(QString filepath, QString destination, QString apktoolPath, QString frameworks, bool smali)
{
    destination = QDir::fromNativeSeparators(destination);
    apktool->disconnect();

    // Clear temporary directory;

//    emit loading(20, tr("Unpacking APK..."));
//    QDir(destination).removeRecursively();

    // Unpack APK:

    emit loading(50, tr("Unpacking APK..."));
    QStringList args;
    args << "-jar";
    args << apktoolPath;
    args << "d" << filepath;
    args << "-f";
    args << "--res-resolve-mode" << "greedy";
    if (!smali) { args << "-s"; }
    args << "-o" << destination;
    args << "-p" << frameworks;

    connect(apktool, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), [=](int code, QProcess::ExitStatus) {
        const int QPROCESS_KILL_CODE = 62097;
        switch (code) {
            case 0: {
                emit loading(70, tr("Reading AndroidManifest.xml..."));
                emit loading(72, tr("Checking split APK resources..."));
                const QStringList splitContentsPaths = unpackCompatibleSplits(filepath, destination, apktoolPath, frameworks);
                Apk::File *apk = new Apk::File(destination, splitContentsPaths);
                apk->setFilePath(filepath);
                qDebug() << "Done.\n";
                emit loading(100, tr("APK successfully loaded"));
                emit unpacked(apk);
                break;
            }
#ifdef Q_OS_OSX
            case 9:
#endif
            case QPROCESS_KILL_CODE:
                qDebug() << "Unpacking cancelled by user.";
                break;
            default: {
                const QString errorText = apktool->readAllStandardError().replace("\r\n", "\n");
                qDebug() << errorText;
                emit error(Apk::ERROR.arg("Apktool"), errorText);
                break;
            }
        }
    });

    connect(apktool, static_cast<void(QProcess::*)(QProcess::ProcessError)>(&QProcess::errorOccurred), [=](QProcess::ProcessError processError) {
        if (processError == QProcess::FailedToStart) {
            if (isJavaInstalled()) {
                const QString errorText = apktool->errorString();
                qDebug() << "Error starting Apktool";
                qDebug() << "Error:" << errorText;
                emit error(Apk::ERRORSTART.arg("Apktool"), errorText);
            } else {
                emit error(Apk::NOJAVA + "<br>" + Apk::GETJAVA);
            }
        }
    });

    apktool->start("java", args);
}

void Unpacker::cancel()
{
    apktool->kill();
}
