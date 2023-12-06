#include "jsengine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJSValueIterator>
#include <QQmlEngine>
#include <QStringList>

#include "scriptextension.h"

namespace ScriptEngine
{

JSEngine::JSEngine() = default;

JSEngine::~JSEngine() = default;

bool JSEngine::loadFile(const QString& filename)
{
    QFile file(filename);
    if (file.open(QFile::ReadOnly))
    {
        QTextStream in(&file);
        in.setCodec("UTF-8");
        QString script = in.readAll();
        file.close();
        auto result = JSEngine::scriptEngine()->evaluate(script);

        if (!result.isError())
        {
            return true;
        }
        qDebug() << "Evaluating error" << result.toString();
    }
    else
    {
        qDebug() << __FUNCTION__ << "Can not open script file";
    }

    return false;
}

QVariant JSEngine::invokeFunction(const QString& object, const QString& method, const QVariantList& arguments)
{
    auto func = (object.isEmpty()) ? JSEngine::scriptEngine()->globalObject().property(method)
                                   : JSEngine::scriptEngine()->globalObject().property(object).property(method);

    if (!func.isCallable())
    {
        return {};
    }

    QJSValueList m_callParams;
    for (const auto& arg : arguments)
    {
        switch (arg.type())
        {
        case QVariant::String:
            m_callParams.append(arg.toString());
            break;
        case QVariant::Int:
            m_callParams.append(arg.toInt());
            break;
        case QVariant::UInt:
            m_callParams.append(arg.toUInt());
            break;
        case QVariant::Bool:
            m_callParams.append(arg.toBool());
            break;
        default:
            if (auto* obj = qvariant_cast<QObject*>(arg))
            {
                m_callParams.append(scriptEngine()->newQObject(obj));
                QQmlEngine::setObjectOwnership(obj, QQmlEngine::CppOwnership);
            }
            break;
        }
    }
    auto result = func.call(m_callParams);
    m_callParams.clear();
    return result.toVariant();
}

void JSEngine::exportVariable(const QString& name, const QVariant& value)
{
    auto* qobject = qvariant_cast<QObject*>(value);
    auto appProperty = (qobject != nullptr) ? JSEngine::scriptEngine()->newQObject(qobject)
                                            : JSEngine::scriptEngine()->toScriptValue<QVariant>(value);
    JSEngine::scriptEngine()->globalObject().setProperty(name, appProperty);
}

QJSEngine* JSEngine::scriptEngine()
{
    if (m_scriptEngine == nullptr)
    {
        m_scriptEngine.reset(new QJSEngine());
        initializeEngine(m_scriptEngine.data());
    }

    return m_scriptEngine.data();
}

void JSEngine::initializeEngine(QJSEngine* engine)
{
    // Global variables
    engine->globalObject().setProperty("APPLICATION_DIRECTORY",
                                       QJSValue(QCoreApplication::applicationDirPath()));  // , QScriptValue::ReadOnly);

    auto* extension = new ScriptExtension(engine);
    exportVariable("ext", QVariant::fromValue<QObject*>(extension));
}

}  // namespace ScriptEngine
