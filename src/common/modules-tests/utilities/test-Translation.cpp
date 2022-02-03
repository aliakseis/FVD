#include "test-Translation.h"

#include <iterator>
#include "utilities/translation.h"

#include <QtTest/QtTest>
#include <QString>



void Test_Translation::locationString_data()
{
	QTest::addColumn<QString>("file_name");
	QTest::addColumn<QString>("location_str");

	QTest::newRow("empty") << "" << "";

	QTest::newRow("ilivid_en.qm") << "ilivid_en.qm" << "en";
	QTest::newRow("c:/ilivid_en.qm") << "c:/ilivid_en.qm" << "en";
	QTest::newRow("C:/ILIVID_EN.QM") << "C:/ILIVID_EN.QM" << "en";

	QTest::newRow("ilivid_en.ts") << "ilivid_en.ts" << "";
	QTest::newRow("c:/ilivid_en.ts") << "c:/ilivid_en.ts" << "";

	QTest::newRow("ilivid_en") << "ilivid_en" << "";
	QTest::newRow("ilivid_en.ts.qm") << "ilivid_en.ts.qm" << "";
}

void Test_Translation::locationString()
{
	QFETCH(QString, file_name);
	QFETCH(QString, location_str);

	QCOMPARE(utilities::locationString(file_name), location_str);
}

/********************** DECLARE TEST MAIN ****************************/
QTEST_MAIN(Test_Translation)