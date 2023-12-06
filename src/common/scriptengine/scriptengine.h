#pragma once

#include <QString>
#include <QVariant>

namespace ScriptEngine
{

class ScriptEngine
{
public:
	virtual ~ScriptEngine() {};

	virtual QVariant invokeFunction(const QString& object, const QString& method, const QVariantList& arguments = QVariantList()) = 0;
	virtual bool loadFile(const QString& filename) = 0;
	virtual void exportVariable(const QString& name, const QVariant& value) = 0;
};

}
