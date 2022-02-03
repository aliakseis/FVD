#include "scriptprovider.h"
#include "scriptengine/jsengine.h"
#include "scriptengine/pythonengine.h"
#include "utilities/utils.h"
#include "settings_declaration.h"
#include <QFile>
#include <QDebug>


ScriptProvider::ScriptProvider(const QString& scriptFilename)
    : m_scriptFilenameForLoading(scriptFilename)
    , m_isPython(scriptFilename.mid(m_scriptFilenameForLoading.lastIndexOf('.') + 1).toLower() == "py")
{
}

ScriptProvider::~ScriptProvider()
= default;

QVariant ScriptProvider::invokeFunction(const QString& object, const QString& method, const QVariantList& arguments /*= QVariantList()*/)
{
    if (!m_scriptEngine)
    {
        if (m_isPython) {
            m_scriptEngine = std::make_unique<ScriptEngine::PythonEngine>();
        } else {
            m_scriptEngine = std::make_unique<ScriptEngine::JSEngine>();
        }
        m_scriptEngine->loadFile(QStringLiteral(":/strategies/") + m_scriptFilenameForLoading);
    }
    return m_scriptEngine->invokeFunction(object, method, arguments);
}
