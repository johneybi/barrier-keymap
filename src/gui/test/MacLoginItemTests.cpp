#include "MacLoginItem.h"

#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

TEST(MacLoginItemTests, WritesRecognizableLaunchAgentAndRemovesIt)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const QString executablePath = directory.filePath("Barrier.app/Contents/MacOS/barrier");
    ASSERT_TRUE(QDir().mkpath(QFileInfo(executablePath).absolutePath()));
    QFile executable(executablePath);
    ASSERT_TRUE(executable.open(QIODevice::WriteOnly));
    executable.close();
    ASSERT_TRUE(executable.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
                                          QFile::ExeOwner));

    const QString agentPath = directory.filePath("LaunchAgents/barrier.plist");
    QString error;
    ASSERT_TRUE(MacLoginItem::setEnabledAt(agentPath, true, executablePath, &error))
        << error.toStdString();
    EXPECT_TRUE(MacLoginItem::isEnabledAt(agentPath, executablePath));
    EXPECT_FALSE(MacLoginItem::isEnabledAt(agentPath, directory.filePath("other")));

    ASSERT_TRUE(MacLoginItem::setEnabledAt(agentPath, false, executablePath, &error))
        << error.toStdString();
    EXPECT_FALSE(QFile::exists(agentPath));
}
