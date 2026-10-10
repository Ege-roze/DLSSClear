#include "Renderer.h"
#include <d3dcompiler.h>
#include <d3d11sdklayers.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) throw std::runtime_error(std::string(what) + " (HRESULT " + std::to_string(static_cast<unsigned>(hr)) + ")");
}
bool SameFilter(const Settings& a, const Settings& b) {
    return a.strength == b.strength && a.detail == b.detail && a.noise == b.noise && a.antiRinging == b.antiRinging && a.radius == b.radius;
}
bool SameView(const Settings& a, const Settings& b) {
    return a.mode == b.mode && a.split == b.split && a.scale == b.scale &&
        a.viewWidth == b.viewWidth && a.viewHeight == b.viewHeight && a.panX == b.panX && a.panY == b.panY;
}
ComPtr<ID3DBlob> Compile(const std::filesystem::path& path, const char* entry, const char* target) {
    ComPtr<ID3DBlob> result, errors;
    HRESULT hr = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &result, &errors);
    if (FAILED(hr)) {
        std::string text = "Shader derlenemedi: " + path.string() + "\n";
        if (errors) text.append(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        throw std::runtime_error(text);
    }
    return result;
}
}
void Renderer::Initialize(HWND viewport, const std::filesystem::path& shaderPath, bool forceWarp) {
    shaderPath_ = shaderPath;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    auto create = [&](D3D_DRIVER_TYPE driver) {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        HRESULT hr = D3D11CreateDevice(nullptr, driver, nullptr, flags, levels, 1, D3D11_SDK_VERSION,
            &device_, nullptr, &context_);
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            hr = D3D11CreateDevice(nullptr, driver, nullptr, flags, levels, 1, D3D11_SDK_VERSION,
                &device_, nullptr, &context_);
        }
        debugLayer_ = SUCCEEDED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG);
        return hr;
    };
    HRESULT hr = forceWarp ? E_FAIL : create(D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(hr)) { Check(create(D3D_DRIVER_TYPE_WARP), "DX11 device"); software_ = true; }
    ComPtr<IDXGIDevice> dxgiDevice;
    Check(device_.As(&dxgiDevice), "DXGI device");
    ComPtr<IDXGIAdapter> adapter;
    Check(dxgiDevice->GetAdapter(&adapter), "DXGI adapter");
    DXGI_ADAPTER_DESC desc{};
    Check(adapter->GetDesc(&desc), "Adapter description");
    adapter_ = desc.Description;
    if (viewport) {
        ComPtr<IDXGIFactory> factory;
        Check(adapter->GetParent(IID_PPV_ARGS(&factory)), "DXGI factory");
        RECT rect{}; GetClientRect(viewport, &rect);
        viewWidth_ = std::max(1L, rect.right); viewHeight_ = std::max(1L, rect.bottom);
        DXGI_SWAP_CHAIN_DESC swapDesc{};
        swapDesc.BufferDesc.Width = viewWidth_; swapDesc.BufferDesc.Height = viewHeight_;
        swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDesc.SampleDesc.Count = 1; swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = 2; swapDesc.OutputWindow = viewport; swapDesc.Windowed = TRUE;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        Check(factory->CreateSwapChain(device_.Get(), &swapDesc, &swap_), "Swap chain");
        Check(factory->MakeWindowAssociation(viewport, DXGI_MWA_NO_ALT_ENTER), "Window association");
        MakeBackbuffer();
    }
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth = sizeof(Settings); cb.Usage = D3D11_USAGE_DEFAULT;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Check(device_->CreateBuffer(&cb, nullptr, &constants_), "Constants");
    D3D11_SAMPLER_DESC sampler{}; sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    Check(device_->CreateSamplerState(&sampler, &sampler_), "Sampler");
    D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID;
    raster.CullMode = D3D11_CULL_NONE; raster.DepthClipEnable = TRUE;
    Check(device_->CreateRasterizerState(&raster, &rasterizer_), "Rasterizer");
    for (auto& timer : timers_) CreateTimer(timer);
    ReloadShaders();
}
void Renderer::ReloadShaders() {
    auto vs = Compile(shaderPath_, "FullscreenVS", "vs_5_0");
    auto ps = Compile(shaderPath_, "SharpenPS", "ps_5_0");
    auto ds = Compile(shaderPath_, "DisplayPS", "ps_5_0");
    Ptr<ID3D11VertexShader> vertex;
    Ptr<ID3D11PixelShader> sharpen, display;
    Check(device_->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex), "Vertex shader");
    Check(device_->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &sharpen), "Sharpen shader");
    Check(device_->CreatePixelShader(ds->GetBufferPointer(), ds->GetBufferSize(), nullptr, &display), "Display shader");
    // Keep the previous working shaders if any compilation or creation fails.
    vertex_ = vertex; sharpen_ = sharpen; display_ = display;
    InvalidateOutput();
}
void Renderer::InvalidateOutput() {
    outputValid_ = false; presentationValid_ = false; gpuMs_ = -1;
    ++timingRevision_; // Ignore measurements belonging to a previous image/settings/shader.
}
void Renderer::Upload(const Image& image) {
    if (!image.width || !image.height || image.pixels.size() != static_cast<size_t>(image.width) * image.height * 4)
        throw std::runtime_error("Gecersiz RGBA goruntu.");
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = image.width; desc.Height = image.height; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{image.pixels.data(), image.width * 4, 0};
    Ptr<ID3D11Texture2D> source, output;
    Ptr<ID3D11ShaderResourceView> sourceView, outputView;
    Ptr<ID3D11RenderTargetView> outputRtv;
    Check(device_->CreateTexture2D(&desc, &data, &source), "Input texture");
    Check(device_->CreateShaderResourceView(source.Get(), nullptr, &sourceView), "Input view");
    desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    Check(device_->CreateTexture2D(&desc, nullptr, &output), "Output texture");
    Check(device_->CreateShaderResourceView(output.Get(), nullptr, &outputView), "Output view");
    Check(device_->CreateRenderTargetView(output.Get(), nullptr, &outputRtv), "Output target");
    ID3D11ShaderResourceView* empty[] = {nullptr, nullptr};
    context_->PSSetShaderResources(0, 2, empty); context_->OMSetRenderTargets(0, nullptr, nullptr);
    source_ = source; output_ = output; sourceSrv_ = sourceView; outputSrv_ = outputView; outputRtv_ = outputRtv;
    imageWidth_ = image.width; imageHeight_ = image.height;
    InvalidateOutput();
}
void Renderer::MakeBackbuffer() {
    Ptr<ID3D11Texture2D> texture;
    Check(swap_->GetBuffer(0, IID_PPV_ARGS(&texture)), "Backbuffer");
    Check(device_->CreateRenderTargetView(texture.Get(), nullptr, &backbuffer_), "Backbuffer target");
}
void Renderer::Resize(unsigned width, unsigned height) {
    if (!swap_ || !width || !height || (width == viewWidth_ && height == viewHeight_)) return;
    context_->OMSetRenderTargets(0, nullptr, nullptr); backbuffer_.Reset();
    Check(swap_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "Resize");
    viewWidth_ = width; viewHeight_ = height; MakeBackbuffer();
    InvalidatePresentation();
}
void Renderer::BindSettings(const Settings& settings) {
    Settings data = settings; data.width = imageWidth_; data.height = imageHeight_;
    context_->UpdateSubresource(constants_.Get(), 0, nullptr, &data, 0, 0);
    context_->PSSetConstantBuffers(0, 1, constants_.GetAddressOf());
    context_->IASetInputLayout(nullptr); context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertex_.Get(), nullptr, 0); context_->RSSetState(rasterizer_.Get());
}
void Renderer::DrawPass(const Settings& settings) {
    ID3D11ShaderResourceView* empty[] = {nullptr, nullptr};
    context_->PSSetShaderResources(0, 2, empty);
    context_->OMSetRenderTargets(1, outputRtv_.GetAddressOf(), nullptr);
    D3D11_VIEWPORT viewport{0, 0, static_cast<float>(imageWidth_), static_cast<float>(imageHeight_), 0, 1};
    context_->RSSetViewports(1, &viewport);
    BindSettings(settings);
    context_->PSSetShader(sharpen_.Get(), nullptr, 0);
    context_->PSSetShaderResources(0, 1, sourceSrv_.GetAddressOf());
    context_->Draw(3, 0);
    ++filterDrawCount_;
    context_->OMSetRenderTargets(0, nullptr, nullptr);
}
void Renderer::CreateTimer(Timer& timer) {
    D3D11_QUERY_DESC desc{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
    Check(device_->CreateQuery(&desc, &timer.disjoint), "Disjoint query");
    desc.Query = D3D11_QUERY_TIMESTAMP;
    Check(device_->CreateQuery(&desc, &timer.start), "Start query");
    Check(device_->CreateQuery(&desc, &timer.end), "End query");
}
void Renderer::PollTimers() {
    for (auto& t : timers_) {
        if (!t.pending) continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{}; UINT64 start = 0, end = 0;
        HRESULT ready = context_->GetData(t.disjoint.Get(), &d, sizeof(d), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        Check(ready, "GPU timer");
        if (ready != S_OK) continue;
        HRESULT a = context_->GetData(t.start.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        HRESULT b = context_->GetData(t.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        Check(a, "GPU start"); Check(b, "GPU end");
        if (a != S_OK || b != S_OK) continue;
        t.pending = false;
        if (t.revision == timingRevision_ && !d.Disjoint && d.Frequency && end >= start) {
            double ms = static_cast<double>(end - start) * 1000.0 / static_cast<double>(d.Frequency);
            gpuMs_ = ms; // Last actual pass, never pretend a cached frame is a new timing sample.
        }
    }
}
void Renderer::Process(const Settings& settings) {
    if (!source_) return;
    PollTimers();
    if (outputValid_ && SameFilter(settings, filteredSettings_)) return;
    InvalidateOutput();
    Timer& t = timers_[nextTimer_];
    bool measure = !t.pending;
    if (measure) { context_->Begin(t.disjoint.Get()); context_->End(t.start.Get()); }
    DrawPass(settings);
    if (measure) {
        context_->End(t.end.Get()); context_->End(t.disjoint.Get()); t.pending = true; t.revision = timingRevision_;
        nextTimer_ = (nextTimer_ + 1) % timers_.size();
    }
    filteredSettings_ = settings; outputValid_ = true;
}
bool Renderer::Present(Settings settings, bool nativeSize, bool forceRedraw) {
    if (!swap_ || !source_) return false;
    Process(settings);
    settings.viewWidth = static_cast<float>(viewWidth_); settings.viewHeight = static_cast<float>(viewHeight_);
    settings.scale = nativeSize ? 1.f : std::min(settings.viewWidth / imageWidth_, settings.viewHeight / imageHeight_);
    if (!nativeSize) settings.panX = settings.panY = 0;
    else {
        // Round the image origin to a physical pixel for a true 1:1 view.
        settings.panX = std::round((settings.viewWidth - imageWidth_) * .5f + settings.panX) - (settings.viewWidth - imageWidth_) * .5f;
        settings.panY = std::round((settings.viewHeight - imageHeight_) * .5f + settings.panY) - (settings.viewHeight - imageHeight_) * .5f;
    }
    if (!forceRedraw && presentationValid_ && SameView(settings, presentedSettings_)) return false;
    BindSettings(settings);
    context_->OMSetRenderTargets(1, backbuffer_.GetAddressOf(), nullptr);
    D3D11_VIEWPORT viewport{0, 0, settings.viewWidth, settings.viewHeight, 0, 1}; context_->RSSetViewports(1, &viewport);
    context_->PSSetShader(display_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srvs[] = {sourceSrv_.Get(), outputSrv_.Get()};
    context_->PSSetShaderResources(0, 2, srvs); context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    context_->Draw(3, 0);
    ++displayDrawCount_;
    HRESULT hr = swap_->Present(1, 0);
    Check(hr, "Present");
    presentedSettings_ = settings;
    // Retry an occluded window when the next UI event/timer wakes the app.
    presentationValid_ = hr != DXGI_STATUS_OCCLUDED;
    return presentationValid_;
}
Image Renderer::Readback() {
    if (!output_) throw std::runtime_error("Once bir goruntu yukleyin.");
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    D3D11_TEXTURE2D_DESC desc{}; output_->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Ptr<ID3D11Texture2D> staging;
    Check(device_->CreateTexture2D(&desc, nullptr, &staging), "Readback texture");
    context_->CopyResource(staging.Get(), output_.Get());
    Image result{imageWidth_, imageHeight_, std::vector<std::uint8_t>(static_cast<size_t>(imageWidth_) * imageHeight_ * 4)};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Readback map");
    for (unsigned y = 0; y < imageHeight_; ++y)
        std::memcpy(result.pixels.data() + static_cast<size_t>(y) * imageWidth_ * 4,
            static_cast<const BYTE*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch, imageWidth_ * 4);
    context_->Unmap(staging.Get(), 0);
    return result;
}
double Renderer::Measure(const Settings& settings, unsigned repeats) {
    if (!repeats || !source_) throw std::runtime_error("Invalid benchmark input.");
    // Benchmark must execute real work, bypass the cache, and leave no stale cached output.
    InvalidateOutput();
    Timer t; CreateTimer(t);
    for (unsigned i = 0; i < 12; ++i) DrawPass(settings);
    context_->Begin(t.disjoint.Get()); context_->End(t.start.Get());
    for (unsigned i = 0; i < repeats; ++i) DrawPass(settings);
    context_->End(t.end.Get()); context_->End(t.disjoint.Get()); context_->Flush();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{}; UINT64 start = 0, end = 0;
    for (;;) {
        HRESULT a = context_->GetData(t.disjoint.Get(), &d, sizeof(d), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        HRESULT b = context_->GetData(t.start.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        HRESULT c = context_->GetData(t.end.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        Check(a, "Benchmark disjoint"); Check(b, "Benchmark start"); Check(c, "Benchmark end");
        if (a == S_OK && b == S_OK && c == S_OK) break;
        if (std::chrono::steady_clock::now() > deadline) throw std::runtime_error("GPU benchmark timeout.");
        Sleep(1);
    }
    if (d.Disjoint || !d.Frequency || end < start) throw std::runtime_error("GPU clock changed; retry benchmark.");
    return static_cast<double>(end - start) * 1000.0 / static_cast<double>(d.Frequency) / repeats;
}
unsigned Renderer::DebugErrors() const {
    ComPtr<ID3D11InfoQueue> queue;
    if (FAILED(device_.As(&queue))) return 0;
    unsigned errors = 0;
    for (UINT64 i = 0; i < queue->GetNumStoredMessages(); ++i) {
        SIZE_T size = 0; Check(queue->GetMessage(i, nullptr, &size), "Debug message size");
        std::vector<BYTE> storage(size);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
        Check(queue->GetMessage(i, message, &size), "Debug message");
        if (message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING) ++errors;
    }
    return errors;
}
