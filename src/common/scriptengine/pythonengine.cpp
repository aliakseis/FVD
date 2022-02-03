#include "pythonengine.h"

#include "PythonQt.h"

#include <QStandardPaths>

#include <mutex>

namespace {

auto pythonQtInstance() {

    static std::once_flag flag;

    std::call_once(flag,  []{
        PythonQt::init(/*PythonQt::IgnoreSiteModule |*/ PythonQt::RedirectStdOut);
        atexit(PythonQt::cleanup);

        QObject::connect(PythonQt::self(), &PythonQt::pythonStdOut, [](const QString& str) { qInfo() << str; });
        QObject::connect(PythonQt::self(), &PythonQt::pythonStdErr, [](const QString& str) { qCritical() << str; });
    });

    return PythonQt::self();
};

}

namespace ScriptEngine {


PythonEngine::PythonEngine()
= default;

PythonEngine::~PythonEngine()
= default;

bool PythonEngine::loadFile(const QString& filename)
{
    pythonQtInstance()->getMainModule().evalFile(filename);
    return true;
}

QVariant PythonEngine::invokeFunction(const QString& object, const QString& method, const QVariantList& arguments /*= QVariantList()*/)
{
    QString callable = (!object.isEmpty()) ? object + QStringLiteral(".") + method : method;
    return pythonQtInstance()->getMainModule().call(callable, arguments);
}

void PythonEngine::exportVariable(const QString& name, const QVariant& value)
{
    auto* qobject = qvariant_cast<QObject*>(value);
    if (qobject != nullptr)
    {
        pythonQtInstance()->registerClass(qobject->metaObject());
        pythonQtInstance()->getMainModule().addObject(name, qobject);
    }
    else
    {
        pythonQtInstance()->getMainModule().addVariable(name, value);
    }
}

} //namespace ScriptEngine
