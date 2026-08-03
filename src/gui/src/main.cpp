/*
 * barrier -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TRAY_RETRY_COUNT 5
#define TRAY_RETRY_WAIT 2000

#include "QBarrierApplication.h"
#include "MainWindow.h"
#include "AppConfig.h"
#include "SetupWizard.h"
#include "DisplayIsValid.h"

#include <QtCore>
#include <QtGui>
#include <QSettings>
#include <QMessageBox>
#include <QLockFile>
#include <QStandardPaths>

#ifdef Q_OS_DARWIN
#include <cstdlib>
#endif

class QThreadImpl : public QThread
{
public:
	static void msleep(unsigned long msecs)
	{
		QThread::msleep(msecs);
	}
};

int waitForTray();

int main(int argc, char* argv[])
{
#ifdef WINAPI_XWINDOWS
    // QApplication's constructor will call a fscking abort() if
    // DISPLAY is bad. Let's check it first and handle it gracefully
    if (!display_is_valid()) {
        fprintf(stderr, "The Barrier GUI requires a display. Quitting...\n");
        return 1;
    }
#endif
#ifdef Q_OS_DARWIN
    /* Workaround for QTBUG-40332 - "High ping when QNetworkAccessManager is instantiated" */
    ::setenv ("QT_BEARER_POLL_TIMEOUT", "-1", 1);
#endif
	QCoreApplication::setOrganizationName("Debauchee");
	QCoreApplication::setOrganizationDomain("github.com");
	QCoreApplication::setApplicationName("Barrier");

	QBarrierApplication app(argc, argv);

    const QString runtimeDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(runtimeDirectory);
    QLockFile instanceLock(QDir(runtimeDirectory).filePath("barrier-gui.lock"));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(100)) {
        QMessageBox::information(
            NULL, "Barrier", "Barrier is already running in the menu bar.");
        return 0;
    }

#if defined(Q_OS_MAC)
	if (app.applicationDirPath().startsWith("/Volumes/")) {
        // macOS preferences track applications allowed assistive access by path
        // Unfortunately, there's no user-friendly way to allow assistive access
        // to applications that are not in default paths (/Applications),
        // especially if an identically named application already exists in
        // /Applications). Thus we require Barrier to reside in the /Applications
        // folder
		QMessageBox::information(
			NULL, "Barrier",
			"Please drag Barrier to the Applications folder, and open it from there.");
		return 1;
	}

#endif

	int trayAvailable = waitForTray();

	QApplication::setQuitOnLastWindowClosed(false);

    if (QGuiApplication::platformName() == "wayland") {
        QMessageBox::warning(
        NULL, "Barrier",
        "You are using wayland session, which is currently not fully supported by Barrier.");
    }

	QSettings settings;
	AppConfig appConfig (&settings);
    const bool startInBackground = app.arguments().contains("--background");

	if (appConfig.getAutoHide() && !trayAvailable)
	{
		// force auto hide to false - otherwise there is no way to get the GUI back
		fprintf(stdout, "System tray not available, force disabling auto hide!\n");
		appConfig.setAutoHide(false);
	}

	app.switchTranslator(appConfig.language());

	MainWindow mainWindow(settings, appConfig);
	SetupWizard setupWizard(mainWindow, true);

	if (appConfig.wizardShouldRun())
	{
		setupWizard.show();
	}
	else
	{
		mainWindow.open(startInBackground);
	}

	return app.exec();
}

int waitForTray()
{
	// on linux, the system tray may not be available immediately after logging in,
	// so keep retrying but give up after a short time.
	int trayAttempts = 0;
	while (true)
	{
		if (QSystemTrayIcon::isSystemTrayAvailable())
		{
			break;
		}

		if (++trayAttempts > TRAY_RETRY_COUNT)
		{
			fprintf(stdout, "System tray is unavailable.\n");
			return false;
		}

		QThreadImpl::msleep(TRAY_RETRY_WAIT);
	}
	return true;
}
