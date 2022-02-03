#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <QString>
#include <QVariant>

namespace ScriptEngine
{

class ScriptEngine
{
public:
	ScriptEngine() {};
	virtual ~ScriptEngine() {};

	virtual QVariant invokeFunction(const QString& object, const QString& method, const QVariantList& arguments = QVariantList()) = 0;
	virtual bool loadFile(const QString& filename) = 0;
	virtual void exportVariable(const QString& name, const QVariant& value) = 0;
};

}

#endif // SCRIPTENGINE_H
