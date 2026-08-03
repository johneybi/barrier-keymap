#include "MacLoginItem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

const char kLaunchAgentLabel[] = "org.barrier-foss.barrier-keymap";

QStringList readProgramArguments(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringList();
    }

    QXmlStreamReader xml(&file);
    QString key;
    QStringList arguments;
    bool readingArguments = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (readingArguments && xml.isEndElement() &&
            xml.name() == QLatin1String("array")) {
            readingArguments = false;
            continue;
        }
        if (!xml.isStartElement()) {
            continue;
        }

        if (xml.name() == QLatin1String("key")) {
            key = xml.readElementText();
        }
        else if (xml.name() == QLatin1String("array") &&
                 key == QLatin1String("ProgramArguments")) {
            readingArguments = true;
        }
        else if (readingArguments && xml.name() == QLatin1String("string")) {
            arguments.append(xml.readElementText());
        }
    }

    return xml.hasError() ? QStringList() : arguments;
}

} // namespace

QString MacLoginItem::launchAgentPath()
{
    return QDir::home().filePath(
        QStringLiteral("Library/LaunchAgents/%1.plist").arg(kLaunchAgentLabel));
}

bool MacLoginItem::isEnabled(const QString& executablePath)
{
    return isEnabledAt(launchAgentPath(), executablePath);
}

bool MacLoginItem::isEnabledAt(const QString& path, const QString& executablePath)
{
    const QStringList arguments = readProgramArguments(path);
    return arguments.size() == 2 &&
           QFileInfo(arguments.at(0)).canonicalFilePath() ==
               QFileInfo(executablePath).canonicalFilePath() &&
           arguments.at(1) == QLatin1String("--background");
}

bool MacLoginItem::setEnabled(bool enabled, const QString& executablePath,
                              QString* errorMessage)
{
    return setEnabledAt(launchAgentPath(), enabled, executablePath, errorMessage);
}

bool MacLoginItem::setEnabledAt(const QString& path, bool enabled,
                                const QString& executablePath,
                                QString* errorMessage)
{
    if (!enabled) {
        if (!QFile::exists(path) || QFile::remove(path)) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not remove %1").arg(path);
        }
        return false;
    }

    const QFileInfo executable(executablePath);
    if (!executable.isExecutable()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Application executable is not available: %1")
                                .arg(executablePath);
        }
        return false;
    }

    QDir directory(QFileInfo(path).absolutePath());
    if (!directory.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create %1").arg(directory.path());
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeDTD(QStringLiteral(
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));
    xml.writeStartElement(QStringLiteral("plist"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("dict"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("Label"));
    xml.writeTextElement(QStringLiteral("string"), QLatin1String(kLaunchAgentLabel));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("ProgramArguments"));
    xml.writeStartElement(QStringLiteral("array"));
    xml.writeTextElement(QStringLiteral("string"), executable.absoluteFilePath());
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("--background"));
    xml.writeEndElement();
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("RunAtLoad"));
    xml.writeEmptyElement(QStringLiteral("true"));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();

    if (!file.commit()) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    return true;
}
