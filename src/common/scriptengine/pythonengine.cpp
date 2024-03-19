#include "pythonengine.h"

#include "PythonQt.h"

#include <QFileInfo>
#include <QStandardPaths>

#include <mutex>

namespace {

auto pythonQtInstance() {

    static std::once_flag flag;

    std::call_once(flag,  []{
        PythonQt::init(/*PythonQt::IgnoreSiteModule |*/ PythonQt::RedirectStdOut);
        atexit(PythonQt::cleanup);

        auto sys = PythonQt::self()->importModule("sys");
        auto paths = PythonQt::self()->getVariable(sys, "path");

        qDebug() << "Python sys.path:" << paths;

        for (auto path : paths.value<QVariantList>())
        {
            auto sitePackages = path.toString() + QStringLiteral("/site-packages");

            if (QFileInfo::exists(sitePackages))
            {
                PythonQt::self()->addSysPath(sitePackages);
            }
        }

        QObject::connect(PythonQt::self(), &PythonQt::pythonStdOut, [](const QString& str) { qInfo() << str; });
        QObject::connect(PythonQt::self(), &PythonQt::pythonStdErr, [](const QString& str) { qCritical() << str; });
    });

    return PythonQt::self();
};

std::mutex pythonMonitor;

}

namespace ScriptEngine {


PythonEngine::PythonEngine()
= default;

PythonEngine::~PythonEngine()
= default;

bool PythonEngine::loadFile(const QString& filename)
{
    std::lock_guard<std::mutex> guard(pythonMonitor);
    pythonQtInstance()->getMainModule().evalFile(filename);
    return true;
}

QVariant PythonEngine::invokeFunction(const QString& object, const QString& method, const QVariantList& arguments /*= QVariantList()*/)
{
    QString callable = (object.isEmpty()) ? method : object + QStringLiteral(".") + method;
    std::lock_guard<std::mutex> guard(pythonMonitor);
    //QStringList l = PythonQt::self()->introspection(PythonQt::self()->getMainModule(), callable, PythonQt::CallOverloads);
    //qDebug() << l;
    //auto obj = pythonQtInstance()->lookupObject(pythonQtInstance()->getMainModule(), callable);
    //PyObject* pDocString = PyObject_GetAttrString(obj, "__doc__");
    //if (pDocString)
    //{
    //    const char* docString = PyUnicode_AsUTF8(pDocString);
    //    if (docString)
    //    {
    //        std::string result(docString);
    //    }
    //    Py_DECREF(pDocString);
    //}
    return pythonQtInstance()->getMainModule().call(callable, arguments);
}

void PythonEngine::exportVariable(const QString& name, const QVariant& value)
{
    auto* qobject = qvariant_cast<QObject*>(value);
    std::lock_guard<std::mutex> guard(pythonMonitor);
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
