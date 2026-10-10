#include "Localization.h"
#include "Renderer.h"
#include "Validation.h"
#include "Games.h"
#include "NativeLink.h"
#include "GameHotkey.h"
#include "GameProfiles.h"
#include "TrayIcon.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

// Request the discrete GPU on hybrid systems; the selected adapter is shown in the UI.
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
namespace {
constexpr int kOpen = 101, kSave = 102, kDemo = 103, kMode = 104, kNative = 105,
    kRing = 106, kReset = 107, kStrength = 108, kDetail = 109, kNoise = 110, kPreset = 111,
    kRadius = 112, kAutoGame = 113, kRefreshGame = 114, kGameInfo = 115,
    kGameEnable=116, kGameProfile=117, kGameSplit=118, kInstall=119, kRemove=120, kNativeDemo=121,
    kHotkey=122, kSaveHotkey=123, kSaveGame=124, kLoadGame=125, kExit=126, kShowApp=127, kLanguage=128;
constexpr UINT kTrayMessage=WM_APP+1;
constexpr int kToolbar = 491, kStatus = 32;
std::filesystem::path ExecutableDirectory() {
    std::array<wchar_t, 32768> buffer{};
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length == buffer.size()) throw std::runtime_error("Executable path unavailable.");
    return std::filesystem::path(buffer.data()).parent_path();
}
std::filesystem::path ShaderPath() {
    auto folder = ExecutableDirectory();
    std::filesystem::path fallback;
    for (int i = 0; i < 5; ++i) {
        auto candidate = folder / L"shaders" / L"Clear.hlsl";
        if (std::filesystem::exists(candidate)) {
            if (std::filesystem::exists(folder / L"DLSSClear.vcxproj")) return candidate;
            if (fallback.empty()) fallback = candidate;
        }
        folder = folder.parent_path();
    }
    if (!fallback.empty()) return fallback;
    throw std::runtime_error("shaders/Clear.hlsl bulunamadi. Shader klasorunu EXE yanina koyun.");
}
void Error(HWND owner, const std::exception& error) {
    int length = MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, nullptr, 0);
    std::wstring text(static_cast<size_t>(std::max(length, 1)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, text.data(), length);
    text.resize(wcslen(text.c_str()));
    const auto translated=i18n::Text(text.c_str());
    const std::wstring message=translated==text.c_str()
        ? std::wstring(i18n::Text(L"İşlem başarısız oldu."))+L"\n\n"+text : translated;
    MessageBoxW(owner, message.c_str(), L"DLSSClear", MB_OK | MB_ICONERROR);
}
class App {
public:
    ~App() { if (profileProcess_) CloseHandle(profileProcess_); }
    int Run(HINSTANCE instance, int show, const std::filesystem::path& initialImage, bool smoke);
private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK ViewProc(HWND window, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK HotkeyProc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data);
    LRESULT Handle(UINT message, WPARAM wp, LPARAM lp);
    HWND Control(const wchar_t* type, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id = 0);
    void BuildControls();
    void ApplyLanguage(bool save);
    void Layout();
    void UpdateSettings();
    void UpdateStatus();
    void Open();
    void Save();
    void Reset();
    void ApplyPreset(unsigned index);
    void RefreshGames(bool discover);
    void RefreshNative(bool apply=false);
    void BindGameProfile(DWORD pid);
    void GameProfileAction(bool save);
    void UpdateGameProfileStatus();
    void InstallGame(bool remove);
    void NativeDemo();
    void UpdateHotkey(bool save);
    void ToggleGame();
    void HideToTray();
    void ShowFromTray();
    void TrayMenu(POINT point);
    void SetImage(const Image& image, const std::wstring& name);
    HWND window_ = nullptr, view_ = nullptr, status_ = nullptr;
    HWND mode_ = nullptr, native_ = nullptr, ring_ = nullptr;
    HWND preset_ = nullptr, viewHint_ = nullptr;
    HWND radius_ = nullptr, autoGame_ = nullptr, gameStatus_ = nullptr;
    HWND gameEnable_=nullptr, gameProfile_=nullptr, gameSplit_=nullptr, nativeStatus_=nullptr;
    NativeLink nativeLink_;
    GameHotkey gameHotkey_;
    TrayIcon trayIcon_;
    UINT taskbarCreated_=0, showAppMessage_=0;
    bool hiddenToTray_=false;
    HWND hotkey_=nullptr, hotkeyStatus_=nullptr;
    GameProfiles gameProfiles_;
    std::filesystem::path profileExe_;
    std::wstring profileName_, profileIssue_;
    std::optional<native::Options> savedGameOptions_, pendingGameOptions_;
    std::vector<std::pair<DWORD, ULONGLONG>> profileSessions_;
    HANDLE profileProcess_=nullptr;
    HWND saveGame_=nullptr, loadGame_=nullptr, profileStatus_=nullptr;
    DWORD nativePid_=0, demoPid_=0;
    HANDLE demoProcess_=nullptr;
    HWND strength_ = nullptr, detail_ = nullptr, noise_ = nullptr;
    HWND strengthLabel_ = nullptr, detailLabel_ = nullptr, noiseLabel_ = nullptr;
    HFONT font_ = nullptr;
    HWND languageControl_=nullptr;
    std::vector<std::pair<HWND,const wchar_t*>> controlLabels_;
    Renderer renderer_;
    Settings settings_;
    bool ready_ = false, panning_ = false, minimized_ = false;
    POINT lastMouse_{};
    std::filesystem::path sourcePath_;
    std::wstring imageName_ = i18n::Text(L"Sentetik test görüntüsü");
    std::vector<GameInstallation> games_;
    GameStatus gameState_;
    ULONGLONG nextGameCheck_ = 0;
};
HWND App::Control(const wchar_t* type, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND result = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
        window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (!result) throw std::runtime_error("UI control creation failed.");
    SendMessageW(result, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    controlLabels_.push_back({result,i18n::Key(text)});
    return result;
}
void App::BuildControls() {
    font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, i18n::FontFace());
    Control(L"BUTTON", i18n::Text(L"Görüntü aç (O)"), BS_PUSHBUTTON | WS_TABSTOP, 12, 10, 140, 30, kOpen);
    Control(L"BUTTON", i18n::Text(L"PNG kaydet (S)"), BS_PUSHBUTTON | WS_TABSTOP, 160, 10, 140, 30, kSave);
    Control(L"BUTTON", i18n::Text(L"Test görüntüsü"), BS_PUSHBUTTON | WS_TABSTOP, 308, 10, 130, 30, kDemo);
    mode_ = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 452, 11, 225, 150, kMode);
    for (const wchar_t* text : {i18n::Text(L"1 · Orijinal"), i18n::Text(L"2 · İşlenmiş"), i18n::Text(L"3 · Sol orijinal / sağ işlenmiş"), i18n::Text(L"4 · Fark ×8")})
        SendMessageW(mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    SendMessageW(mode_, CB_SETCURSEL, 2, 0);
    native_ = Control(L"BUTTON", i18n::Text(L"%100 görünüm (Z)"), BS_AUTOCHECKBOX | WS_TABSTOP, 693, 12, 170, 26, kNative);
    Control(L"BUTTON", i18n::Text(L"Sıfırla (R)"), BS_PUSHBUTTON | WS_TABSTOP, 880, 10, 106, 30, kReset);
    strengthLabel_ = Control(L"STATIC", L"", 0, 12, 53, 250, 23);
    strength_ = Control(TRACKBAR_CLASSW, L"", TBS_HORZ | WS_TABSTOP, 8, 76, 258, 32, kStrength);
    detailLabel_ = Control(L"STATIC", L"", 0, 290, 53, 250, 23);
    detail_ = Control(TRACKBAR_CLASSW, L"", TBS_HORZ | WS_TABSTOP, 286, 76, 258, 32, kDetail);
    noiseLabel_ = Control(L"STATIC", L"", 0, 565, 53, 260, 23);
    noise_ = Control(TRACKBAR_CLASSW, L"", TBS_HORZ | WS_TABSTOP, 561, 76, 258, 32, kNoise);
    SendMessageW(strength_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
    SendMessageW(detail_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(noise_, TBM_SETRANGE, TRUE, MAKELPARAM(0, 50));
    ring_ = Control(L"BUTTON", i18n::Text(L"Anti-ringing"), BS_AUTOCHECKBOX | WS_TABSTOP, 840, 75, 155, 26, kRing);
    Control(L"STATIC", i18n::Text(L"Profil:"), 0, 14, 119, 50, 23);
    preset_ = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 70, 114, 225, 220, kPreset);
    for (const auto& profile : kFilterPresets) SendMessageW(preset_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(i18n::Text(profile.name)));
    SendMessageW(preset_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(i18n::Text(L"Özel")));
    viewHint_ = Control(L"STATIC", L"", 0, 312, 119, 710, 25);
    Control(L"STATIC", i18n::Text(L"Foto netlik alanı:"), 0, 14, 154, 133, 24);
    radius_ = Control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 150, 149, 145, 140, kRadius);
    for (const auto* text : {i18n::Text(L"1 px · ince"), i18n::Text(L"2 px · orta"), i18n::Text(L"3 px · geniş")})
        SendMessageW(radius_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    Control(L"STATIC", i18n::Text(L"SDR fotoğraflar  |  1–4: görünüm  |  Z: %100  |  Sağ tuş: taşı  |  F5: yenile"), 0, 312, 154, 710, 24);
    autoGame_ = Control(L"BUTTON", i18n::Text(L"Otomatik oyun algıla (Steam)"), BS_AUTOCHECKBOX | WS_TABSTOP, 14, 190, 270, 26, kAutoGame);
    SendMessageW(autoGame_, BM_SETCHECK, BST_CHECKED, 0);
    Control(L"BUTTON", i18n::Text(L"Yenile"), BS_PUSHBUTTON | WS_TABSTOP, 310, 187, 100, 30, kRefreshGame);
    Control(L"BUTTON", i18n::Text(L"Oyun bağlantısı bilgisi"), BS_PUSHBUTTON | WS_TABSTOP, 420, 187, 205, 30, kGameInfo);
    Control(L"STATIC", i18n::Text(L"Home: oyun içi karşılaştırma paneli"), 0, 646, 194, 380, 24);
    gameStatus_ = Control(L"STATIC", i18n::Text(L"Oyunlar aranıyor..."), SS_LEFT, 14, 230, 1008, 26);
    gameEnable_=Control(L"BUTTON", i18n::Text(L"Oyun filtresi"), BS_AUTOCHECKBOX|WS_TABSTOP,14,266,130,26,kGameEnable);
    SendMessageW(gameEnable_,BM_SETCHECK,BST_CHECKED,0);
    gameProfile_=Control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,150,263,140,150,kGameProfile);
    for(unsigned i=0;i<3;++i) SendMessageW(gameProfile_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(i18n::Text(kFilterPresets[i].name)));
    SendMessageW(gameProfile_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(i18n::Text(L"Özel")));
    SendMessageW(gameProfile_,CB_SETCURSEL,1,0);
    gameSplit_=Control(L"BUTTON",i18n::Text(L"Yarı yarıya"),BS_AUTOCHECKBOX|WS_TABSTOP,302,266,120,26,kGameSplit);
    Control(L"BUTTON",i18n::Text(L"Oyuna kur"),BS_PUSHBUTTON|WS_TABSTOP,430,262,110,30,kInstall);
    Control(L"BUTTON",i18n::Text(L"Kaldır"),BS_PUSHBUTTON|WS_TABSTOP,549,262,90,30,kRemove);
    Control(L"BUTTON",i18n::Text(L"Bağlantı denemesi"),BS_PUSHBUTTON|WS_TABSTOP,650,262,175,30,kNativeDemo);
    nativeStatus_=Control(L"STATIC",i18n::Text(L"Kendi bağlantımız · Başlatılıyor..."),SS_LEFT,14,308,1010,28);
    Control(L"STATIC",i18n::Text(L"Oyun aç/kapat tuşu:"),0,14,349,165,24);
    hotkey_=Control(HOTKEY_CLASSW,L"",WS_TABSTOP,181,343,165,30,kHotkey);
    Control(L"BUTTON",i18n::Text(L"Tuşu kaydet"),BS_PUSHBUTTON|WS_TABSTOP,359,343,120,30,kSaveHotkey);
    hotkeyStatus_=Control(L"STATIC",L"",SS_LEFT,493,348,532,32);
    saveGame_=Control(L"BUTTON",i18n::Text(L"Bu oyun için kaydet"),BS_PUSHBUTTON|WS_TABSTOP,14,391,180,32,kSaveGame);
    loadGame_=Control(L"BUTTON",i18n::Text(L"Kaydı yükle"),BS_PUSHBUTTON|WS_TABSTOP,204,391,120,32,kLoadGame);
    profileStatus_=Control(L"STATIC",i18n::Text(L"Kayıt için bağlı bir oyun veya deneme bekleniyor"),SS_LEFT,340,388,485,50);
    Control(L"BUTTON",i18n::Text(L"Uygulamayı kapat"),BS_PUSHBUTTON|WS_TABSTOP,839,391,185,32,kExit);
    Control(L"STATIC",i18n::Text(L"Dil"),0,14,451,85,26);
    languageControl_=Control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,106,446,218,150,kLanguage);
    for(const auto* name:{L"Türkçe",L"English",L"简体中文"}) SendMessageW(languageControl_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name));
    SendMessageW(languageControl_,CB_SETCURSEL,static_cast<WPARAM>(i18n::Current()),0);
    EnableWindow(saveGame_,FALSE); EnableWindow(loadGame_,FALSE);
    if (!SetWindowSubclass(hotkey_,HotkeyProc,1,reinterpret_cast<DWORD_PTR>(this)))
        throw std::runtime_error("Hotkey control setup failed.");
    status_ = Control(L"STATIC", i18n::Text(L"Başlatılıyor..."), SS_LEFT, 12, 0, 1100, 26);
    Reset();
}
void App::ApplyLanguage(bool save) {
    const auto oldLanguage=i18n::Current();
    if(save) {
        const auto selected=static_cast<i18n::Language>(std::clamp<LRESULT>(SendMessageW(languageControl_,CB_GETCURSEL,0,0),0,2));
        try { i18n::SavePreference(selected); } catch(...) {
            SendMessageW(languageControl_,CB_SETCURSEL,static_cast<WPARAM>(oldLanguage),0); throw;
        }
        i18n::Set(selected);
    }
    HFONT nextFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,i18n::FontFace());
    for(const auto& [control,key]:controlLabels_) {
        if(nextFont) SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(nextFont),TRUE);
        if(*key) SetWindowTextW(control,i18n::Text(key));
    }
    if(nextFont) { if(font_) DeleteObject(font_); font_=nextFont; }
    auto combo=[](HWND control,const std::vector<const wchar_t*>& keys) {
        const auto selected=SendMessageW(control,CB_GETCURSEL,0,0);
        SendMessageW(control,CB_RESETCONTENT,0,0);
        for(const auto* key:keys) SendMessageW(control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(i18n::Text(key)));
        SendMessageW(control,CB_SETCURSEL,selected,0);
    };
    combo(mode_,{L"1 · Orijinal",L"2 · İşlenmiş",L"3 · Sol orijinal / sağ işlenmiş",L"4 · Fark ×8"});
    combo(radius_,{L"1 px · ince",L"2 px · orta",L"3 px · geniş"});
    std::vector<const wchar_t*> presets;
    for(const auto& preset:kFilterPresets) presets.push_back(preset.name);
    presets.push_back(L"Özel"); combo(preset_,presets);
    combo(gameProfile_,{kFilterPresets[0].name,kFilterPresets[1].name,kFilterPresets[2].name,L"Özel"});
    if(sourcePath_.empty()) imageName_=i18n::Text(L"Sentetik test görüntüsü");
    SetWindowTextW(window_,(L"DLSSClear · "+imageName_+i18n::Text(L" · Bağımsız DX11 prototipi")).c_str());
    if(nativePid_ && nativePid_==demoPid_) profileName_=i18n::Text(L"Bağlantı denemesi");
    profileIssue_=i18n::Text(i18n::Key(profileIssue_.c_str()));
    SetWindowTextW(gameStatus_,SendMessageW(autoGame_,BM_GETCHECK,0,0)==BST_CHECKED
        ? DescribeGames(gameState_).c_str() : i18n::Text(L"Otomatik algılama kapalı. Güncel durum için Yenile'ye bas."));
    UpdateSettings(); UpdateStatus(); UpdateHotkey(false); RefreshNative();
    trayIcon_.RefreshText();
}
void App::Reset() {
    settings_ = Settings{};
    SendMessageW(native_, BM_SETCHECK, BST_UNCHECKED, 0); SendMessageW(mode_, CB_SETCURSEL, 2, 0);
    ApplyPreset(1);
}
void App::ApplyPreset(unsigned index) {
    if (index >= kFilterPresets.size()) return; // Selecting Custom keeps current settings.
    const auto& profile = kFilterPresets[index];
    SendMessageW(strength_, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(profile.strength * 100.f)));
    SendMessageW(detail_, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(profile.detail * 100.f)));
    SendMessageW(noise_, TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(profile.noise * 1000.f)));
    SendMessageW(ring_, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(radius_, CB_SETCURSEL, profile.radius - 1, 0);
    UpdateSettings();
}
void App::UpdateSettings() {
    settings_.strength = static_cast<float>(SendMessageW(strength_, TBM_GETPOS, 0, 0)) / 100.f;
    settings_.detail = static_cast<float>(SendMessageW(detail_, TBM_GETPOS, 0, 0)) / 100.f;
    settings_.noise = static_cast<float>(SendMessageW(noise_, TBM_GETPOS, 0, 0)) / 1000.f;
    settings_.antiRinging = SendMessageW(ring_, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1.f : 0.f;
    settings_.radius = static_cast<unsigned>(std::clamp<LRESULT>(SendMessageW(radius_, CB_GETCURSEL, 0, 0), 0, 2)) + 1;
    settings_.mode = static_cast<unsigned>(SendMessageW(mode_, CB_GETCURSEL, 0, 0));
    auto label = [](HWND control, const wchar_t* name, float value) {
        std::wostringstream text; text << name << L": " << std::fixed << std::setprecision(3) << value;
        SetWindowTextW(control, text.str().c_str());
    };
    label(strengthLabel_, i18n::Text(L"Keskinlik"), settings_.strength);
    label(detailLabel_, i18n::Text(L"İnce ayrıntı kontrastı"), settings_.detail);
    label(noiseLabel_, i18n::Text(L"Gürültü koruma eşiği"), settings_.noise);
    unsigned match = static_cast<unsigned>(kFilterPresets.size());
    for (unsigned i = 0; i < kFilterPresets.size(); ++i) {
        const auto& profile = kFilterPresets[i];
        if (settings_.antiRinging > .5f && settings_.radius == profile.radius && std::abs(settings_.strength - profile.strength) < .00001f &&
            std::abs(settings_.detail - profile.detail) < .00001f && std::abs(settings_.noise - profile.noise) < .00001f) match = i;
    }
    SendMessageW(preset_, CB_SETCURSEL, match, 0);
    SetWindowTextW(viewHint_, settings_.mode == 3
        ? i18n::Text(L"Fark ×8: parlak bölgeler daha çok değişti. Bu bir kalite puanı değildir.")
        : settings_.radius > 1 ? i18n::Text(L"Foto modu: daha geniş alanda netlik. Kaybolmuş ayrıntıları yeniden üretmez.")
        : i18n::Text(L"Profiller başlangıç ayarıdır. Küçük ayrıntıları %100 görünümde incele."));
}
void App::RefreshGames(bool discover) {
    if (discover) games_ = DiscoverSteamGames();
    gameState_ = DetectGames(games_, gameState_.activeGame);
    const auto text = DescribeGames(gameState_);
    std::array<wchar_t, 2048> previous{}; GetWindowTextW(gameStatus_, previous.data(), static_cast<int>(previous.size()));
    if (text != previous.data()) SetWindowTextW(gameStatus_, text.c_str());
    nextGameCheck_ = GetTickCount64() + 2000;
}
void App::RefreshNative(bool apply) {
    DWORD pid=gameState_.activePid;
    if(!pid && demoProcess_) { DWORD code=0; if(GetExitCodeProcess(demoProcess_,&code) && code==STILL_ACTIVE) pid=demoPid_; }
    if(pid==nativePid_ && profileProcess_ && WaitForSingleObject(profileProcess_,0)!=WAIT_TIMEOUT) pid=0;
    if(nativeLink_.Connect(pid)) {
        const bool newTarget=nativePid_!=pid;
        if(newTarget) { nativePid_=pid; BindGameProfile(pid); }
        if(apply && !newTarget) {
            native::Options options;
            if(pendingGameOptions_) options=*pendingGameOptions_;
            else if(!nativeLink_.Read(options)) return; // Never replace unknown settings with defaults.
            const auto selected=SendMessageW(gameProfile_,CB_GETCURSEL,0,0);
            if(selected>=0 && selected<3) {
                const auto& profile=kFilterPresets[static_cast<size_t>(selected)];
                options.strength=static_cast<LONG>(std::lround(profile.strength*1000));
                options.detail=static_cast<LONG>(std::lround(profile.detail*1000));
                options.noise=static_cast<LONG>(std::lround(profile.noise*1000));
                options.antiRinging=1;
            }
            options.enabled=SendMessageW(gameEnable_,BM_GETCHECK,0,0)==BST_CHECKED;
            options.split=SendMessageW(gameSplit_,BM_GETCHECK,0,0)==BST_CHECKED;
            pendingGameOptions_=options;
        }
        if(pendingGameOptions_ && nativeLink_.Apply(*pendingGameOptions_)) pendingGameOptions_.reset();
        if(!pendingGameOptions_) {
            native::Options options;
            if(nativeLink_.Read(options)) {
                const WPARAM enabled=options.enabled?BST_CHECKED:BST_UNCHECKED, split=options.split?BST_CHECKED:BST_UNCHECKED;
                if(SendMessageW(gameEnable_,BM_GETCHECK,0,0)!=static_cast<LRESULT>(enabled)) SendMessageW(gameEnable_,BM_SETCHECK,enabled,0);
                if(SendMessageW(gameSplit_,BM_GETCHECK,0,0)!=static_cast<LRESULT>(split)) SendMessageW(gameSplit_,BM_SETCHECK,split,0);
                int match=3;
                for(int i=0;i<3;++i) {
                    const auto& profile=kFilterPresets[static_cast<size_t>(i)];
                    if(options.antiRinging && options.strength==std::lround(profile.strength*1000) && options.detail==std::lround(profile.detail*1000) && options.noise==std::lround(profile.noise*1000)) match=i;
                }
                if(!SendMessageW(gameProfile_,CB_GETDROPPEDSTATE,0,0) && SendMessageW(gameProfile_,CB_GETCURSEL,0,0)!=match)
                    SendMessageW(gameProfile_,CB_SETCURSEL,match,0);
            }
        }
    } else if(nativePid_) { nativePid_=0; BindGameProfile(0); }
    const auto text=nativeLink_.Status(); std::array<wchar_t,2048> previous{};
    GetWindowTextW(nativeStatus_,previous.data(),static_cast<int>(previous.size()));
    if(text!=previous.data()) SetWindowTextW(nativeStatus_,text.c_str());
    UpdateGameProfileStatus();
}
void App::BindGameProfile(DWORD pid) {
    if(profileProcess_) CloseHandle(profileProcess_);
    profileProcess_=nullptr; profileExe_.clear(); profileName_.clear(); profileIssue_.clear();
    savedGameOptions_.reset(); pendingGameOptions_.reset();
    if(!pid) return;
    try {
        profileProcess_=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
        std::array<wchar_t,32768> path{}; DWORD length=static_cast<DWORD>(path.size());
        FILETIME created{},exited{},kernel{},user{};
        if(!profileProcess_ || WaitForSingleObject(profileProcess_,0)!=WAIT_TIMEOUT ||
            !QueryFullProcessImageNameW(profileProcess_,0,path.data(),&length) ||
            !GetProcessTimes(profileProcess_,&created,&exited,&kernel,&user))
            throw std::runtime_error("Bagli oyunun dosyasi belirlenemedi. Kayit kullanilamiyor.");
        profileExe_=std::wstring(path.data(),length);
        profileName_=pid==demoPid_ ? i18n::Text(L"Bağlantı denemesi") : gameState_.activeGame;
        if(profileName_.empty()) profileName_=profileExe_.stem().wstring();
        const auto session=std::make_pair(pid,(static_cast<ULONGLONG>(created.dwHighDateTime)<<32)|created.dwLowDateTime);
        const bool first=std::find(profileSessions_.begin(),profileSessions_.end(),session)==profileSessions_.end();
        if(first) profileSessions_.push_back(session);
        savedGameOptions_=gameProfiles_.Load(profileExe_);
        // Switching between running games must not discard temporary comparisons.
        if(first && savedGameOptions_) pendingGameOptions_=savedGameOptions_;
    } catch(const std::exception&) {
        profileIssue_=profileExe_.empty() ? i18n::Text(L"Oyun dosyası belirlenemedi · Kayıt kullanılamıyor")
            : i18n::Text(L"Kayıt okunamadı · Canlı ayarlar korundu");
    }
}
void App::UpdateGameProfileStatus() {
    const bool connected=nativePid_ && !profileExe_.empty() && profileProcess_ && WaitForSingleObject(profileProcess_,0)==WAIT_TIMEOUT;
    EnableWindow(saveGame_,connected && !pendingGameOptions_);
    EnableWindow(loadGame_,connected && !pendingGameOptions_);
    std::wstring text=i18n::Text(L"Kayıt için bağlı bir oyun veya deneme bekleniyor");
    if(connected) {
        text=profileName_+L" · ";
        if(!profileIssue_.empty()) text+=profileIssue_;
        else if(pendingGameOptions_) text+=i18n::Text(L"Kayıtlı ayar uygulanıyor...");
        else if(!savedGameOptions_) text+=i18n::Text(L"Kayıt yok · Beğendiğin ayarı kaydet");
        else {
            native::Options current;
            text+=nativeLink_.Read(current) && SameGameOptions(current,*savedGameOptions_)
                ? i18n::Text(L"Kayıtlı ayar etkin") : i18n::Text(L"Kaydedilmemiş değişiklikler");
        }
    } else if(!profileIssue_.empty()) text=profileIssue_;
    std::array<wchar_t,2048> previous{}; GetWindowTextW(profileStatus_,previous.data(),static_cast<int>(previous.size()));
    if(text!=previous.data()) SetWindowTextW(profileStatus_,text.c_str());
}
void App::GameProfileAction(bool save) {
    // Refresh detection at the click, so a stale selection cannot save for another game.
    if(SendMessageW(autoGame_,BM_GETCHECK,0,0)==BST_CHECKED) RefreshGames(false);
    const DWORD previousPid=nativePid_;
    RefreshNative();
    if(!nativePid_ || nativePid_!=previousPid || profileExe_.empty() || pendingGameOptions_ ||
        !profileProcess_ || WaitForSingleObject(profileProcess_,0)!=WAIT_TIMEOUT) return;
    try {
        if(save) {
            native::Options options;
            if(!nativeLink_.Read(options)) throw std::runtime_error("Canli ayar okunamadi. Tekrar kaydetmeyi dene.");
            gameProfiles_.Save(profileExe_,options);
            savedGameOptions_=options;
        } else {
            auto loaded=gameProfiles_.Load(profileExe_);
            savedGameOptions_=loaded;
            if(loaded) pendingGameOptions_=loaded;
        }
        profileIssue_.clear();
    } catch(const std::exception& error) {
        profileIssue_=save ? i18n::Text(L"Kaydedilemedi · Önceki kayıt korundu") : i18n::Text(L"Kayıt okunamadı · Canlı ayarlar korundu");
        Error(window_,error);
    }
    RefreshNative();
}
void App::InstallGame(bool remove) {
    std::array<wchar_t,32768> path{}; OPENFILENAMEW dialog{sizeof(dialog)}; dialog.hwndOwner=window_;
    dialog.lpstrFile=path.data(); dialog.nMaxFile=static_cast<DWORD>(path.size());
    dialog.lpstrTitle=remove?i18n::Text(L"Bağlantısı kaldırılacak oyunun EXE dosyasını seç"):i18n::Text(L"Bağlantı kurulacak oyunun EXE dosyasını seç");
    dialog.lpstrFilter=i18n::Text(L"64-bit oyun uygulaması\0*.exe\0\0"); dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&dialog)) return;
    if(remove) RemoveNative(path.data()); else InstallNative(ExecutableDirectory()/L"native/dxgi.dll",path.data());
    SetWindowTextW(nativeStatus_,remove?i18n::Text(L"Kendi bağlantımız kaldırıldı."):i18n::Text(L"Bağlantı kuruldu. Oyunu normal şekilde aç; algılama ve filtre durumu burada görünecek."));
}
void App::NativeDemo() {
    if(demoProcess_) { DWORD code=0; if(GetExitCodeProcess(demoProcess_,&code) && code==STILL_ACTIVE) return; CloseHandle(demoProcess_); demoProcess_=nullptr; }
    const auto path=ExecutableDirectory()/L"native/DLSSClearNativeTest.exe";
    SHELLEXECUTEINFOW info{sizeof(info)}; info.fMask=SEE_MASK_NOCLOSEPROCESS; info.hwnd=window_;
    info.lpFile=path.c_str(); info.lpParameters=L"--demo"; info.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&info)) throw std::runtime_error("Baglanti denemesi acilamadi. native klasorunu kontrol et.");
    demoProcess_=info.hProcess; demoPid_=GetProcessId(demoProcess_);
}
void App::UpdateHotkey(bool save) {
    try {
        if (save) gameHotkey_.Bind(static_cast<WORD>(SendMessageW(hotkey_,HKM_GETHOTKEY,0,0)),true);
        else gameHotkey_.Resume();
        SetWindowTextW(hotkeyStatus_,gameHotkey_.Choice() ? i18n::Text(L"Hazır · Uygulama açıkken oyunda aç/kapat") : i18n::Text(L"Kısayol kapalı · Yeni bir tuş seçebilirsin"));
    } catch (const std::exception& error) {
        SetWindowTextW(hotkeyStatus_,gameHotkey_.Active() ? i18n::Text(L"Yeni tuş atanamadı · Önceki kısayol hâlâ etkin") : i18n::Text(L"Kısayol etkin değil · Farklı bir tuş seçip kaydet"));
        if (save) Error(window_,error);
    }
}
void App::ToggleGame() {
    if (GetFocus()==hotkey_) return;
    if (SendMessageW(autoGame_,BM_GETCHECK,0,0)==BST_CHECKED) RefreshGames(false);
    RefreshNative();
    if (!nativePid_) { SetWindowTextW(hotkeyStatus_,i18n::Text(L"Tuş alındı · Bağlı bir oyun veya deneme bekleniyor")); return; }
    const bool enabled=SendMessageW(gameEnable_,BM_GETCHECK,0,0)!=BST_CHECKED;
    SendMessageW(gameEnable_,BM_SETCHECK,enabled?BST_CHECKED:BST_UNCHECKED,0);
    RefreshNative(true);
    SetWindowTextW(hotkeyStatus_,enabled?i18n::Text(L"Kısayol · Filtreyi açma isteği gönderildi"):i18n::Text(L"Kısayol · Filtreyi kapatma isteği gönderildi"));
}
void App::HideToTray() {
    // Never hide an app that has no working way back from the notification area.
    if (!trayIcon_.Add(window_,kTrayMessage)) {
        MessageBoxW(window_,i18n::Text(L"Sistem tepsisi simgesi oluşturulamadı. Pencere açık bırakıldı. Tekrar deneyebilirsin."),
            L"DLSSClear",MB_OK|MB_ICONINFORMATION);
        return;
    }
    if (GetCapture()==view_) ReleaseCapture();
    panning_=false;
    if (GetFocus()==hotkey_) SetFocus(window_);
    UpdateHotkey(false);
    hiddenToTray_=true;
    ShowWindow(window_,SW_HIDE);
}
void App::ShowFromTray() {
    hiddenToTray_=false;
    ShowWindow(window_,IsIconic(window_)?SW_RESTORE:SW_SHOW);
    SetForegroundWindow(window_);
    SetFocus(window_);
    renderer_.InvalidatePresentation();
}
void App::TrayMenu(POINT point) {
    HMENU menu=CreatePopupMenu();
    if (!menu) { ShowFromTray(); return; }
    AppendMenuW(menu,MF_STRING,kShowApp,i18n::Text(L"Uygulamayı aç"));
    AppendMenuW(menu,MF_STRING,kExit,i18n::Text(L"Uygulamayı kapat"));
    SetMenuDefaultItem(menu,kShowApp,FALSE);
    if (point.x==-1 && point.y==-1) GetCursorPos(&point);
    SetForegroundWindow(window_);
    const UINT selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,
        point.x,point.y,0,window_,nullptr);
    DestroyMenu(menu);
    PostMessageW(window_,WM_NULL,0,0); // Lets a dismissed notification-area menu close normally.
    if (selected==kShowApp) ShowFromTray();
    else if (selected==kExit) DestroyWindow(window_);
}
LRESULT CALLBACK App::HotkeyProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR data) {
    auto* app=reinterpret_cast<App*>(data);
    if (message==WM_SETFOCUS) {
        app->gameHotkey_.Suspend();
        SetWindowTextW(app->hotkeyStatus_,i18n::Text(L"Tuşa bas → Tuşu kaydet · Backspace: kısayolu sil"));
    } else if (message==WM_KILLFOCUS) app->UpdateHotkey(false);
    return DefSubclassProc(window,message,wp,lp);
}
void App::Layout() {
    if (!view_) return;
    RECT r{}; GetClientRect(window_, &r);
    MoveWindow(view_, 0, kToolbar, r.right, std::max(1L, r.bottom - kToolbar - kStatus), TRUE);
    MoveWindow(status_, 12, std::max(0L, r.bottom - kStatus + 5), std::max(1L, r.right - 24), 26, TRUE);
    if (ready_) renderer_.Resize(std::max(1L, r.right), std::max(1L, r.bottom - kToolbar - kStatus));
}
void App::SetImage(const Image& image, const std::wstring& name) {
    renderer_.Upload(image); imageName_ = name; settings_.panX = settings_.panY = 0;
    SetWindowTextW(window_, (L"DLSSClear · " + name + i18n::Text(L" · Bağımsız DX11 prototipi")).c_str());
    UpdateStatus();
}
void App::UpdateStatus() {
    std::wostringstream text;
    text << renderer_.Width() << L" × " << renderer_.Height() << L"  |  " << renderer_.Adapter() << L"  |  ";
    if (renderer_.Software()) text << i18n::Text(L"WARP yazılım · GPU performansı değildir");
    else if (renderer_.GpuMs() < 0) text << i18n::Text(L"Son filtre süresi henüz yok");
    else text << i18n::Text(L"Son filtre: ") << std::fixed << std::setprecision(3) << renderer_.GpuMs() << L" ms GPU";
    text << i18n::Text(L"  |  Filtre çalıştırma: ") << renderer_.FilterDrawCount() << i18n::Text(L" · değişmedikçe sonuç korunur");
    // Avoid repainting the UI and waking the renderer for identical status text.
    std::array<wchar_t, 2048> previous{}; GetWindowTextW(status_, previous.data(), static_cast<int>(previous.size()));
    if (text.str() != previous.data()) SetWindowTextW(status_, text.str().c_str());
}
void App::Open() {
    std::array<wchar_t, 32768> path{};
    OPENFILENAMEW dialog{sizeof(dialog)}; dialog.hwndOwner = window_; dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrFilter = i18n::Text(L"SDR görüntüler\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff\0Tüm dosyalar\0*.*\0");
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        auto image = LoadImageFile(path.data()); SetImage(image, std::filesystem::path(path.data()).filename().wstring());
        sourcePath_ = path.data();
    }
}
void App::Save() {
    std::array<wchar_t, 32768> path{};
    wcscpy_s(path.data(), path.size(), L"DLSSClear-enhanced.png");
    OPENFILENAMEW dialog{sizeof(dialog)}; dialog.hwndOwner = window_; dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size()); dialog.lpstrFilter = i18n::Text(L"PNG görüntüsü\0*.png\0");
    dialog.lpstrDefExt = L"png"; dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetSaveFileNameW(&dialog)) {
        std::filesystem::path target(path.data());
        if (!sourcePath_.empty() && std::filesystem::exists(target) && std::filesystem::equivalent(target, sourcePath_))
            throw std::runtime_error("Kaynak goruntuyu korumak icin farkli bir dosya adi secin.");
        renderer_.Process(settings_); SavePng(target, renderer_.Readback());
        SetWindowTextW(status_, i18n::Text(L"İşlenmiş görüntü tam kaynak çözünürlüğünde PNG olarak kaydedildi."));
    }
}
LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        app->window_ = window; SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app) {
        try { return app->Handle(message, wp, lp); } catch (const std::exception& error) { Error(window, error); return 0; }
    }
    return DefWindowProcW(window, message, wp, lp);
}
LRESULT CALLBACK App::ViewProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (app) {
        if (message == WM_ERASEBKGND) return 1;
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{}; BeginPaint(window, &paint); EndPaint(window, &paint);
            app->renderer_.InvalidatePresentation(); return 0;
        }
        if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN) {
            SetFocus(app->window_); SetCapture(window); app->panning_ = message == WM_RBUTTONDOWN;
            app->lastMouse_ = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        }
        if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN) {
            if (GetCapture() == window) {
                int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
                if (app->panning_) {
                    app->settings_.panX += static_cast<float>(x - app->lastMouse_.x);
                    app->settings_.panY += static_cast<float>(y - app->lastMouse_.y);
                    app->lastMouse_ = {x, y};
                } else {
                    RECT rect{}; GetClientRect(window, &rect);
                    app->settings_.split = std::clamp(static_cast<float>(x) / std::max(1L, rect.right), 0.f, 1.f);
                }
            }
        }
        if (message == WM_LBUTTONUP || message == WM_RBUTTONUP) { if (GetCapture() == window) ReleaseCapture(); }
    }
    return DefWindowProcW(window, message, wp, lp);
}
LRESULT App::Handle(UINT message, WPARAM wp, LPARAM lp) {
    if (showAppMessage_ && message==showAppMessage_) { ShowFromTray(); return 0; }
    if (taskbarCreated_ && message==taskbarCreated_) {
        trayIcon_.ExplorerRestarted();
        if (!trayIcon_.Add(window_,kTrayMessage) && hiddenToTray_) ShowFromTray();
        return 0;
    }
    switch (message) {
    case WM_CLOSE: HideToTray(); return 0;
    case WM_QUERYENDSESSION: return TRUE;
    case WM_ENDSESSION: if (wp) DestroyWindow(window_); return 0;
    case kTrayMessage:
        // NOTIFYICON_VERSION_4 puts the event in LOWORD(lParam).
        if (LOWORD(lp)==NIN_SELECT || LOWORD(lp)==NIN_KEYSELECT) ShowFromTray();
        else if (LOWORD(lp)==WM_CONTEXTMENU) TrayMenu({GET_X_LPARAM(wp),GET_Y_LPARAM(wp)});
        return 0;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize = {1060, 900}; return 0;
    case WM_SIZE:
        minimized_ = wp == SIZE_MINIMIZED; if (!minimized_) Layout(); return 0;
    case WM_HSCROLL: UpdateSettings(); return 0;
    case WM_HOTKEY:
        if (gameHotkey_.Matches(wp,lp)) ToggleGame();
        return 0;
    case WM_TIMER:
        if (!hiddenToTray_) UpdateStatus();
        if (SendMessageW(autoGame_, BM_GETCHECK, 0, 0) == BST_CHECKED && GetTickCount64() >= nextGameCheck_) RefreshGames(false);
        RefreshNative();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case kOpen: Open(); break;
        case kSave: Save(); break;
        case kDemo: SetImage(MakeTestImage(), i18n::Text(L"Sentetik test görüntüsü")); sourcePath_.clear(); break;
        case kReset: Reset(); break;
        case kPreset:
            if (HIWORD(wp) == CBN_SELCHANGE) ApplyPreset(static_cast<unsigned>(SendMessageW(preset_, CB_GETCURSEL, 0, 0)));
            break;
        case kNative: settings_.panX = settings_.panY = 0; break;
        case kMode: case kRing: case kRadius: UpdateSettings(); break;
        case kRefreshGame: RefreshGames(true); break;
        case kGameEnable: case kGameSplit: RefreshNative(true); break;
        case kGameProfile: if(HIWORD(wp)==CBN_SELCHANGE) RefreshNative(true); break;
        case kSaveGame: GameProfileAction(true); break;
        case kLoadGame: GameProfileAction(false); break;
        case kInstall: InstallGame(false); break;
        case kRemove: InstallGame(true); break;
        case kNativeDemo: NativeDemo(); break;
        case kSaveHotkey: UpdateHotkey(true); break;
        case kExit: DestroyWindow(window_); break;
        case kLanguage: if(HIWORD(wp)==CBN_SELCHANGE) ApplyLanguage(true); break;
        case kAutoGame:
            if (SendMessageW(autoGame_, BM_GETCHECK, 0, 0) == BST_CHECKED) RefreshGames(true);
            else SetWindowTextW(gameStatus_, i18n::Text(L"Otomatik algılama kapalı. Güncel durum için Yenile'ye bas."));
            break;
        case kGameInfo:
            RefreshGames(false);
            MessageBoxW(window_, (DescribeGames(gameState_) +
                L"\n\n" + std::wstring(i18n::Text(i18n::kGameHelpKey))).c_str(), i18n::Text(L"DLSSClear · oyun bağlantısı"), MB_OK | MB_ICONINFORMATION);
            break;
        }
        return 0;
    case WM_KEYDOWN:
        if (wp >= '1' && wp <= '4') { SendMessageW(mode_, CB_SETCURSEL, wp - '1', 0); UpdateSettings(); }
        else if (wp == 'O') Open();
        else if (wp == 'S') Save();
        else if (wp == 'R') Reset();
        else if (wp == 'Z') {
            SendMessageW(native_, BM_SETCHECK, SendMessageW(native_, BM_GETCHECK, 0, 0) == BST_CHECKED ? BST_UNCHECKED : BST_CHECKED, 0);
            settings_.panX = settings_.panY = 0;
        } else if (wp == VK_F5) renderer_.ReloadShaders();
        return 0;
    case WM_DESTROY: trayIcon_.Remove(); gameHotkey_.Suspend(); KillTimer(window_, 1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window_, message, wp, lp);
}
int App::Run(HINSTANCE instance, int show, const std::filesystem::path& initialImage, bool smoke) {
    WNDCLASSW cls{}; cls.hInstance = instance; cls.lpszClassName = L"DLSSClearWindow";
    cls.lpfnWndProc = WindowProc; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    if (!RegisterClassW(&cls)) throw std::runtime_error("Window class failed.");
    cls.lpszClassName = L"DLSSClearView"; cls.lpfnWndProc = ViewProc; cls.hbrBackground = nullptr;
    if (!RegisterClassW(&cls)) throw std::runtime_error("View class failed.");
    window_ = CreateWindowW(L"DLSSClearWindow", L"DLSSClear", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1380, 980, nullptr, nullptr, instance, this);
    if (!window_) throw std::runtime_error("Window creation failed.");
    if (!smoke) {
        taskbarCreated_=RegisterWindowMessageW(L"TaskbarCreated");
        showAppMessage_=ShowAppMessage();
        if (!taskbarCreated_ || !showAppMessage_) throw std::runtime_error("Arka plan pencere iletisimi baslatilamadi.");
    }
    BuildControls();
    if (!smoke) {
        gameHotkey_.Initialize(window_);
        SendMessageW(hotkey_,HKM_SETHOTKEY,gameHotkey_.Choice(),0);
        UpdateHotkey(false);
    }
    if (!smoke) RefreshGames(true);
    view_ = CreateWindowW(L"DLSSClearView", L"", WS_CHILD | WS_VISIBLE, 0, kToolbar, 1280, 720, window_, nullptr, instance, this);
    if (!view_) throw std::runtime_error("Viewport creation failed.");
    Layout(); renderer_.Initialize(view_, ShaderPath()); ready_ = true;
    if (!initialImage.empty()) {
        SetImage(LoadImageFile(initialImage), initialImage.filename().wstring()); sourcePath_ = initialImage;
    } else SetImage(MakeTestImage(), i18n::Text(L"Sentetik test görüntüsü"));
    if (!smoke) { ShowWindow(window_, show); trayIcon_.Add(window_,kTrayMessage); }
    SetTimer(window_, 1, 500, nullptr);
    MSG message{}; bool running = true; unsigned frames = 0;
    while (running) {
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { running = false; break; }
            // Keep shortcuts working after a button or slider has focus.
            if (GetFocus()!=hotkey_ && message.message == WM_KEYDOWN && (message.wParam == 'O' || message.wParam == 'S' || message.wParam == 'R' ||
                message.wParam == 'Z' || message.wParam == VK_F5 || (message.wParam >= '1' && message.wParam <= '4'))) {
                SendMessageW(window_, WM_KEYDOWN, message.wParam, message.lParam); continue;
            }
            const bool editingHotkey = message.hwnd==hotkey_ && !(message.message==WM_KEYDOWN && message.wParam==VK_TAB);
            if (editingHotkey || !IsDialogMessageW(window_, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
        }
        if (!running) break;
        if (minimized_ || hiddenToTray_) { WaitMessage(); continue; }
        bool presented = renderer_.Present(settings_, SendMessageW(native_, BM_GETCHECK, 0, 0) == BST_CHECKED, smoke);
        if (!presented && !smoke)
            MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (smoke) {
            ++frames;
            if (frames == 8) { SetWindowPos(window_, nullptr, 0, 0, 1150, 760, SWP_NOMOVE | SWP_NOZORDER); ApplyPreset(0); SendMessageW(window_, WM_KEYDOWN, '1', 0); }
            if (frames == 16) { ApplyPreset(2); SendMessageW(window_, WM_KEYDOWN, '2', 0); SendMessageW(native_, BM_SETCHECK, BST_CHECKED, 0); }
            if (frames == 24) { ApplyPreset(1); SendMessageW(window_, WM_KEYDOWN, '4', 0); renderer_.ReloadShaders(); }
            if (frames == 28) ApplyPreset(3);
            if (frames == 30) ApplyPreset(4);
            if (frames == 32) { DestroyWindow(window_); running = false; }
        }
    }
    if (font_) DeleteObject(font_);
    if (demoProcess_) CloseHandle(demoProcess_);
    if (smoke && (renderer_.FilterDrawCount() != 6 || renderer_.DisplayDrawCount() != 32))
        throw std::runtime_error("UI smoke: unexpected filter/display cache behavior");
    if (smoke && renderer_.DebugErrors()) throw std::runtime_error("UI smoke: D3D11 warnings/errors");
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES|ICC_HOTKEY_CLASS}; InitCommonControlsEx(&controls);
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    if (argv) { for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]); LocalFree(argv); }
    int result = 1;
    try {
        i18n::RefreshPreference();
        for(auto it=args.begin();it!=args.end();) {
            if(it->starts_with(L"--language=")) { i18n::Set(i18n::Parse(it->substr(11))); it=args.erase(it); }
            else ++it;
        }
        if (!args.empty() && args[0] == L"--self-test") {
            auto destination = args.size() > 1 ? std::filesystem::path(args[1]) : ExecutableDirectory() / L"validation";
            std::filesystem::path reference;
            for (size_t i = 2; i < args.size(); ++i) {
                if (args[i] == L"--reference" && i + 1 < args.size()) reference = args[++i];
                else if (args[i] != L"--warp") throw std::runtime_error("Unknown or incomplete self-test option.");
            }
            result = RunValidation(ShaderPath(), destination, std::find(args.begin(), args.end(), L"--warp") != args.end(), reference);
        } else if (!args.empty() && args[0] == L"--localization-test") {
            result = RunLocalizationValidation(args.size()>1 ? std::filesystem::path(args[1]) : ExecutableDirectory()/L"localization-validation");
        } else if (!args.empty() && args[0] == L"--profile-test") {
            result = RunGameProfileValidation(args.size()>1 ? std::filesystem::path(args[1]) : ExecutableDirectory()/L"profile-validation");
        } else if (!args.empty() && args[0] == L"--detect-games") {
            if (args.size() != 2) throw std::runtime_error("--detect-games requires an output path");
            const auto installations = DiscoverSteamGames(); const auto status = DetectGames(installations);
            std::ofstream log(std::filesystem::path(args[1]), std::ios::binary);
            if (!log) throw std::runtime_error("Cannot write detection report");
            log << "Steam installations: " << installations.size() << "\nActive game: " << std::filesystem::path(status.activeGame).string() << '\n';
            for (const auto& game : status.running) log << "Running: " << std::filesystem::path(game).string() << '\n';
            result = 0;
        } else {
            bool smoke = !args.empty() && args[0] == L"--smoke-test";
            std::filesystem::path input = !args.empty() && !smoke ? args[0] : L"";
            AppInstance resident;
            if (smoke || resident.Start()) {
                App app; result = app.Run(instance, show, input, smoke);
            } else result=0;
        }
    } catch (const std::exception& error) {
        if (!args.empty() && (args[0] == L"--smoke-test" || args[0] == L"--profile-test" || args[0] == L"--localization-test")) {
            std::ofstream log(ExecutableDirectory() / "smoke-error.txt"); log << error.what();
        } else Error(nullptr, error);
    }
    CoUninitialize(); return result;
}
