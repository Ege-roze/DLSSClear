#pragma once
#include "../native/Protocol.h"
#include <filesystem>

class NativeLink {
public:
    ~NativeLink();
    bool Connect(DWORD pid);
    bool Apply(native::Options options);
    bool Read(native::Options& options) const { return native::Read(shared_,options); }
    std::wstring Status() const;
private:
    void Close();
    DWORD pid_ = 0;
    HANDLE mapping_ = nullptr;
    native::Shared* shared_ = nullptr;
    bool legacy_=false;
};
void InstallNative(const std::filesystem::path& sourceDll, const std::filesystem::path& gameExe);
void RemoveNative(const std::filesystem::path& gameExe);
