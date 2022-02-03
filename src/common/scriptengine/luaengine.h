#ifndef LUAENGINE_H
#define LUAENGINE_H

#ifdef WITH_LUA

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#include "scriptengine.h"
#include <string>
#include <map>

#ifdef Q_OS_WIN32
#include <QThread>
#else
#include <thread>
#endif

#include <QString>

namespace ScriptEngine
{

class LuaEngine : public ScriptEngine
{
public:
	struct LuaEventContext
	{
		lua_State* L;
		const char* table;
		const char* method;
		int args;
		int ret;
	};

	LuaEngine();
	virtual ~LuaEngine();

	QVariant invokeFunction(const char* function, const QVariantList& arguments = QVariantList(), QLSCallback callback = nullptr) override;
	QVariant invokeFunction(const char* object, const char* method, const QVariantList& arguments = QVariantList(), QLSCallback callback = nullptr) override;
	void registerFunction(const char* name, lua_CFunction func);
	bool loadFile(const char* filename);
	bool loadFile(const std::string& filename) { loadFile(filename.c_str()); };
	void reload();

	int getStatus() { return status; };
	int getFileStatus() { return loadFileStatus; };

	// Push templates
	template< typename T > void push(const T& value)
	{
		luaPush(value);
	}
	void luaPush(int i);
	void luaPush(bool b);
	void luaPush(const std::string& str);
	void luaPush(const std::map<std::string, std::string>& map);
	void luaPush(const char* str);
	void luaPush(void* func);
	void luaPush(const QString& string);
	void luaPush(const QVariant& variant);
	void luaPush(const QHash<QString, QVariant>& hash);
	void luaPush(const QMap<QString, QVariant>& hash);
	template< typename T > void luaPush(T* p) { luaPush((void*)p); };

	void exportVariable(const char* name, const QVariant& value) override;

	std::string luaPopString(void);
	double luaPopNumber(void);
	template< typename T> T* luaPopData(void)
	{
		T* back;
		if (lua_isuserdata(L, -1))
		{
			back = lua_touserdata(L, -1);
		}
		lua_pop(L, 1);
		return back;
	};
	std::map<std::string, std::string> luaPopTable(void);
	QVariant luaPopVariant();
protected:
#ifdef Q_OS_WIN32
	class _CallThread : public QThread
	{
	public:
		void run() override;
		LuaEventContext context;
	} call_thread_q;
#endif
	static void call_thread(const LuaEventContext& context);

	static int status;
	int loadFileStatus;
private:
	lua_State* L;
	static void reportError(lua_State* L);
};

}

#endif // WITH_LUA

#endif // LUAENGINE_H
