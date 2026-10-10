#include "Localization.h"
#include "NativeLink.h"
#include <TlHelp32.h>
#include <bcrypt.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>

void NativeLink::Close() { if (shared_) UnmapViewOfFile(shared_); if (mapping_) CloseHandle(mapping_); shared_=nullptr; mapping_=nullptr; pid_=0; legacy_=false; }
NativeLink::~NativeLink() { Close(); }
bool NativeLink::Connect(DWORD pid) {
    if(pid && pid==pid_ && native::Valid(shared_)) return true;
    Close(); if(!pid) return false;
    mapping_=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,native::MappingName(pid).c_str());
    if(!mapping_) {
        HANDLE old=OpenFileMappingW(FILE_MAP_READ,FALSE,(L"Local\\DLSSClear.Native.v1."+std::to_wstring(pid)).c_str());
        if(old) { legacy_=true; CloseHandle(old); }
        return false;
    }
    shared_=static_cast<native::Shared*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(native::Shared)));
    if(!native::Valid(shared_) || shared_->pid!=static_cast<LONG>(pid)) { Close(); return false; }
    pid_=pid; return true;
}
bool NativeLink::Apply(native::Options options) { return native::Write(shared_,options); }
std::wstring NativeLink::Status() const {
    if(legacy_) return i18n::Text(L"Eski oyun bağlantısı çalışıyor. Oyunu kapatıp Oyuna kur ile güncelle; sonra yeniden aç.");
    if(!native::Valid(shared_)) return i18n::Text(L"Bağlantı bekleniyor. Oyuna kur veya Bağlantı denemesi'ni aç.");
    const LONG state=InterlockedCompareExchange(&shared_->state,0,0);
    const auto frames=InterlockedCompareExchange64(&shared_->frames,0,0);
    switch(state) {
    case native::Active11: case native::Active12:
        return std::wstring(state==native::Active11?i18n::Text(L"Kendi bağlantımız · DX11"):i18n::Text(L"Kendi bağlantımız · DX12")) + i18n::Text(L" · İşlenen kare: ") + std::to_wstring(frames) +
            (InterlockedCompareExchange(&shared_->menuKeysAvailable,0,0)<0 ? i18n::Text(L" · Panel tuşu başka uygulamada kullanılıyor") : i18n::Text(L" · Home: oyun içi panel"));
    case native::Disabled: return i18n::Text(L"Bağlı · Oyun filtresi kapalı");
    case native::Unsupported: return i18n::Text(L"Bağlı · Bu görüntü yolu desteklenmiyor; filtre uygulanmıyor (SDR gerekir).");
    case native::Failed: return i18n::Text(L"Bağlı · Filtre hata nedeniyle devre dışı. DLSSClearNative.log dosyasına bak.");
    case native::GpuBusy: return i18n::Text(L"Bağlı · GPU meşgul; filtre bu karede atlandı.");
    default: return i18n::Text(L"Bağlı · İlk görüntü karesi bekleniyor.");
    }
}
namespace {
std::string Hash(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) throw std::runtime_error("SHA256 provider failed");
    std::array<BYTE,32> digest{};
    NTSTATUS status=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
    std::ifstream file(path,std::ios::binary); if(!file) status=-1;
    std::array<char,65536> bytes{};
    while(status>=0 && file) { file.read(bytes.data(),bytes.size()); auto size=file.gcount(); if(size) status=BCryptHashData(hash,reinterpret_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(size),0); }
    if(file.bad()) status=-1;
    if(status>=0) status=BCryptFinishHash(hash,digest.data(),static_cast<ULONG>(digest.size()),0);
    if(hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm,0);
    if(status<0) throw std::runtime_error("Dosya dogrulamasi basarisiz.");
    constexpr char hex[]="0123456789abcdef"; std::string result;
    for(BYTE byte:digest) { result+=hex[byte>>4]; result+=hex[byte&15]; } return result;
}
void CheckTarget(const std::filesystem::path& game) {
    DWORD type=0;
    if(!game.is_absolute() || !GetBinaryTypeW(game.c_str(),&type) || type!=SCS_64BIT_BINARY) throw std::runtime_error("64-bit oyun EXE dosyasini sec.");
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error("Calisan uygulamalar kontrol edilemedi.");
    bool running=false;
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry)) do {
        HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID); if(!process) continue;
        std::array<wchar_t,32768> path{}; DWORD size=static_cast<DWORD>(path.size());
        if(QueryFullProcessImageNameW(process,0,path.data(),&size)) {
            std::error_code error; if(std::filesystem::equivalent(game,path.data(),error)) running=true;
        }
        CloseHandle(process);
    } while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);
    if(running) throw std::runtime_error("Baglantiyi degistirmeden once oyunu kapat.");
}
std::string RecordedHash(const std::filesystem::path& marker) {
    std::ifstream file(marker); std::string magic,hash; std::getline(file,magic); std::getline(file,hash);
    return magic=="DLSSClearNative v1" && hash.size()==64 ? hash : std::string{};
}
}
void InstallNative(const std::filesystem::path& source,const std::filesystem::path& game) {
    CheckTarget(game);
    if(!std::filesystem::is_regular_file(source)) throw std::runtime_error("native/dxgi.dll bulunamadi. Cozumun x64 surumunu derle.");
    const auto target=game.parent_path()/L"dxgi.dll", marker=game.parent_path()/L"DLSSClearNative.install.txt";
    const bool exists=std::filesystem::exists(target);
    if(!exists && std::filesystem::exists(marker)) throw std::runtime_error("Onceki kurulum kaydi mevcut. Klasoru kontrol et.");
    if(exists && RecordedHash(marker)!=Hash(target)) throw std::runtime_error("Bu klasorde baska bir grafik baglantisi var. Uzerine yazilmadi; mevcut baglantiyi once kaldir.");
    const auto hash=Hash(source);
    if(exists && hash==Hash(target)) return;
    const auto temporary=game.parent_path()/L"DLSSClearNative.new.dll";
    if(std::filesystem::exists(temporary)) throw std::runtime_error("Gecici kurulum dosyasi mevcut. Klasoru kontrol et.");
    if(!CopyFileW(source.c_str(),temporary.c_str(),TRUE)) throw std::runtime_error("Oyun klasorune yazilamadi.");
    const auto previous=RecordedHash(marker);
    try {
        if(Hash(temporary)!=hash) throw std::runtime_error("DLL kopyasi dogrulanamadi.");
        { std::ofstream output(marker,std::ios::trunc); output<<"DLSSClearNative v1\n"<<hash<<'\n'; output.close(); if(!output) throw std::runtime_error("Kurulum kaydi yazilamadi."); }
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Baglanti dosyasi yerlestirilemedi.");
    } catch(...) {
        DeleteFileW(temporary.c_str());
        if(previous.empty()) DeleteFileW(marker.c_str()); else { std::ofstream old(marker); old<<"DLSSClearNative v1\n"<<previous<<'\n'; }
        throw;
    }
}
void RemoveNative(const std::filesystem::path& game) {
    CheckTarget(game);
    const auto target=game.parent_path()/L"dxgi.dll", marker=game.parent_path()/L"DLSSClearNative.install.txt";
    if(!std::filesystem::exists(target) || RecordedHash(marker)!=Hash(target)) throw std::runtime_error("Bu klasorde dogrulanmis DLSSClear baglantisi bulunamadi. Dosyalar degistirilmedi.");
    if(!DeleteFileW(target.c_str())) throw std::runtime_error("Baglanti dosyasi kaldirilamadi.");
    DeleteFileW(marker.c_str());
}
