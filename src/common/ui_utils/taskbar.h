#pragma once

#include <qglobal.h>
#include "qwindowdefs.h"

#include <QString>

#if defined(Q_OS_WIN)

struct ITaskbarList3;

namespace ui_utils
{

enum { BUTTON_HIT_MESSAGE = 33345 };

class TaskBar
{
public:
    TaskBar();
    ~TaskBar();

    static unsigned int InitMessage();
    void Init(WId main);
    void Uninit();
    void setMaximum(int m) { m_maximum = m; }

    void setProgress(int progress);
    void unsetProgress();
    void setError();
    void setPaused();
    void setNormal();

    void setButton(HICON hIcon, const QString& tip);
    void updateButton(HICON hIcon, const QString& tip);

private:
    ITaskbarList3* m_taskBar;
    bool isProgressInitialized;
    WId m_main;
    int m_maximum;
};

}

#endif
