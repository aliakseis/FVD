#include "pythonengine.h"

#include "PythonQt.h"

#include "utilities/utils.h"

#include <QFileInfo>
#include <QStandardPaths>

#include <QApplication>
#include <QMessageBox>
#include <QMainWindow>
#include <QProcess>

#include <mutex>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

bool isPythonInstalled()
{
    const int status =
        QProcess::execute(QCoreApplication::applicationFilePath(),
            QStringList() << ScriptEngine::CHECK_PYTHON_OPTION);

    return status == 0;
}

void showPythonNotInstalledMessageBox()
{
    QMetaObject::invokeMethod(QApplication::instance(), [] {
        QMessageBox::warning(
            utilities::getMainWindow(),
            QObject::tr("Matching Python is not installed."),
            QObject::tr("Matching Python is not installed: ") + PY_VERSION
        );
        });
}

auto pythonQtInstance() {

    static bool succeeded = false;
    static std::once_flag flag{};

    std::call_once(flag,  []{
        if (!isPythonInstalled()) {
            showPythonNotInstalledMessageBox();
            return;
        }

#ifdef Q_OS_WIN
        // Allocate a console for the process.
        if (AllocConsole()) {
            // Get the handle to the new console window.
            HWND hwndConsole = GetConsoleWindow();
            if (hwndConsole != NULL) {
                // Hide the console window.
                ShowWindow(hwndConsole, SW_HIDE);
            }
        }
#endif

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

        QObject::connect(PythonQt::self(), &PythonQt::pythonStdOut, [](const QString& str) { if (str.length() > 1) qInfo() << str; });
        QObject::connect(PythonQt::self(), &PythonQt::pythonStdErr, [](const QString& str) { if (str.length() > 1) qCritical() << str; });

        succeeded = true;
    });

    return succeeded ? PythonQt::self() : nullptr;
};

std::mutex pythonMonitor;

}

namespace ScriptEngine {

int checkPython()
{ 
    PythonQt::init(/*PythonQt::IgnoreSiteModule |*/ PythonQt::RedirectStdOut);
    atexit(PythonQt::cleanup);

    auto sys = PythonQt::self()->importModule("sys");
    if (sys.isNull())
        return 1;
    auto paths = PythonQt::self()->getVariable(sys, "path");
    if (paths.isNull())
        return 1;

    if (paths.value<QVariantList>().empty())
        return 1;

    return 0;
}

PythonEngine::PythonEngine()
= default;

PythonEngine::~PythonEngine()
= default;

bool PythonEngine::loadFile(const QString& filename)
{
    if (auto inst = pythonQtInstance())
    {
        std::lock_guard<std::mutex> guard(pythonMonitor);
        inst->getMainModule().evalFile(filename);
        return true;
    }
    return false;
}

QVariant PythonEngine::invokeFunction(const QString& object, const QString& method, const QVariantList& arguments /*= QVariantList()*/)
{
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
    if (auto inst = pythonQtInstance())
    {
        QString callable = (object.isEmpty()) ? method : object + QStringLiteral(".") + method;
        std::lock_guard<std::mutex> guard(pythonMonitor);
        return inst->getMainModule().call(callable, arguments);
    }
    return false;
}

void PythonEngine::exportVariable(const QString& name, const QVariant& value)
{
    if (auto inst = pythonQtInstance())
    {
        auto* qobject = qvariant_cast<QObject*>(value);
        std::lock_guard<std::mutex> guard(pythonMonitor);
        if (qobject != nullptr)
        {
            inst->registerClass(qobject->metaObject());
            inst->getMainModule().addObject(name, qobject);
        }
        else
        {
            inst->getMainModule().addVariable(name, value);
        }
    }
}

} //namespace ScriptEngine
