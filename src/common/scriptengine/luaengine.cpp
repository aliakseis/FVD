#ifdef WITH_LUA

#include "luaengine.h"
#include <cstring>

#ifndef _MSC_VER
#define each
#define in :
#endif

namespace ScriptEngine
{

int LuaEngine::status;

LuaEngine::LuaEngine() : L(lua_open())
{
	luaL_openlibs(L);
}

LuaEngine::~LuaEngine()
{
	lua_close(L);
}

void LuaEngine::call_thread(const LuaEventContext& context)
{
	bool withtable = (context.table != NULL);

	if (withtable)
	{
		lua_getglobal(context.L, context.table); // args + 1
		lua_pushstring(context.L, context.method); // args + 2
	}
	else
	{
		lua_getglobal(context.L, context.method); // args + 1
	}

	if (withtable)
	{
		lua_gettable(context.L, -2); // args + 2
		lua_remove(context.L, -2); // args + 1
	}

	lua_insert(context.L, 1); // args + 1 (смещаем на 1 вниз)
	LuaEngine::status = lua_pcall(context.L, context.args, context.ret, 0);
	reportError(context.L);
}

#ifdef Q_OS_WIN32
void LuaEngine::_CallThread::run()
{
	call_thread(this->context);
}
#endif

QVariant LuaEngine::invokeFunction(const char* object, const char* method, const QVariantList& arguments /*= QVariantList()*/, QLSCallback callback /*= nullptr*/)
{
	lua_getglobal(L, object); // args + 1
	lua_pushstring(L, method); // args + 2
	if (!lua_istable(L, -2))
	{
		lua_settop(L, 0);
		return QVariant();
	}
	lua_gettable(L, -2);
	if (!lua_isfunction(L, -1))
	{
		lua_settop(L, 0);
		return QVariant();
	}
	lua_settop(L, 0);

	int argc = arguments.count();
	for each(const QVariant & argv in arguments)
	{
		luaPush(argv);
	}

	LuaEventContext context = {L, object, method, argc, 1};
#ifndef Q_OS_WIN32
	std::thread thread(call_thread_with_table, context);
	if (!async)
	{
		thread.join();
	}
#else
	call_thread_q.context = context;
	call_thread_q.start();
	if (callback == nullptr)
	{
		call_thread_q.wait();
	}
#endif

	return luaPopVariant();
}

QVariant LuaEngine::invokeFunction(const char* function, const QVariantList& arguments /* = QVariantList() */, QLSCallback callback /* = nullptr */)
{
	lua_getglobal(L, function);
	if (!lua_isfunction(L, -1))
	{
		lua_settop(L, 0);
		return QVariant();
	}
	lua_settop(L, 0);

	int argc = arguments.count();
	for each(const QVariant & argv in arguments)
	{
		luaPush(argv);
	}

	LuaEventContext context = {L, NULL, function, argc, 1};

#ifndef Q_OS_WIN32
	std::thread thread(call_thread_with_table, context);
	if (!async)
	{
		thread.join();
	}
#else
	call_thread_q.context = context;
	call_thread_q.start();
	if (callback == nullptr)
	{
		call_thread_q.wait();
	}
#endif

	return luaPopVariant();
}

void LuaEngine::registerFunction(const char* name, lua_CFunction func)
{
	//Log(std::string("[LUA][REGISTER]") + name, Log::DEBUG);
	lua_register(L, name, func);
}

bool LuaEngine::loadFile(const char* filename)
{
	//Log(std::string("[LUA][LOAD]") + filename, Log::DEBUG);
	return (loadFileStatus = luaL_dofile(L, filename)) == 0;
}

void LuaEngine::reload()
{
	// Full reload
	lua_close(L);
	L = lua_open();
	luaL_openlibs(L);
}

void LuaEngine::reportError(lua_State* L)
{
	if (LuaEngine::status != 0)
	{
		//Log(std::string("[LUA][ERROR]") + lua_tostring(L, -1), Log::WARNING);
		lua_pop(L, 1);
	}
}


void LuaEngine::luaPush(int i)
{
	lua_pushnumber(L, i);
}

void LuaEngine::luaPush(bool b)
{
	lua_pushboolean(L, b);
}

void LuaEngine::luaPush(const std::string& str)
{
	lua_pushlstring(L, str.data(), str.size());
}

void LuaEngine::luaPush(const char* str)
{
	lua_pushlstring(L, str, strlen(str));
}

void LuaEngine::luaPush(void* func)
{
	lua_pushlightuserdata(L, func);
}

void LuaEngine::luaPush(const std::map< std::string, std::string >& map)
{
	lua_newtable(L);
	for each(std::pair<std::string, std::string> row in map)
	{
		lua_pushstring(L, row.first.c_str());
		lua_pushstring(L, row.second.c_str());
		lua_settable(L, -3);
	}
}

void LuaEngine::luaPush(const QString& string)
{
	luaPush(string.toStdString());
}

void LuaEngine::luaPush(const QVariant& variant)
{
	switch (variant.type())
	{
	case QVariant::Bool:
	{
		luaPush(variant.toBool());
		break;
	}
	case QVariant::String:
	{
		luaPush(variant.toString());
		break;
	}
	case QVariant::Int:
	{
		luaPush(variant.toInt());
		break;
	}
	case QVariant::Map:
	{
		luaPush(variant.toMap());
		break;
	}
	case QVariant::Hash:
	{
		luaPush(variant.toHash());
		break;
	}
	default:
		break;
	}
}

void LuaEngine::luaPush(const QHash<QString, QVariant>& hash)
{
	lua_newtable(L);
	for (QHash<QString, QVariant>::const_iterator it = hash.constBegin(); it != hash.constEnd(); ++it)
	{
		luaPush(it.key());
		luaPush(it.value());
		lua_settable(L, -3);
	}
}

void LuaEngine::luaPush(const QMap<QString, QVariant>& hash)
{
	lua_newtable(L);
	for (QMap<QString, QVariant>::const_iterator it = hash.constBegin(); it != hash.constEnd(); ++it)
	{
		luaPush(it.key());
		luaPush(it.value());
		lua_settable(L, -3);
	}
}

double LuaEngine::luaPopNumber(void)
{
	double back;
	if (lua_isnumber(L, -1))
	{
		back = lua_tonumber(L, -1);
	}
	lua_pop(L, 1);
	return back;
}

std::string LuaEngine::luaPopString(void)
{
	std::string back;
	if (lua_isstring(L, -1))
	{
		back = lua_tostring(L, -1);
	}
	lua_pop(L, 1);
	return back;
}

std::map< std::string, std::string > LuaEngine::luaPopTable(void)
{
	std::map< std::string, std::string > table;
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			table[lua_tostring(L, -2)] = lua_tostring(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1); // remove table
	}
	return table;
}

QVariant LuaEngine::luaPopVariant()
{
	QVariant result;

	if (lua_isnumber(L, -1))
	{
		result = luaPopNumber();
	}
	else if (lua_isstring(L, -1))
	{
		result = QString(luaPopString().c_str());
	}
	else if (lua_isuserdata(L, -1))
	{
		result = (qulonglong)lua_touserdata(L, -1);
	}
	else if (lua_istable(L, -1))
	{
		QHash<QString, QVariant> table;
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			table[lua_tostring(L, -2)] = lua_tostring(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1); // remove table

		result = table;
	}

	return result;
}

void LuaEngine::exportVariable(const char* name, const QVariant& value)
{
	luaPush(value);
	lua_setglobal(L, name);
}

}


#endif
