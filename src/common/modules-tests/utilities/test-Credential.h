#pragma once

#include <QObject>
class Test_Credential: public QObject
{
	Q_OBJECT
private slots:
	void initTestCase();
	void init();

	void default_constructor();
	void data_constructor();
	void assigment();
	void encrypt_decrypt();

	void cleanup();
	void cleanupTestCase();
};
