#include <QtCore/QCoreApplication>

#include <iostream>

#include <Windows.h>

#include <signal.h>

#include "vld.h"


QCoreApplication* a;

void sigHandler(int s)
{
	if (a != 0)
	{
		a->exit(1);
	}
}


#include "tcpserver.h"
////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	signal(SIGINT, sigHandler);

	a = new QCoreApplication(argc, argv);

	std::cout << "START TCP SERVER PID: " << GetCurrentProcessId() << " TID: " << GetCurrentThreadId() << std::endl;

	tcpServer* tcpS = new tcpServer(a);
	QSharedPointer<tcpServer> ptcpS = QSharedPointer<tcpServer>(tcpS, &QObject::deleteLater);

	if (!tcpS->listen(QHostAddress::Any, 5103))
	{
		std::cout << "Error listen(): " << tcpS->errorString().toStdString().c_str() << std::endl;
	}

	int result = 1;
	try
	{
		result = a->exec();
		delete a;
		a = 0;
	}
	catch (...)
	{
		std::cout << "Exception caught";
	}

	return result;
}
////////////////////////////////////////////////////////////////////
