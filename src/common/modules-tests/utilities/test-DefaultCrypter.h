#pragma once

#include <QObject>
namespace utilities {class DefaultCrypter;}
class Test_DefaultCrypter: public QObject
{
	Q_OBJECT
	utilities::DefaultCrypter* pCr_;
private slots:
	void initTestCase();
	void init();

	void default_constructor();
	void empty_encrypt();
	void empty_decrypt();
	void encrypt_decrypt();
	void static_encrypt_decrypt();

	void cleanup();
	void cleanupTestCase();
};