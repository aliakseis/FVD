#pragma once

#include "scriptengine.h"
#include <QJSEngine>

class Test_Script;

namespace ScriptEngine
{

class JSEngine : public ScriptEngine
{
	friend class ::Test_Script;
public:
	JSEngine();
	virtual ~JSEngine();

	bool loadFile(const QString& filename) override;
	QVariant invokeFunction(const QString& object, const QString& method, const QVariantList& arguments = QVariantList()) override;
	void exportVariable(const QString& name, const QVariant& value) override;
protected:
	virtual QJSEngine* scriptEngine();
	virtual void initializeEngine(QJSEngine* engine);

private:
	QScopedPointer<QJSEngine> m_scriptEngine;

};

}
