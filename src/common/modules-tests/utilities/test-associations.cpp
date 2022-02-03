#include "test-associations.h"

#include <QtTest/QtTest>
#include "utilities/associate_app.h"

using namespace utilities;

void Test_Associations::setAsDefaultTorrent()
{
	setDefaultTorrentApp();
	QVERIFY(isDefaultTorrentApp());
}

void Test_Associations::removeAsDefaultTorrent()
{
	unsetDefaultTorrentApp();
	QVERIFY(!isDefaultTorrentApp());
}


/********************** DECLARE TEST MAIN ****************************/
QTEST_MAIN(Test_Associations)
