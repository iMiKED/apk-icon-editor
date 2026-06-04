#include "apkunpacker.h"
#include "manifest.h"
#include "globals.h"
#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

using Apk::Unpacker;

namespace {

static QDomAttr attributeByLocalName(const QDomElement &element, const QString &name)
{
    QDomElement node = element;
    QDomAttr attr = node.attributeNode(name);
    if (!attr.isNull()) {
        return attr;
    }

    const QDomNamedNodeMap attrs = element.attributes();
    for (int i = 0; i < attrs.count(); ++i) {
        const QDomAttr candidate = attrs.item(i).toAttr();
        if (candidate.name().section(':', -1) == name) {
            return candidate;
        }
    }
    return QDomAttr();
}

static QString attributeValue(const QDomElement &element, const QString &name)
{
    const QDomAttr attr = attributeByLocalName(element, name);
    return attr.isNull() ? QString() : attr.value();
}

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

    return attributeValue(manifest, name);
}

static bool isTruthyManifestValue(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == "true" || normalized == "1";
}

static QStringList xmlResourcePaths(const QString &baseContentsPath, const QString &resourceRef)
{
    const QString prefix = "@xml/";
    if (!resourceRef.startsWith(prefix)) {
        return QStringList();
    }

    const QString name = resourceRef.mid(prefix.length()).trimmed();
    if (name.isEmpty() || name.contains('/')) {
        return QStringList();
    }

    QStringList result;
    const QString resPath = QDir::cleanPath(baseContentsPath + "/res");
    QDirIterator it(resPath, QStringList() << (name + ".xml"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString dirName = QFileInfo(it.fileInfo().dir().path()).fileName();
        if (dirName == "xml" || dirName.startsWith("xml-")) {
            result << QDir::cleanPath(it.filePath());
        }
    }
    result.removeDuplicates();
    return result;
}

static bool splitsXmlDeclaresNamedSplits(const QString &xmlPath)
{
    QFile file(xmlPath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return false;
    }

    QDomDocument doc;
    if (!doc.setContent(file.readAll())) {
        return false;
    }

    const QDomNodeList entries = doc.elementsByTagName("entry");
    for (int i = 0; i < entries.count(); ++i) {
        const QDomElement entry = entries.at(i).toElement();
        if (!attributeValue(entry, "split").trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

static bool splitsMetadataDeclaresNamedSplits(const QString &baseContentsPath, const QString &resourceRef)
{
    const QStringList paths = xmlResourcePaths(baseContentsPath, resourceRef);
    if (paths.isEmpty()) {
        return false;
    }

    foreach (const QString &path, paths) {
        if (splitsXmlDeclaresNamedSplits(path)) {
            return true;
        }
    }

    qDebug().noquote() << "Split APK metadata ignored: split entries are empty in" << QDir::toNativeSeparators(paths.first());
    return false;
}

static bool manifestRequestsSplitResources(const QString &manifestPath, const QString &baseContentsPath)
{
    QFile file(manifestPath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return false;
    }

    QDomDocument doc;
    if (!doc.setContent(file.readAll())) {
        return false;
    }

    const QDomElement manifest = doc.firstChildElement("manifest");
    if (manifest.isNull()) {
        return false;
    }

    if (!attributeValue(manifest, "requiredSplitTypes").isEmpty()
        || !attributeValue(manifest, "splitTypes").isEmpty()
        || isTruthyManifestValue(attributeValue(manifest, "isSplitRequired"))
        || isTruthyManifestValue(attributeValue(manifest, "isolatedSplits"))) {
        return true;
    }

    const QDomElement application = manifest.firstChildElement("application");
    if (application.isNull()) {
        return false;
    }

    if (isTruthyManifestValue(attributeValue(application, "isSplitRequired"))
        || isTruthyManifestValue(attributeValue(application, "isolatedSplits"))) {
        return true;
    }

    for (QDomElement meta = application.firstChildElement("meta-data"); !meta.isNull(); meta = meta.nextSiblingElement("meta-data")) {
        const QString name = attributeValue(meta, "name");
        if (name == "com.android.vending.splits.required" && isTruthyManifestValue(attributeValue(meta, "value"))) {
            return true;
        }
        if (name == "com.android.vending.splits") {
            const QString resource = attributeValue(meta, "resource");
            if (!resource.isEmpty()) {
                if (splitsMetadataDeclaresNamedSplits(baseContentsPath, resource)) {
                    return true;
                }
            } else if (!attributeValue(meta, "value").isEmpty()) {
                return true;
            }
        }
    }

    return false;
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

static bool isLikelySplitApkName(const QString &fileName)
{
    const QString baseName = QFileInfo(fileName).completeBaseName().toLower();
    return baseName.startsWith("config.")
           || baseName.startsWith("split_config.")
           || baseName.startsWith("split.")
           || baseName.contains(".config.")
           || baseName.contains("_config.")
           || baseName.endsWith(".split")
           || baseName.endsWith("_split");
}

static QStringList unpackCompatibleSplits(const QString &baseApkPath, const QString &baseContentsPath, const QString &apktoolPath, const QString &frameworks)
{
    QStringList result;
    const QString basePackage = manifestAttribute(QDir::cleanPath(baseContentsPath + "/AndroidManifest.xml"), "package");
    if (basePackage.isEmpty()) {
        return result;
    }

    if (!manifestRequestsSplitResources(QDir::cleanPath(baseContentsPath + "/AndroidManifest.xml"), baseContentsPath)) {
        qDebug().noquote() << "Split APK check skipped: base manifest does not declare split resource metadata.";
        return result;
    }

    const QFileInfo baseInfo(baseApkPath);
    QDir dir(baseInfo.dir());
    const QFileInfoList candidates = dir.entryInfoList(QStringList() << "*.apk", QDir::Files, QDir::Name);
    if (candidates.count() <= 1) {
        return result;
    }

    QFileInfoList splitCandidates;
    foreach (const QFileInfo &candidate, candidates) {
        if (candidate.canonicalFilePath() == baseInfo.canonicalFilePath()) {
            continue;
        }
        if (isLikelySplitApkName(candidate.fileName())) {
            splitCandidates << candidate;
        }
    }
    if (splitCandidates.isEmpty()) {
        qDebug().noquote() << "Split APK check skipped: no split-like sibling APK filenames found.";
        return result;
    }

    const QString splitsRoot = QDir::cleanPath(baseContentsPath + "/_splits");
    QDir().mkpath(splitsRoot);
    int checked = 0;
    int skipped = 0;
    foreach (const QFileInfo &candidate, splitCandidates) {
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
                emit loading(84, tr("Building icon list..."));
                QApplication::processEvents();
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
