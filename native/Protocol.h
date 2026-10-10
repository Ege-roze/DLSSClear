#pragma once
#include <Windows.h>
#include <algorithm>
#include <string>
#include <Unknwn.h>

namespace native {
// Test host reads pixels before DXGI rotates/presents the backbuffer. This invokes
// the identical Render path used by Present, without the subsequent DXGI call.
struct __declspec(uuid("F4CC6F47-1E20-4653-8448-E8D25F8D1539")) Diagnostics : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE ProcessForValidation() = 0;
    virtual HRESULT STDMETHODCALLTYPE MenuForValidation(BOOL visible, UINT selection) = 0;
};
constexpr LONG kMagic = 0x444c5343, kVersion = 2;
struct Options {
    LONG enabled = 1, strength = 650, detail = 200, noise = 12, antiRinging = 1, split = 0;
};
enum State : LONG { Waiting, Active11, Active12, Disabled, Unsupported, Failed, GpuBusy };
struct alignas(8) Shared {
    LONG magic = kMagic, version = kVersion, size = sizeof(Shared), pid = 0;
    volatile LONG sequence = 0;
    Options options;
    volatile LONG state = Waiting, width = 0, height = 0, error = 0;
    alignas(8) volatile LONG64 frames = 0;
    volatile LONG menuVisible=0, menuSelection=0, menuKeysAvailable=0;
};
inline std::wstring MappingName(DWORD pid) { return L"Local\\DLSSClear.Native.v2." + std::to_wstring(pid); }
inline bool Valid(const Shared* p) { return p && p->magic == kMagic && p->version == kVersion && p->size == sizeof(Shared); }
inline Options Clamp(Options value) {
    value.enabled = !!value.enabled; value.strength = std::clamp<LONG>(value.strength, 0, 2000);
    value.detail = std::clamp<LONG>(value.detail, 0, 1000); value.noise = std::clamp<LONG>(value.noise, 0, 50);
    value.antiRinging = !!value.antiRinging; value.split = !!value.split; return value;
}
inline bool Read(Shared* p, Options& value) {
    if (!Valid(p)) return false;
    const LONG sequence = InterlockedCompareExchange(&p->sequence, 0, 0);
    if (sequence & 1) return false;
    const auto copy = p->options; MemoryBarrier();
    if (sequence != InterlockedCompareExchange(&p->sequence, 0, 0)) return false;
    value = Clamp(copy); return true;
}
inline bool Write(Shared* p, Options value) {
    if (!Valid(p)) return false;
    const LONG sequence = InterlockedCompareExchange(&p->sequence, 0, 0);
    if ((sequence & 1) || InterlockedCompareExchange(&p->sequence, sequence + 1, sequence) != sequence) return false;
    p->options = Clamp(value); MemoryBarrier(); InterlockedIncrement(&p->sequence); return true;
}
struct Constants {
    UINT width, height; float strength, detail, noise, antiRinging;
    UINT mode; float split; float viewWidth, viewHeight, scale; UINT radius;
    float panX, panY, padding2, padding3;
};
static_assert(sizeof(Constants) == 64);
inline Constants Data(UINT w, UINT h, Options o) {
    return {w, h, o.strength / 1000.f, o.detail / 1000.f, o.noise / 1000.f,
        static_cast<float>(o.antiRinging), 0, 0, 0, 0, 1, 1, 0, 0, 0, 0};
}
}
