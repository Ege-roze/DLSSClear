#pragma once
#include "Protocol.h"
#include <atomic>
#include <mutex>
#include <vector>

namespace native {
enum MenuKey { CloseMenu, ToggleMenu, PreviousRow, NextRow, Decrease, Increase, Activate };
// A private message thread reserves Home only while our game window is active.
// Navigation keys are reserved only while the panel is open. No global input hook.
class MenuInput {
public:
    explicit MenuInput(Shared* shared);
    void Arm(HWND window, bool usable);
    void Visible(bool value) { visible_.store(value); }
    std::vector<MenuKey> Take(HWND window);
private:
    void Run();
    std::atomic<HWND> window_{nullptr};
    std::atomic<ULONGLONG> present_{0};
    std::atomic<bool> visible_{false};
    std::mutex mutex_;
    std::vector<MenuKey> events_;
    Shared* shared_;
};
struct MenuImage {
    UINT x=0,y=0,width=0,height=0;
    UINT64 revision=0;
    std::vector<UINT32> rgba;
};
class Menu {
public:
    void Update(HWND window, UINT width, UINT height, Shared* shared, Options& options, MenuInput* input);
    void ForValidation(bool open, UINT selection) { open_=open; selected_=static_cast<int>(selection%5); dirty_=true; }
    const MenuImage* Image() const { return open_ && !image_.rgba.empty() ? &image_ : nullptr; }
private:
    void Paint(UINT width, UINT height, const Options& options);
    bool open_=false,dirty_=true;
    int selected_=0;
    int lastLanguage_=-1;
    Options last_{};
    MenuImage image_;
};
}
