#pragma once

#include <QObject>
#include "scriptengine/jsengine.h"
#ifdef WITH_LUA
#include "scriptengine/luaengine.h"
#endif

class Test_Script
	: public QObject
{
	Q_OBJECT

public:
	Test_Script() {}

private Q_SLOTS:
	void initTestCase();

	void test_evaluateFunction();
	void test_evaluateNONFunction();
	void test_initstate();
	void test_exportlist();
	void test_loadfilelist();
	void test_qobjectmethodcall();
	void test_qobjectreflection();

	void cleanupTestCase();
private:
	ScriptEngine::JSEngine* m_jsengine;
#ifdef WITH_LUA
	ScriptEngine::LuaEngine* m_lua;
#endif
};

class ReflectTest : public QObject
{
	Q_OBJECT
public slots:
	int refTest()
	{
		return 25;
	}
};
