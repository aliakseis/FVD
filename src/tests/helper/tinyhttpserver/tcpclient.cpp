#include "tcpclient.h"
////////////////////////////////////////////////////////////////////
tcpClient::tcpClient(int _socketDescriptor, QObject* _parent) :
	QThread(_parent)
{
	std::cout << __FUNCTION__ << " " << GetCurrentThreadId() << " " << _socketDescriptor << std::endl;

	this->socketDescriptor = _socketDescriptor;

	this->moveToThread(this);
}
////////////////////////////////////////////////////////////////////
tcpClient::~tcpClient()
{
	std::cout << __FUNCTION__ << " " << GetCurrentThreadId() << std::endl;
}
////////////////////////////////////////////////////////////////////
void tcpClient::run()
{
	std::cout << "START TCP CLIENT THREAD PID: " << GetCurrentProcessId() << " TID: " << GetCurrentThreadId() << std::endl;

	QTcpSocket socket;

	socket.setSocketDescriptor(this->socketDescriptor);

	this->connect(&socket, SIGNAL(disconnected()), this, SLOT(quit()));

	this->exec();

	socket.close();

	std::cout << "END TCP CLIENT THREAD PID: " << GetCurrentProcessId() << " TID: " << GetCurrentThreadId() << std::endl;
}
////////////////////////////////////////////////////////////////////
int tcpClient::getSocketDescriptor()
{
	std::cout << __FUNCTION__ << " " << GetCurrentThreadId() << std::endl;

	return this->socketDescriptor;
}
////////////////////////////////////////////////////////////////////
