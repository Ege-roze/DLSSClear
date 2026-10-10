#pragma once
#include "Localization.h"
#include <Windows.h>
#include <shellapi.h>
#include <stdexcept>

// The window stays alive for timers, game IPC and the global hotkey when hidden.
class TrayIcon {
public:
    ~TrayIcon() { Remove(); }
    bool Add(HWND owner, UINT callback) {
        if (present_) return true;
        data_ = {};
        data_.cbSize = sizeof(data_);
        data_.hWnd = owner;
        data_.uID = 1;
        data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data_.uCallbackMessage = callback;
        data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION); // Shared Windows icon.
        wcscpy_s(data_.szTip, i18n::Text(L"DLSSClear · Açmak için tıkla"));
        if (!Shell_NotifyIconW(NIM_ADD, &data_)) return false;
        present_ = true;
        data_.uVersion = NOTIFYICON_VERSION_4;
        if (!Shell_NotifyIconW(NIM_SETVERSION, &data_)) { Remove(); return false; }
        return true;
    }
    void Remove() {
        if (present_) Shell_NotifyIconW(NIM_DELETE, &data_);
        present_ = false;
    }
    void ExplorerRestarted() { present_ = false; }
    void RefreshText() {
        if(!present_) return;
        wcscpy_s(data_.szTip,i18n::Text(L"DLSSClear · Açmak için tıkla"));
        auto update=data_; update.uFlags=NIF_TIP|NIF_SHOWTIP;
        Shell_NotifyIconW(NIM_MODIFY,&update);
    }
private:
    NOTIFYICONDATAW data_{};
    bool present_ = false;
};

inline UINT ShowAppMessage() { return RegisterWindowMessageW(L"DLSSClear.ShowWindow.v1"); }

// Launching the shortcut again restores the resident instance. CLI validation
// modes deliberately bypass this guard and never register a tray icon/hotkey.
class AppInstance {
public:
    ~AppInstance() { if (mutex_) CloseHandle(mutex_); }
    bool Start() {
        mutex_ = CreateMutexW(nullptr, FALSE, L"Local\\DLSSClear.Desktop.v1");
        if (!mutex_) throw std::runtime_error("Uygulama oturumu baslatilamadi.");
        if (GetLastError() != ERROR_ALREADY_EXISTS) return true;
        const UINT message = ShowAppMessage();
        if (!message) throw std::runtime_error("Uygulama penceresi acilamadi.");
        // The first process can still be creating its window after taking the guard.
        for (unsigned attempt = 0; attempt < 30; ++attempt) {
            if (HWND window = FindWindowW(L"DLSSClearWindow", nullptr)) {
                DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
                if (pid) AllowSetForegroundWindow(pid);
                if (PostMessageW(window, message, 0, 0)) return false;
                break;
            }
            Sleep(100);
        }
        throw std::runtime_error("DLSSClear zaten calisiyor. Sistem tepsisi simgesinden acmayi dene.");
    }
private:
    HANDLE mutex_ = nullptr;
};
