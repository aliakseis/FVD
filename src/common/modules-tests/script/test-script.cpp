#ifndef QT_WIDGETS_LIB
#define QT_WIDGETS_LIB
#endif
#ifndef QT_GUI_LIB
#define QT_GUI_LIB
#endif

#include "test-script.h"

#include <QtTest>

using namespace ScriptEngine;

void Test_Script::initTestCase()
{
	m_jsengine = new JSEngine();
#ifdef WITH_LUA
	m_lua = new LuaEngine();
#endif
}

void Test_Script::cleanupTestCase()
{
	delete m_jsengine;
#ifdef WITH_LUA
	delete m_lua;
#endif
}

void Test_Script::test_evaluateFunction()
{
	m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData());
	QVariant result = m_jsengine->invokeFunction("multiplyValues", QVariantList() << 3 << 4);
	QCOMPARE(result.toInt(), 12);
	QVariant result2 = m_jsengine->invokeFunction("Myltiplier", "multiplyValues", QVariantList() << 3 << 4);
	QCOMPARE(result2.toInt(), 12);

#ifdef WITH_LUA
	m_lua->loadFile((QCoreApplication::applicationDirPath() + "/script.lua").toUtf8().constData());
	QVariant result4 = m_lua->invokeFunction("Test", "pow2", QVariantList() << 3 << 4);
	QCOMPARE(result4.toInt(), 12);
	QVariant result3 = m_lua->invokeFunction("pow2f", QVariantList() << 3 << 4);
	QCOMPARE(result3.toInt(), 12);
#endif
}


void Test_Script::test_initstate()
{
	JSEngine* enjnew = new JSEngine;
	QVERIFY(enjnew ->m_scriptEngine.isNull());
	enjnew ->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData());
	QVERIFY(!enjnew->m_scriptEngine.isNull());
	delete enjnew;
}

void Test_Script::test_exportlist()
{
	m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData());
	m_jsengine->exportVariable("expo", 12);
	QVariant result = m_jsengine->invokeFunction("checkexport");
	QCOMPARE(result.toInt(), 12);

#ifdef WITH_LUA
	m_lua->loadFile((QCoreApplication::applicationDirPath() + "/script.lua").toUtf8().constData());
	m_lua->exportVariable("expo", 14);
	QVariant resultl = m_lua->invokeFunction("checkexport");
	QCOMPARE(resultl.toInt(), 14);
#endif
}

void Test_Script::test_evaluateNONFunction()
{
	m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData());
	QVariant result = m_jsengine->invokeFunction("nonexist", QVariantList() << 3 << 4);
	QCOMPARE(result, QVariant());
	result = m_jsengine->invokeFunction("Myltiplier", "nonexist", QVariantList() << 3 << 4);
	QCOMPARE(result, QVariant());
	result = m_jsengine->invokeFunction("somevar", QVariantList() << 3 << 4);
	QCOMPARE(result, QVariant());

#ifdef WITH_LUA
	m_lua->loadFile((QCoreApplication::applicationDirPath() + "/script.lua").toUtf8().constData());
	QVariant result4 = m_lua->invokeFunction("Test", "nonexist", QVariantList() << 3 << 4);
	QCOMPARE(result4, QVariant());
	QVariant result3 = m_lua->invokeFunction("nonexist", QVariantList() << 3 << 4);
	QCOMPARE(result3, QVariant());
#endif
}

void Test_Script::test_loadfilelist()
{
	QVERIFY(m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData()));
	QVERIFY(!m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/wrongsyntax.js").toUtf8().constData()));
	QVERIFY(!m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/nonexist.js").toUtf8().constData()));

#ifdef WITH_LUA
	QVERIFY(m_lua->loadFile((QCoreApplication::applicationDirPath() + "/script.lua").toUtf8().constData()));
	QVERIFY(!m_lua->loadFile((QCoreApplication::applicationDirPath() + "/wrongsyntax.lua").toUtf8().constData()));
	QVERIFY(!m_lua->loadFile((QCoreApplication::applicationDirPath() + "/nonexist.lua").toUtf8().constData()));
#endif
}

void Test_Script::test_qobjectmethodcall()
{
	m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData());
	QObject* obj = new QObject();
	obj->setObjectName("testobject");
	QCOMPARE(m_jsengine->invokeFunction("checkobject", QVariantList() << QVariant::fromValue<QObject*>(obj)), QVariant("testobject"));
	delete obj;
}

void Test_Script::test_qobjectreflection()
{
	m_jsengine->loadFile((QCoreApplication::applicationDirPath() + "/script.js").toUtf8().constData());
	ReflectTest* obj = new ReflectTest();
	m_jsengine->exportVariable("expo2", QVariant::fromValue<QObject*>(obj));
	QCOMPARE(m_jsengine->invokeFunction("reflectionTest"), QVariant(25));
	delete obj;
}

QTEST_MAIN(Test_Script)
