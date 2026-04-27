#include "apk.h"
#include "globals.h"
#include "settings.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

QString javaExecutable(Java java)
{
    const QString name = java == JRE ? "java" : "javac";
    const QString fromPath = QStandardPaths::findExecutable(name);
    if (!fromPath.isEmpty()) {
        return fromPath;
    }

    const QString javaHome = qgetenv("JAVA_HOME");
    if (!javaHome.isEmpty()) {
        const QString candidate = QDir(javaHome).filePath("bin/" + name);
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

#ifdef Q_OS_OSX
    QProcess javaHomeProcess;
    javaHomeProcess.start("/usr/libexec/java_home", QStringList());
    if (javaHomeProcess.waitForFinished(3000) && javaHomeProcess.exitCode() == 0) {
        const QString detectedHome = QString::fromUtf8(javaHomeProcess.readAllStandardOutput()).trimmed();
        const QString candidate = QDir(detectedHome).filePath("bin/" + name);
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    if (java == JRE) {
        const QString pluginJava = "/Library/Internet Plug-Ins/JavaAppletPlugin.plugin/Contents/Home/bin/java";
        if (QFileInfo::exists(pluginJava)) {
            return pluginJava;
        }
    }
#endif

    return QString();
}

}

bool Apk::whichJava(Java java)
{
    return !javaExecutable(java).isEmpty();
}

QString Apk::getApktoolVersion()
{
    if (whichJava(JRE)) {
        QProcess p;
        p.start(javaExecutable(JRE), QStringList() << "-jar" << Settings::get_apktool() << "-version");
        if (p.waitForStarted(-1)) {
            p.waitForFinished(-1);
            return p.readAllStandardOutput().trimmed();
        }
        else {
            return QString();
        }
    }
    else {
        return QString();
    }
}

QString Apk::getJavaVersion(Java java)
{
    if (whichJava(java)) {
        QProcess p;
        p.start(javaExecutable(java), QStringList() << "-version");
        if (p.waitForStarted(-1)) {
            p.waitForFinished(-1);
            QString VERSION = p.readAllStandardError().replace("\r\n", "\n").trimmed();
            if (VERSION.isEmpty()) {
                VERSION = p.readAllStandardOutput().replace("\r\n", "\n").trimmed();
            }
            return VERSION;
        }
        else {
            return QString();
        }
    }
    else {
        return QString();
    }
}

QString Apk::getJreVersion() { return getJavaVersion(JRE); }
QString Apk::getJdkVersion() { return getJavaVersion(JDK); }
bool Apk::isJavaInstalled()  { return !(getJreVersion().isNull() && getJdkVersion().isNull()); }
