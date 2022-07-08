#include "taskbar.h"

#if defined(Q_OS_WIN)

#include <windows.h>
#include <tchar.h>
#include <Shobjidl.h>

#pragma comment(lib, "Comctl32.lib")

namespace ui_utils
{

unsigned int TaskBar::InitMessage()
{
    static const auto taskBarCreatedMsg = ::RegisterWindowMessageW(L"TaskbarButtonCreated");
    return taskBarCreatedMsg;
}

TaskBar::TaskBar() : m_taskBar(nullptr), isProgressInitialized(false), m_main(NULL), m_maximum(100)
{
}

TaskBar::~TaskBar()
{
    Uninit();
}

void TaskBar::Uninit()
{
    if (m_taskBar)
    {
        m_taskBar->Release();
        m_taskBar = nullptr;
    }
}

void TaskBar::Init(WId main)
{
    Uninit();

    m_main = main;

    OSVERSIONINFOEX osver = {sizeof(OSVERSIONINFOEX), 6, 1};
    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
    bool isVersionOk = FALSE != VerifyVersionInfo(
                           &osver,
                           VER_MAJORVERSION | VER_MINORVERSION,
                           dwlConditionMask
                       );

    if (isVersionOk)
    {
        // Let the TaskbarButtonCreated message through the UIPI filter. If we don't
        // do this, Explorer would be unable to send that message to our window if we
        // were running elevated. It's OK to make the call all the time, since if we're
        // not elevated, this is a no-op.
        CHANGEFILTERSTRUCT cfs = { sizeof(CHANGEFILTERSTRUCT) };

        HMODULE user32 = ::LoadLibrary(_T("user32.dll"));
        if (user32 != nullptr)
        {
            auto ChangeWindowMessageFilterEx_ = reinterpret_cast<decltype(ChangeWindowMessageFilterEx)*>(GetProcAddress(user32, "ChangeWindowMessageFilterEx"));
            if (ChangeWindowMessageFilterEx_)
            {
                ChangeWindowMessageFilterEx_((HWND)main, InitMessage(), MSGFLT_ALLOW, &cfs);
                ChangeWindowMessageFilterEx_((HWND)main, WM_COMMAND, MSGFLT_ALLOW, &cfs);
            }
            else
            {
                isVersionOk = false;
            }
            ::FreeLibrary(user32);
        }
    }
    if (isVersionOk)
    {
        if (FAILED(::CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_ALL, IID_ITaskbarList3, (void**)&m_taskBar)))
        {
            m_taskBar = nullptr;
        }
    }
}

void TaskBar::setProgress(int progress)
{
    if (m_taskBar)
    {
        if (progress > 0)
        {
            if (!isProgressInitialized)
            {
                setNormal();
                isProgressInitialized = true;
            }
            m_taskBar->SetProgressValue((HWND)m_main, progress, m_maximum);
        }
        else
        {
            unsetProgress();
        }
    }
}

void TaskBar::unsetProgress()
{
    if (m_taskBar)
    {
        m_taskBar->SetProgressState((HWND)m_main, TBPF_NOPROGRESS);
        isProgressInitialized = false;
    }
}

void TaskBar::setError()
{
    if (m_taskBar)
    {
        m_taskBar->SetProgressState((HWND)m_main, TBPF_ERROR);
    }
}

void TaskBar::setPaused()
{
    if (m_taskBar)
    {
        m_taskBar->SetProgressState((HWND)m_main, TBPF_PAUSED);
    }
}

void TaskBar::setNormal()
{
    if (m_taskBar)
    {
        m_taskBar->SetProgressState((HWND)m_main, TBPF_NORMAL);
    }
}

void TaskBar::setButton(HICON hIcon, const QString& tip)
{
    if (m_taskBar && SUCCEEDED(m_taskBar->HrInit()))
    {
        enum { NUM_ICONS = 1 };
        int const cxButton = GetSystemMetrics(SM_CXSMICON);
        if (auto himl = ImageList_Create(cxButton, cxButton, ILC_MASK, NUM_ICONS, 0))
        {
            HRESULT hr = m_taskBar->ThumbBarSetImageList((HWND)m_main, himl);
            if (SUCCEEDED(hr))
            {
                THUMBBUTTON buttons[NUM_ICONS] = {};

                // First button
                buttons[0].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
                buttons[0].dwFlags = THBF_ENABLED | THBF_DISMISSONCLICK;
                buttons[0].iId = BUTTON_HIT_MESSAGE;
                buttons[0].hIcon = hIcon;
                wcscpy_s(buttons[0].szTip, qUtf16Printable(tip));

                // Set the buttons to be the thumbnail toolbar
                hr = m_taskBar->ThumbBarAddButtons((HWND)m_main, ARRAYSIZE(buttons), buttons);
            }
            ImageList_Destroy(himl);
        }
    }
}

void TaskBar::updateButton(HICON hIcon, const QString& tip)
{
    if (m_taskBar)
    {
        THUMBBUTTON buttons[1] = {};

        // First button
        buttons[0].dwMask = THB_ICON | THB_TOOLTIP | THB_FLAGS;
        buttons[0].dwFlags = THBF_ENABLED | THBF_DISMISSONCLICK;
        buttons[0].iId = BUTTON_HIT_MESSAGE;
        buttons[0].hIcon = hIcon;
        wcscpy_s(buttons[0].szTip, qUtf16Printable(tip));
        m_taskBar->ThumbBarUpdateButtons((HWND)m_main, 1, buttons);
    }
}

}

#endif
