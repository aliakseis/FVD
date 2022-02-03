#include "tcpserver.h"
////////////////////////////////////////////////////////////////////
tcpServer::tcpServer(QObject* _parent) :
	QTcpServer(_parent)
{
}
////////////////////////////////////////////////////////////////////
void tcpServer::incomingConnection(int _socketDescriptor)
{
	std::cout << __FUNCTION__ << " " << GetCurrentThreadId() << " " << _socketDescriptor << std::endl;

	tcpClient* tcpCl = NULL;
	try
	{
		tcpCl = new tcpClient(_socketDescriptor);
	}
	catch (std::bad_alloc& _e)
	{
		std::cout << "std::bad_alloc caught: " << _e.what() << std::endl;
		return;
	}
	this->connect(tcpCl, SIGNAL(finished()), this, SLOT(clientDisconnected()));

	tcpCl->start();
}
////////////////////////////////////////////////////////////////////
void tcpServer::clientDisconnected()
{
	std::cout << __FUNCTION__ << " " << GetCurrentThreadId() << std::endl;

	tcpClient* tcpCl = qobject_cast<tcpClient*>(sender());

	delete tcpCl;
}
////////////////////////////////////////////////////////////////////
