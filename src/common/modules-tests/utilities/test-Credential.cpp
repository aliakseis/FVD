#include "test-Credential.h"

#include <iterator>
#include "utilities/credential.h"
#include "utilities/default_crypter.h"

#include <QString>
#include <QTest>

void Test_Credential::initTestCase()
{
}

void Test_Credential::init()
{
}

void Test_Credential::default_constructor()
{
	utilities::Credential* pCred_ = new utilities::Credential();

	QVERIFY(pCred_ != nullptr);
	QVERIFY(ValidCredential(*pCred_) == false);

	QVERIFY(pCred_->persistent == true);
	QVERIFY(pCred_->host == QString());
	QVERIFY(pCred_->login == QString());
	QVERIFY(pCred_->password == QString());

	delete pCred_;
	pCred_ = nullptr;
}

void Test_Credential::data_constructor()
{
	utilities::Credential* pCred_ = new utilities::Credential("aaa", "user", "password");

	QVERIFY(pCred_ != nullptr);
	QVERIFY(pCred_->persistent == true);
	QVERIFY(ValidCredential(*pCred_) == true);

	QVERIFY(pCred_->persistent == true);
	QVERIFY(pCred_->host == "aaa");
	QVERIFY(pCred_->login == "user");
	QVERIFY(pCred_->password == "password");

	delete pCred_;
	pCred_ = nullptr;
}


void Test_Credential::assigment()
{
	utilities::Credential* pCred_ = new utilities::Credential("aaa", "user1", "password1");

	QVERIFY(ValidCredential(*pCred_) == true);
	QVERIFY(*pCred_ == "aaa");

	delete pCred_;
	pCred_ = nullptr;

}


void Test_Credential::encrypt_decrypt()
{
	utilities::Credential* pCred_1 = new utilities::Credential("aaa", "user", "password");

	QVERIFY(ValidCredential(*pCred_1) == true);
	QString str_data = utilities::GetEncryptedLoginPassword<utilities::DefaultCrypter>(*pCred_1);
	QVERIFY(!str_data.isEmpty());

	utilities::Credential* pCred_2 = new utilities::Credential();
	QVERIFY(utilities::InitFromEncrypted<utilities::DefaultCrypter>("aaa", str_data, pCred_2));
	QVERIFY(pCred_1->host == pCred_2->host);
	QVERIFY(pCred_1->login == pCred_2->login);
	QVERIFY(pCred_1->password == pCred_2->password);


	delete pCred_1;
	pCred_1 = nullptr;

	delete pCred_2;
	pCred_2 = nullptr;


}

void Test_Credential::cleanup()
{
	//	delete pCred_;
	//	pCred_ = nullptr;
}

void Test_Credential::cleanupTestCase()
{
}

/********************** DECLARE_TEST LIST ****************************/
QTEST_MAIN(Test_Credential)
