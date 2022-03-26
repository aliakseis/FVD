#pragma once

#include <QString>
#include <QVariantList>
#include <memory>

namespace ScriptEngine
{
class ScriptEngine;
}

class ScriptProvider
{
public:
    ScriptProvider(const QString& scriptFilename);
    ~ScriptProvider();

    QVariant invokeFunction(const QString& object, const QString& method,
                            const QVariantList& arguments = QVariantList());
    QVariant invokeFunction(const QString& method, const QVariantList& arguments = QVariantList())
    {
        return invokeFunction({}, method, arguments);
    }

    bool doesWaitLoop() const { return !m_isPython; }

private:
    const QString m_scriptFilenameForLoading;
    std::unique_ptr<ScriptEngine::ScriptEngine> m_scriptEngine;
    const bool m_isPython;
};
