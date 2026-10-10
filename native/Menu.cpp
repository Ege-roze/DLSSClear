#include "../src/Localization.h"
#include "Menu.h"
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace native {
namespace {
void PinModule() {
    HMODULE pin=nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&PinModule),&pin)) throw std::runtime_error("Native module pin failed");
}
constexpr std::array<int,6> keys{VK_HOME,VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_RETURN};
constexpr std::array<MenuKey,6> actions{ToggleMenu,PreviousRow,NextRow,Decrease,Increase,Activate};
int Profile(const Options& o) {
    if(o.strength==350 && o.detail==100 && o.noise==20) return 0;
    if(o.strength==650 && o.detail==200 && o.noise==12) return 1;
    if(o.strength==1150 && o.detail==350 && o.noise==8) return 2;
    return 3;
}
void Profile(Options& o,int index) {
    constexpr LONG strength[]{350,650,1150}, detail[]{100,200,350}, noise[]{20,12,8};
    o.strength=strength[index]; o.detail=detail[index]; o.noise=noise[index]; o.antiRinging=1;
}
struct Canvas {
    HDC dc=nullptr;
    HBITMAP bitmap=nullptr;
    HGDIOBJ previous=nullptr;
    UINT32* bits=nullptr;
    float scale;
    i18n::Language language;
    Canvas(UINT width,UINT height,float s,i18n::Language lang):scale(s),language(lang) {
        dc=CreateCompatibleDC(nullptr);
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=static_cast<LONG>(width); info.bmiHeader.biHeight=-static_cast<LONG>(height);
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        if(dc) bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&bits),nullptr,0);
        if(!dc || !bitmap) { if(bitmap) DeleteObject(bitmap); if(dc) DeleteDC(dc); throw std::runtime_error("Panel bitmap failed"); }
        previous=SelectObject(dc,bitmap); SetBkMode(dc,TRANSPARENT);
    }
    ~Canvas() { SelectObject(dc,previous); DeleteObject(bitmap); DeleteDC(dc); }
    int P(float value) const { return static_cast<int>(std::lround(value*scale)); }
    void Box(int x,int y,int w,int h,COLORREF color) {
        RECT rect{P(static_cast<float>(x)),P(static_cast<float>(y)),P(static_cast<float>(x+w)),P(static_cast<float>(y+h))};
        HBRUSH brush=CreateSolidBrush(color); FillRect(dc,&rect,brush); DeleteObject(brush);
    }
    void Text(const wchar_t* value,int x,int y,int w,int h,int size,COLORREF color,bool bold=false, bool right=false) {
        HFONT font=CreateFontW(-P(static_cast<float>(size)),0,0,0,bold?FW_SEMIBOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,i18n::FontFace(language));
        if(!font) throw std::runtime_error("Panel font failed");
        auto old=SelectObject(dc,font); SetTextColor(dc,color);
        RECT rect{P(static_cast<float>(x)),P(static_cast<float>(y)),P(static_cast<float>(x+w)),P(static_cast<float>(y+h))};
        DrawTextW(dc,value,-1,&rect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX|(right?DT_RIGHT:DT_LEFT));
        SelectObject(dc,old); DeleteObject(font);
    }
};
}
MenuInput::MenuInput(Shared* shared):shared_(shared) {
    PinModule();
    std::thread([this]{try { Run(); } catch(...) { if(shared_) InterlockedExchange(&shared_->menuKeysAvailable,-1); }}).detach();
}
void MenuInput::Arm(HWND window,bool usable) {
    window_.store(usable?window:nullptr); present_.store(GetTickCount64());
}
std::vector<MenuKey> MenuInput::Take(HWND window) {
    std::lock_guard lock(mutex_);
    if(window_.load()!=window) return {};
    std::vector<MenuKey> result; result.swap(events_); return result;
}
void MenuInput::Run() {
    i18n::RefreshPreference();
    ULONGLONG nextLanguageCheck=0;
    // IDs belong to this dedicated thread's queue, never the game's window/thread.
    constexpr int base=0x5c10;
    std::array<bool,6> registered{};
    bool wasFocused=false;
    for(;;) {
        const HWND target=window_.load();
        const bool focused=target && IsWindow(target) && GetForegroundWindow()==GetAncestor(target,GA_ROOT) &&
            GetTickCount64()-present_.load()<1000;
        if(!focused && wasFocused) { std::lock_guard lock(mutex_); events_.clear(); events_.push_back(CloseMenu); }
        wasFocused=focused;
        // Preference I/O stays off the render thread, and only while the game is focused.
        if(focused && GetTickCount64()>=nextLanguageCheck) {
            i18n::RefreshPreference(); nextLanguageCheck=GetTickCount64()+1000;
        }
        bool available=true;
        for(size_t i=0;i<keys.size();++i) {
            const bool wanted=focused && (i==0 || visible_.load());
            if(registered[i] && !wanted) { UnregisterHotKey(nullptr,base+static_cast<int>(i)); registered[i]=false; }
            if(wanted && !registered[i]) registered[i]=RegisterHotKey(nullptr,base+static_cast<int>(i),MOD_NOREPEAT,keys[i])!=FALSE;
            if(wanted && !registered[i]) available=false;
        }
        if(shared_) InterlockedExchange(&shared_->menuKeysAvailable,focused?(available?1:-1):0);
        MSG message{};
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            if(!focused || message.message!=WM_HOTKEY || message.wParam<base || message.wParam>=base+keys.size()) continue;
            const size_t index=static_cast<size_t>(message.wParam-base);
            if(!registered[index] || (!visible_.load() && index!=0)) continue;
            std::lock_guard lock(mutex_); if(events_.size()<32) events_.push_back(actions[index]);
        }
        MsgWaitForMultipleObjectsEx(0,nullptr,30,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }
}
void Menu::Update(HWND window,UINT width,UINT height,Shared* shared,Options& options,MenuInput* input) {
    if(input) {
        auto events=input->Take(window);
        for(auto event:events) {
            if(event==CloseMenu) { open_=false; continue; }
            if(event==ToggleMenu) { open_=!open_; dirty_=true; continue; }
            if(!open_) continue;
            if(event==PreviousRow || event==NextRow) { selected_=(selected_+(event==NextRow?1:4))%5; dirty_=true; continue; }
            Options desired=options;
            if(selected_==0) desired.enabled=!desired.enabled;
            else if(selected_==1) { int index=Profile(desired); if(index==3) index=1; Profile(desired,(index+(event==Decrease?2:1))%3); }
            else if(selected_==2) desired.split=!desired.split;
            else if(selected_==3) desired=Options{};
            else { open_=false; continue; }
            if(Write(shared,desired)) options=desired;
            dirty_=true;
        }
        if(GetForegroundWindow()!=GetAncestor(window,GA_ROOT)) open_=false;
        input->Visible(open_);
    }
    if(shared) { InterlockedExchange(&shared->menuVisible,open_); InterlockedExchange(&shared->menuSelection,selected_); }
    if(!open_) return;
    if(lastLanguage_!=static_cast<int>(i18n::Current())) dirty_=true;
    const float scale=std::clamp(static_cast<float>(height)/1080.f,.75f,2.f);
    const UINT panelWidth=static_cast<UINT>(std::lround(360*scale)), panelHeight=static_cast<UINT>(std::lround(352*scale));
    if(width<panelWidth+24 || height<panelHeight+24) { open_=false; if(input) input->Visible(false); if(shared) InterlockedExchange(&shared->menuVisible,0); return; }
    if(dirty_ || image_.width!=panelWidth || image_.height!=panelHeight || memcmp(&last_,&options,sizeof(options))!=0) Paint(width,height,options);
}
void Menu::Paint(UINT,UINT height,const Options& options) {
    // One snapshot keeps every label and font in the same language during a redraw.
    const auto language=i18n::Current();
    const auto text=[language](const wchar_t* key) { return i18n::Text(key,language); };
    const float scale=std::clamp(static_cast<float>(height)/1080.f,.75f,2.f);
    image_.width=static_cast<UINT>(std::lround(360*scale)); image_.height=static_cast<UINT>(std::lround(352*scale));
    image_.x=image_.y=static_cast<UINT>(std::lround(24*scale));
    Canvas canvas(image_.width,image_.height,scale,language);
    constexpr COLORREF background=RGB(19,24,33),white=RGB(232,239,247),muted=RGB(151,167,187),accent=RGB(96,224,191);
    canvas.Box(0,0,360,352,background); canvas.Box(0,0,3,352,accent);
    canvas.Text(L"DLSSClear",19,14,230,32,24,white,true);
    canvas.Text(L"HOME",278,20,62,22,12,muted,true,true);
    canvas.Text(text(L"CANLI KARŞILAŞTIRMA"),20,48,320,20,11,muted,true);
    canvas.Text(options.enabled?text(L"●  FİLTRE AÇIK"):text(L"○  ORİJİNAL GÖRÜNTÜ"),20,76,320,24,13,options.enabled?accent:RGB(247,196,110),true);
    const wchar_t* labels[]{text(L"Filtre"),text(L"Profil"),text(L"Karşılaştırma"),text(L"Varsayılana dön"),text(L"Paneli kapat")};
    const wchar_t* profiles[]{text(L"Yumuşak"),text(L"Dengeli"),text(L"Belirgin"),text(L"Özel")};
    const wchar_t* values[]{options.enabled?text(L"Açık"):text(L"Kapalı"),profiles[Profile(options)],options.split?text(L"Yarı yarıya"):text(L"Tam ekran"),L"Enter",L"Home"};
    for(int row=0;row<5;++row) {
        const int top=110+row*35;
        if(row==selected_) { canvas.Box(12,top,336,32,RGB(37,53,65)); canvas.Box(12,top,3,32,accent); }
        canvas.Text(labels[row],23,top,190,32,14,row==selected_?white:muted,row==selected_);
        canvas.Text(values[row],209,top,126,32,14,row==selected_?accent:white,false,true);
    }
    canvas.Box(20,296,320,1,RGB(51,63,78));
    canvas.Text(text(L"↑↓ Seç    ←→ Değiştir    Enter Uygula"),20,303,320,20,12,muted);
    canvas.Text(options.split && options.enabled?text(L"Sol: orijinal  ·  Sağ: işlenmiş"):text(L"Home ile panele dön / paneli kapat"),20,325,320,18,11,muted);
    GdiFlush();
    image_.rgba.resize(static_cast<size_t>(image_.width)*image_.height);
    for(size_t i=0;i<image_.rgba.size();++i) { const UINT32 bgra=canvas.bits[i]; image_.rgba[i]=((bgra>>16)&255)|(bgra&0xff00)|((bgra&255)<<16)|0xfa000000; }
    ++image_.revision; last_=options; lastLanguage_=static_cast<int>(language); dirty_=false;
}
}
