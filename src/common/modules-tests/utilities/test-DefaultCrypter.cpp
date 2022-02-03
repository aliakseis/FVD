#include "test-DefaultCrypter.h"


#include <iterator>
#include "utilities/default_crypter.h"

#include <QtTest>
#include <QString>

void Test_DefaultCrypter::initTestCase()
{
}

void Test_DefaultCrypter::init()
{
	pCr_ = new utilities::DefaultCrypter();
}

void Test_DefaultCrypter::default_constructor()
{
	QVERIFY(pCr_ != nullptr);
}

void Test_DefaultCrypter::empty_encrypt()
{
	QByteArray ar1, ar2;
	QVERIFY(pCr_->Encrypt(ar1, ar2));
	QVERIFY(ar2.isEmpty());
}

void Test_DefaultCrypter::empty_decrypt()
{
	QByteArray ar1, ar2;
	QVERIFY(pCr_->Decrypt(ar1, ar2));
	QVERIFY(ar2.isEmpty());
}

void Test_DefaultCrypter::encrypt_decrypt()
{
	QString orig_data = "Original data string";
	QString final_data;

	QByteArray ar1, ar2;

	QVERIFY(pCr_->Encrypt(orig_data.toUtf8(), ar1));
	QVERIFY(!ar1.isEmpty());

	QVERIFY(pCr_->Decrypt(ar1, ar2));
	QVERIFY(!ar2.isEmpty());

	final_data = QString::fromUtf8(ar2.constData(), ar2.size());
	QVERIFY(!final_data.isEmpty());

	QVERIFY(orig_data == final_data);
}


void Test_DefaultCrypter::static_encrypt_decrypt()
{
	QString orig_data = "Original data string";
	QString final_data;

	QByteArray ar1, ar2;

	QVERIFY(utilities::DefaultCrypter::Encrypt(orig_data.toUtf8(), ar1));
	QVERIFY(!ar1.isEmpty());

	QVERIFY(utilities::DefaultCrypter::Decrypt(ar1, ar2));
	QVERIFY(!ar2.isEmpty());

	final_data = QString::fromUtf8(ar2.constData(), ar2.size());
	QVERIFY(!final_data.isEmpty());

	QVERIFY(orig_data == final_data);
}

void Test_DefaultCrypter::cleanup()
{
	delete pCr_;
	pCr_ = nullptr;
}

void Test_DefaultCrypter::cleanupTestCase()
{
}

/********************** DECLARE_TEST LIST ****************************/
QTEST_MAIN(Test_DefaultCrypter)