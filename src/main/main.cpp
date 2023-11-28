#include <QFile>
#include <QDebug>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QIODevice>

#include "mainwindow.h"
#include "application.h"
#include "faenzastyle.h"
#include "settings_declaration.h"
#include "global_functions.h"
#include "globals.h"
#include "version.hxx"

#include "utilities/logger.h"


#include "searchmanager.h"
#include "configurableproxyfactory.h"


#include "imagecache.h"


#ifdef Q_OS_MAC
#include "darwin/AppHandler.h"
#endif //Q_OS_MAC


#ifdef DEVELOPER_FEATURES

#include <signal.h>

void sigHandler(int)
{
	QCoreApplication* a = QApplication::instance();
	if (a != 0)
	{
		a->exit(1);
	}
}

#endif // DEVELOPER_FEATURES

// Called once QCoreApplication exists
static void ApplicationDependentInitialization()
{
    if (utilities::IsPortableMode())
    {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
            QCoreApplication::applicationDirPath());
}

    utilities::setWriteToLogFile(
#if defined(DEVELOPER_FEATURES) && defined(_DEBUG)
		true
#else
		QSettings().value(app_settings::LoggingEnabled, false).toBool()
#endif
	);
}

Q_COREAPP_STARTUP_FUNCTION(ApplicationDependentInitialization)

int main(int argc, char* argv[])
{
#ifdef DEVELOPER_FEATURES
	signal(SIGINT, sigHandler);
#endif // DEVELOPER_FEATURES

    utilities::InitializeProjectDescription();

	if (4 == argc && 0 == strcmp(GET_RANDOM_FRAME_OPTION, argv[1]))
	{
		QApplication app(argc, argv, 0);
		return getRandomFrame();
	}


	QNetworkProxyFactory::setApplicationProxyFactory(new ConfigurableProxyFactory());

	Application app(argc, argv);
	if (app.isRunning())
	{
        qDebug() << "Another application is already running; exiting ...";
		app.sendMessage("");
		return 0;
	}

    qDebug() << "Starting " PROJECT_FULLNAME " version " PROJECT_VERSION " built " __DATE__ " " __TIME__;


#ifndef DEVELOPER_FEATURES
	Application::setQuitOnLastWindowClosed(false);
#endif

	Application::setStyle(new FaenzaStyle);
    app.retranslateApp(QSettings().value(app_settings::ln, app_settings::ln_Default).toString());

	/* Apply stylesheet */
	QFile css_data(":/style.css");
	if (css_data.open(QIODevice::ReadOnly))
	{
		app.setStyleSheet(css_data.readAll());
		css_data.close();
	}
	MainWindow w;
#ifdef Q_OS_MAC
	Darwin::SetApplicationHandler(&w);
#endif

	w.show();
	app.setActivationWindow(&w);

	int retcode = Application::exec();

	qDebug() << "Exiting " PROJECT_NAME;

	QNetworkProxyFactory::setApplicationProxyFactory(nullptr);

	return retcode;
}
