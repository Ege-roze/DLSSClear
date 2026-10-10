#include "Filter.h"
#include <atomic>
#include <array>
#include <filesystem>
#include <mutex>
#include <new>

namespace {
HMODULE module = nullptr;
HMODULE SystemDxgi() {
    static HMODULE real = [] {
        wchar_t directory[MAX_PATH]{};
        if (!GetSystemDirectoryW(directory, MAX_PATH)) return HMODULE(nullptr);
        const auto path = std::filesystem::path(directory) / L"dxgi.dll";
        return LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    }();
    return real;
}
}
extern "C" FARPROC NativeResolve(const char* name) {
    auto real = SystemDxgi(); auto entry = real ? GetProcAddress(real, name) : nullptr;
    if (!entry) RaiseFailFastException(nullptr, nullptr, 0); // Preserve ABI; never jump through null.
    return entry;
}
namespace native {
void Check(HRESULT hr) { if (FAILED(hr)) throw hr; }
void Log(const wchar_t* message) {
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(module, path, 32768);
    if (!length || length >= 32768) return;
    auto file = std::filesystem::path(path).parent_path() / L"DLSSClearNative.log";
    HANDLE handle = CreateFileW(file.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return;
    const std::wstring text = std::to_wstring(GetCurrentProcessId()) + L" | " + message + L"\r\n";
    std::array<char, 4096> utf8{};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), static_cast<int>(utf8.size()), nullptr, nullptr);
    DWORD written = 0; if (size) WriteFile(handle, utf8.data(), static_cast<DWORD>(size), &written, nullptr);
    CloseHandle(handle);
}
struct Runtime {
    HANDLE mapping = nullptr;
    Shared* shared = nullptr;
    MenuInput* input=nullptr;
    Runtime() {
        mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Shared), MappingName(GetCurrentProcessId()).c_str());
        if (mapping) shared = static_cast<Shared*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
        if (shared) {
            Shared initial{}; initial.magic = 0; initial.pid = static_cast<LONG>(GetCurrentProcessId());
            memcpy(shared, &initial, sizeof(initial)); InterlockedExchange(&shared->magic, kMagic);
        }
        input=new MenuInput(shared);
        Log(L"DLSSClear native v2 loaded; Home comparison panel; no ReShade dependency.");
    }
};
Runtime& RuntimeState() { static auto* runtime = new Runtime; return *runtime; } // OS releases IPC at process exit; no loader-lock teardown.

class Swap final : public IDXGISwapChain4, public Diagnostics {
    std::atomic<ULONG> references_{1};
    Ptr<IDXGISwapChain4> real_;
    Ptr<IUnknown> device_, parent_;
    std::unique_ptr<Filter> filter_;
    std::mutex mutex_;
    Options options_{};
    Menu menu_;
    DXGI_COLOR_SPACE_TYPE color_ = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    bool failed_ = false, logged_ = false;
    bool Reset() {
        if (filter_ && !filter_->Drain()) return false;
        filter_.reset(); failed_ = false; return true;
    }
    void Render(UINT flags,bool validation=false) noexcept {
        if (flags & (DXGI_PRESENT_TEST | DXGI_PRESENT_DO_NOT_WAIT | DXGI_PRESENT_RESTART)) return;
        auto& runtime=RuntimeState(); auto* shared = runtime.shared;
        Read(shared, options_);
        auto status = [&](State state) { if (shared) InterlockedExchange(&shared->state, state); };
        try {
            DXGI_SWAP_CHAIN_DESC1 desc{}; Check(real_->GetDesc1(&desc));
            HWND window=nullptr; real_->GetHwnd(&window);
            const bool usable=color_==DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 && Sdr(desc.Format) && desc.SampleDesc.Count==1 && !(desc.Flags&DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED) && !failed_;
            if(!validation) runtime.input->Arm(window,usable);
            if(!usable) { menu_.ForValidation(false,0); if(shared) InterlockedExchange(&shared->menuVisible,0); status(failed_?Failed:Unsupported); return; }
            menu_.Update(window,desc.Width,desc.Height,shared,options_,validation?nullptr:runtime.input);
            const auto* menu=menu_.Image();
            if(!options_.enabled && !menu) { status(Disabled); return; }
            if (!filter_) { filter_ = Create11(device_.Get()); if (!filter_) filter_ = Create12(device_.Get()); }
            if (!filter_) { status(Unsupported); return; }
            UINT width = 0, height = 0; const auto state = filter_->Render(real_.Get(), options_, width, height,menu);
            if (shared) { InterlockedExchange(&shared->width, width); InterlockedExchange(&shared->height, height); }
            status(state);
            if (state == Active11 || state == Active12) {
                if (shared) InterlockedIncrement64(&shared->frames);
                if (!logged_) { Log(state == Active11 ? L"Filter active: DirectX 11 SDR." : L"Filter active: DirectX 12 SDR."); logged_ = true; }
            }
        } catch (HRESULT error) {
            failed_ = true; status(Failed); if (shared) InterlockedExchange(&shared->error, error);
            Log((L"Filter bypassed after HRESULT " + std::to_wstring(static_cast<UINT>(error))).c_str());
        } catch (...) { failed_ = true; status(Failed); Log(L"Filter bypassed after unexpected error."); }
    }
public:
    Swap(IDXGISwapChain4* swap, IUnknown* device, IUnknown* parent) : real_(swap), device_(device), parent_(parent) { RuntimeState(); }
    ~Swap() {
        if (!Reset()) { filter_.release(); Log(L"GPU drain timed out; retained in-flight resources until process exit."); }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == __uuidof(Diagnostics)) { *out = static_cast<Diagnostics*>(this); AddRef(); return S_OK; }
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDXGIObject) || iid == __uuidof(IDXGIDeviceSubObject) ||
            iid == __uuidof(IDXGISwapChain) || iid == __uuidof(IDXGISwapChain1) || iid == __uuidof(IDXGISwapChain2) ||
            iid == __uuidof(IDXGISwapChain3) || iid == __uuidof(IDXGISwapChain4)) { *out = static_cast<IDXGISwapChain4*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { auto left = --references_; if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE ProcessForValidation() override { std::lock_guard lock(mutex_); Render(0,true); return failed_ ? E_FAIL : S_OK; }
    HRESULT STDMETHODCALLTYPE MenuForValidation(BOOL visible,UINT selection) override { std::lock_guard lock(mutex_); menu_.ForValidation(visible!=FALSE,selection); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetParent(REFIID iid, void** out) override { return parent_->QueryInterface(iid, out); }
    HRESULT STDMETHODCALLTYPE Present(UINT interval, UINT flags) override {
        std::lock_guard lock(mutex_); Render(flags); return real_->Present(interval, flags);
    }
    HRESULT STDMETHODCALLTYPE Present1(UINT interval, UINT flags, const DXGI_PRESENT_PARAMETERS* parameters) override {
        std::lock_guard lock(mutex_);
        // Dirty/scroll updates cannot safely be sharpened in-place as a full frame repeatedly.
        if (!parameters || (!parameters->DirtyRectsCount && !parameters->pScrollRect)) Render(flags);
        return real_->Present1(interval, flags, parameters);
    }
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags) override {
        std::lock_guard lock(mutex_); if (!Reset()) return DXGI_ERROR_WAS_STILL_DRAWING;
        return real_->ResizeBuffers(count, w, h, format, flags);
    }
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT count, UINT w, UINT h, DXGI_FORMAT format, UINT flags, const UINT* nodes, IUnknown* const* queues) override {
        std::lock_guard lock(mutex_); if (!Reset()) return DXGI_ERROR_WAS_STILL_DRAWING;
        HRESULT hr = real_->ResizeBuffers1(count, w, h, format, flags, nodes, queues);
        // Multi-queue swap chains need explicit per-buffer queue ownership support.
        if (SUCCEEDED(hr) && queues) { device_.Reset(); Log(L"ResizeBuffers1 changed present queues; filter bypassed."); }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE color) override {
        std::lock_guard lock(mutex_); HRESULT hr = real_->SetColorSpace1(color); if (SUCCEEDED(hr)) color_ = color; return hr;
    }
#include "SwapForward.h"
};

template<class T> HRESULT WrapSwap(HRESULT result, T** out, IUnknown* device, IUnknown* parent) noexcept {
    if (FAILED(result) || !out || !*out) return result;
    try {
        Ptr<IDXGISwapChain4> real;
        if (FAILED((*out)->QueryInterface(IID_PPV_ARGS(&real)))) return result;
        auto* wrapper = new Swap(real.Get(), device, parent);
        (*out)->Release(); *out = static_cast<T*>(wrapper);
    } catch (...) { Log(L"Swap wrapper unavailable; original swap chain retained."); }
    return result;
}
class Factory final : public IDXGIFactory7 {
    std::atomic<ULONG> references_{1};
    Ptr<IDXGIFactory7> real_;
public:
    explicit Factory(IDXGIFactory7* real) : real_(real) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER; *out = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDXGIObject) || iid == __uuidof(IDXGIFactory) || iid == __uuidof(IDXGIFactory1) ||
            iid == __uuidof(IDXGIFactory2) || iid == __uuidof(IDXGIFactory3) || iid == __uuidof(IDXGIFactory4) || iid == __uuidof(IDXGIFactory5) ||
            iid == __uuidof(IDXGIFactory6) || iid == __uuidof(IDXGIFactory7)) { *out = static_cast<IDXGIFactory7*>(this); AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { auto left = --references_; if (!left) delete this; return left; }
    HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* device, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) override {
        auto hr = real_->CreateSwapChain(device, desc, out); return WrapSwap(hr, out, device, this);
    }
    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown* device, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, IDXGIOutput* restrictOutput, IDXGISwapChain1** out) override {
        auto hr = real_->CreateSwapChainForHwnd(device, window, desc, fullscreen, restrictOutput, out); return WrapSwap(hr, out, device, this);
    }
    HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(IUnknown* device, IUnknown* window, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* restrictOutput, IDXGISwapChain1** out) override {
        auto hr = real_->CreateSwapChainForCoreWindow(device, window, desc, restrictOutput, out); return WrapSwap(hr, out, device, this);
    }
    HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(IUnknown* device, const DXGI_SWAP_CHAIN_DESC1* desc, IDXGIOutput* restrictOutput, IDXGISwapChain1** out) override {
        auto hr = real_->CreateSwapChainForComposition(device, desc, restrictOutput, out); return WrapSwap(hr, out, device, this);
    }
#include "FactoryForward.h"
};
HRESULT WrapFactory(HRESULT hr, REFIID iid, void** out) noexcept {
    if (FAILED(hr) || !out || !*out) return hr;
    try {
        auto* original = static_cast<IUnknown*>(*out);
        Ptr<IDXGIFactory7> real;
        if (FAILED(original->QueryInterface(IID_PPV_ARGS(&real)))) return hr;
        auto* wrapper = new Factory(real.Get()); void* wrapped = nullptr;
        HRESULT result = wrapper->QueryInterface(iid, &wrapped); wrapper->Release();
        if (SUCCEEDED(result)) { original->Release(); *out = wrapped; RuntimeState(); }
    } catch (...) { Log(L"Factory wrapper unavailable; original factory retained."); }
    return hr;
}
}
extern "C" HRESULT WINAPI NativeCreateFactory(REFIID iid, void** out) {
    using Fn = HRESULT(WINAPI*)(REFIID, void**); auto fn = reinterpret_cast<Fn>(NativeResolve("CreateDXGIFactory"));
    HRESULT hr = fn(iid, out); return native::WrapFactory(hr, iid, out);
}
extern "C" HRESULT WINAPI NativeCreateFactory1(REFIID iid, void** out) {
    using Fn = HRESULT(WINAPI*)(REFIID, void**); auto fn = reinterpret_cast<Fn>(NativeResolve("CreateDXGIFactory1"));
    HRESULT hr = fn(iid, out); return native::WrapFactory(hr, iid, out);
}
extern "C" HRESULT WINAPI NativeCreateFactory2(UINT flags, REFIID iid, void** out) {
    using Fn = HRESULT(WINAPI*)(UINT, REFIID, void**); auto fn = reinterpret_cast<Fn>(NativeResolve("CreateDXGIFactory2"));
    HRESULT hr = fn(flags, iid, out); return native::WrapFactory(hr, iid, out);
}
extern "C" UINT WINAPI DLSSClearNativeVersion() { return 2; }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) { if (reason == DLL_PROCESS_ATTACH) module = instance; return TRUE; }
