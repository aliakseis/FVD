#pragma once

#include "scriptengine.h"


namespace ScriptEngine
{

class PythonEngine : public ScriptEngine
{
public:
	PythonEngine();
	virtual ~PythonEngine();

	bool loadFile(const QString& filename) override;
	QVariant invokeFunction(const QString& object, const QString& method, const QVariantList& arguments = QVariantList()) override;
	void exportVariable(const QString& name, const QVariant& value) override;
};

}
