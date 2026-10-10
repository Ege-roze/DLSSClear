#pragma once
#include <Windows.h>
#include <shlobj.h>
#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>
#include <stdexcept>
#include "Translations.h"

namespace i18n {
enum class Language { Turkish, English, SimplifiedChinese };
inline Language DefaultLanguage() {
    const auto primary=PRIMARYLANGID(GetUserDefaultUILanguage());
    return primary==LANG_TURKISH?Language::Turkish:primary==LANG_CHINESE?Language::SimplifiedChinese:Language::English;
}
inline std::atomic<Language> language{DefaultLanguage()};
inline Language Current() { return language.load(std::memory_order_relaxed); }
inline void Set(Language value) { language.store(value,std::memory_order_relaxed); }
inline const wchar_t* Code(Language value) {
    return value==Language::Turkish?L"tr":value==Language::SimplifiedChinese?L"zh-CN":L"en";
}
inline Language Parse(std::wstring_view value) {
    if(value==L"tr") return Language::Turkish;
    if(value==L"en") return Language::English;
    if(value==L"zh-CN") return Language::SimplifiedChinese;
    throw std::runtime_error("Unsupported language. Use tr, en or zh-CN.");
}
inline const wchar_t* Text(const wchar_t* key,Language selected=Current()) {
    for(const auto& entry:kTranslations) if(wcscmp(entry.tr,key)==0)
        return selected==Language::English?entry.en:selected==Language::SimplifiedChinese?entry.zh:entry.tr;
    return key;
}
// Recover stable keys when registering already translated Win32 control labels.
inline const wchar_t* Key(const wchar_t* value) {
    for(const auto& entry:kTranslations)
        if(wcscmp(entry.tr,value)==0 || wcscmp(entry.en,value)==0 || wcscmp(entry.zh,value)==0) return entry.tr;
    return value;
}
inline const wchar_t* FontFace(Language selected=Current()) { return selected==Language::SimplifiedChinese?L"Microsoft YaHei UI":L"Segoe UI"; }
inline std::filesystem::path SettingsPath() {
    PWSTR folder=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&folder))) throw std::runtime_error("Language settings folder unavailable.");
    std::filesystem::path path(folder); CoTaskMemFree(folder);
    return path/L"DLSSClear"/L"settings.ini";
}
inline Language ReadPreference(const std::filesystem::path& path) {
    wchar_t code[32]{};
    GetPrivateProfileStringW(L"UI",L"Language",Code(DefaultLanguage()),code,32,path.c_str());
    try { return Parse(code); } catch(...) { return DefaultLanguage(); }
}
inline void RefreshPreference() noexcept {
    try { Set(ReadPreference(SettingsPath())); } catch(...) {} // Keep current language on an unreadable preference.
}
inline void SavePreference(Language selected,const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    if(!WritePrivateProfileStringW(L"UI",L"Language",Code(selected),path.c_str())) throw std::runtime_error("Dil kaydedilemedi.");
}
inline void SavePreference(Language selected) { SavePreference(selected,SettingsPath()); }
}
