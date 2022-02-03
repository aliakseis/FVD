#pragma once

#include <QObject>
namespace utilities {class Crc32Calc;}
class Test_Crc32: public QObject
{
	Q_OBJECT
	utilities::Crc32Calc* pCrc32_;

private slots:
	void initTestCase();
	void init();

	void default_constructor();
	void reset();
	void empty_string();
	void lowercase_string();

	void cleanup();
	void cleanupTestCase();
};