#pragma once
#include <Windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <filesystem>
#include <stdexcept>
#include <string>

// Uses Windows' event-driven registration; there is no polling or game input hook.
class GameHotkey {
public:
    ~GameHotkey() { Suspend(); }
    WORD Choice() const { return choice_; }
    bool Active() const { return registered_; }
    void Initialize(HWND owner) {
        owner_ = owner;
        PWSTR folder = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)))
            throw std::runtime_error("Kisayol ayar klasoru bulunamadi.");
        path_ = std::filesystem::path(folder) / L"DLSSClear" / L"settings.ini";
        CoTaskMemFree(folder);
        const UINT saved = GetPrivateProfileIntW(L"Hotkeys", L"ToggleGame", VK_F8, path_.c_str());
        choice_ = saved <= 0xffff && Valid(static_cast<WORD>(saved)) ? static_cast<WORD>(saved) : VK_F8;
    }
    void Suspend() { if (registered_) UnregisterHotKey(owner_, id_); registered_ = false; }
    void Resume() { Bind(choice_, false); }
    void Bind(WORD choice, bool save) {
        if (!Valid(choice)) throw std::runtime_error("F1-F11/F13-F24 veya Ctrl/Alt ile bir tus sec. F12 ayrilmistir.");
        const bool reuse = registered_ && choice == choice_;
        const int nextId = id_ == 0x4d00 ? 0x4d01 : 0x4d00;
        if (choice && !reuse && !RegisterHotKey(owner_, nextId, Modifiers(choice) | MOD_NOREPEAT, LOBYTE(choice)))
            throw std::runtime_error("Bu kisayol kullanilamiyor; baska uygulamada atanmis olabilir. Farkli bir tus sec.");
        try {
            if (save) {
                std::filesystem::create_directories(path_.parent_path());
                if (!WritePrivateProfileStringW(L"Hotkeys", L"ToggleGame", std::to_wstring(choice).c_str(), path_.c_str()))
                    throw std::runtime_error("Kisayol kaydedilemedi; onceki atama korundu.");
            }
        } catch (...) { if (choice && !reuse) UnregisterHotKey(owner_, nextId); throw; }
        if (!reuse) { Suspend(); id_ = nextId; registered_ = choice != 0; }
        choice_ = choice;
    }
    bool Matches(WPARAM id, LPARAM keys) const {
        return registered_ && id == static_cast<WPARAM>(id_) && HIWORD(keys) == LOBYTE(choice_) && LOWORD(keys) == Modifiers(choice_);
    }
private:
    static UINT Modifiers(WORD choice) {
        const BYTE flags = HIBYTE(choice);
        return ((flags & HOTKEYF_CONTROL) ? MOD_CONTROL : 0) |
            ((flags & HOTKEYF_ALT) ? MOD_ALT : 0) | ((flags & HOTKEYF_SHIFT) ? MOD_SHIFT : 0);
    }
    static bool Valid(WORD choice) {
        if (!choice) return true; // Backspace in the hotkey control disables the shortcut.
        const BYTE key = LOBYTE(choice), flags = HIBYTE(choice);
        if (!key || key == VK_F12 || key == VK_LWIN || key == VK_RWIN ||
            key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU || key == VK_ESCAPE ||
            (flags & ~(HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_SHIFT | HOTKEYF_EXT))) return false;
        return (key >= VK_F1 && key <= VK_F24) || (flags & (HOTKEYF_CONTROL | HOTKEYF_ALT));
    }
    HWND owner_ = nullptr;
    WORD choice_ = VK_F8;
    int id_ = 0x4d00;
    bool registered_ = false;
    std::filesystem::path path_;
};
