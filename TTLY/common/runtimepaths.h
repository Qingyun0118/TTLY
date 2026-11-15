#ifndef TTLY_RUNTIMEPATHS_H
#define TTLY_RUNTIMEPATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace TTLYRuntime {

inline QString rootAbove(const QString &start)
{
    QDir directory(start);
    do {
        if (QFileInfo(directory.filePath("Sound_Rec/soundimprove.py")).isFile()) {
            return directory.absolutePath();
        }
    } while (directory.cdUp());
    return QString();
}

inline QString projectRoot()
{
    const QString configured = qEnvironmentVariable("TTLY_ROOT");
    if (!configured.isEmpty()) {
        return QDir(configured).absolutePath();
    }

    const QStringList starts = {QCoreApplication::applicationDirPath(), QDir::currentPath()};
    for (const QString &start : starts) {
        const QString root = rootAbove(start);
        if (!root.isEmpty()) {
            return root;
        }
    }

    // qmake records the source root so an out-of-tree build can start anywhere.
#ifdef TTLY_SOURCE_ROOT
    return QDir(QString::fromUtf8(TTLY_SOURCE_ROOT)).absolutePath();
#else
    return QCoreApplication::applicationDirPath();
#endif
}

inline QString path(const QString &relativePath)
{
    return QDir(projectRoot()).absoluteFilePath(relativePath);
}

inline QString pythonExecutable()
{
    const QString configured = qEnvironmentVariable("TTLY_PYTHON");
    if (!configured.isEmpty()) {
        return configured;
    }

#ifdef Q_OS_WIN
    const QString localPython = path(".venv/Scripts/python.exe");
#else
    const QString localPython = path(".venv/bin/python");
#endif
    if (QFileInfo(localPython).isFile() && QFileInfo(localPython).isExecutable()) {
        return localPython;
    }

    for (const QString &name : QStringList{"python3", "python"}) {
        const QString executable = QStandardPaths::findExecutable(name);
        if (!executable.isEmpty()) {
            return executable;
        }
    }
    return QString();
}

} // namespace TTLYRuntime

#endif // TTLY_RUNTIMEPATHS_H
