#include "test-Crc32.h"

#include <iterator>
#include "utilities/crc_32.h"

#include <QtTest>
#include <QString>

void Test_Crc32::initTestCase()
{
}

void Test_Crc32::init()
{
	pCrc32_ = new utilities::Crc32Calc();
}

void Test_Crc32::default_constructor()
{
	QVERIFY(pCrc32_ != nullptr);
	QVERIFY(pCrc32_->value() == 0xFFFFFFFF);
}

void Test_Crc32::reset()
{
	pCrc32_ ->reset();
	QVERIFY(pCrc32_->value() == 0xFFFFFFFF);
}

void Test_Crc32::empty_string()
{
	pCrc32_ ->compute("", 0);
	QVERIFY(pCrc32_->value() == 0xFFFFFFFF);
}

void Test_Crc32::lowercase_string()
{
	pCrc32_ ->compute("test");
	unsigned int res1 = pCrc32_->value();
	QVERIFY(res1 != 0xFFFFFFFF);

	pCrc32_ ->reset();
	QVERIFY(pCrc32_->value() == 0xFFFFFFFF);

	pCrc32_ ->computeFromLowerCase("TEST");
	unsigned int res2 = pCrc32_->value();
	QVERIFY(res1 == res2);

}

void Test_Crc32::cleanup()
{
	delete pCrc32_;
	pCrc32_ = nullptr;
}

void Test_Crc32::cleanupTestCase()
{
}

/********************** DECLARE_TEST LIST ****************************/
QTEST_MAIN(Test_Crc32)