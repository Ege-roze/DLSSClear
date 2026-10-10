#pragma once
#include "Protocol.h"
#include "Menu.h"
#include <dxgi1_6.h>
#include <memory>
#include <wrl/client.h>

namespace native {
template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
inline bool Sdr(DXGI_FORMAT f) { return f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_B8G8R8A8_UNORM; }
void Check(HRESULT hr);
void Log(const wchar_t* message);
struct Filter {
    virtual ~Filter() = default;
    virtual State Render(IDXGISwapChain4* swap, const Options& options, UINT& width, UINT& height, const MenuImage* menu) = 0;
    virtual bool Drain() = 0;
};
std::unique_ptr<Filter> Create11(IUnknown* device);
std::unique_ptr<Filter> Create12(IUnknown* queue);
}
