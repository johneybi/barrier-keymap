#pragma once

#include <QString>

class MacLoginItem
{
public:
    static QString launchAgentPath();
    static bool isEnabled(const QString& executablePath);
    static bool setEnabled(bool enabled, const QString& executablePath,
                           QString* errorMessage);
    static bool isEnabledAt(const QString& path, const QString& executablePath);
    static bool setEnabledAt(const QString& path, bool enabled,
                             const QString& executablePath, QString* errorMessage);
};
