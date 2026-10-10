#pragma once
#include "Image.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <array>
#include <string>

struct Settings {
    unsigned width = 0, height = 0;
    float strength = .65f, detail = .20f;
    float noise = .012f, antiRinging = 1.f;
    unsigned mode = 2;
    float split = .5f;
    float viewWidth = 1, viewHeight = 1, scale = 1;
    unsigned radius = 1;
    float panX = 0, panY = 0, padding2 = 0, padding3 = 0;
};
static_assert(sizeof(Settings) == 64);

struct FilterPreset { const wchar_t* name; float strength, detail, noise; unsigned radius = 1; };
inline constexpr std::array<FilterPreset, 5> kFilterPresets{{
    {L"Yumuşak", .35f, .10f, .020f},
    {L"Dengeli", .65f, .20f, .012f},
    {L"Belirgin", 1.15f, .35f, .008f},
    {L"Foto: hafif bulanık", .90f, .25f, .016f, 2},
    {L"Foto: geniş netlik", 1.20f, .35f, .020f, 3}
}};

class Renderer {
public:
    void Initialize(HWND viewport, const std::filesystem::path& shaderPath, bool forceWarp = false);
    void ReloadShaders();
    void Upload(const Image& image);
    void Resize(unsigned width, unsigned height);
    void Process(const Settings& settings);
    bool Present(Settings settings, bool nativeSize, bool forceRedraw = false);
    void InvalidatePresentation() { presentationValid_ = false; }
    Image Readback();
    double Measure(const Settings& settings, unsigned repeats = 32);
    unsigned DebugErrors() const;
    unsigned Width() const { return imageWidth_; }
    unsigned Height() const { return imageHeight_; }
    double GpuMs() const { return gpuMs_; }
    std::uint64_t FilterDrawCount() const { return filterDrawCount_; }
    std::uint64_t DisplayDrawCount() const { return displayDrawCount_; }
    bool Software() const { return software_; }
    bool DebugLayer() const { return debugLayer_; }
    const std::wstring& Adapter() const { return adapter_; }
private:
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    struct Timer {
        Ptr<ID3D11Query> disjoint, start, end;
        bool pending = false;
        std::uint64_t revision = 0;
    };
    void DrawPass(const Settings& settings);
    void BindSettings(const Settings& settings);
    void PollTimers();
    void CreateTimer(Timer& timer);
    void MakeBackbuffer();
    void InvalidateOutput();
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext> context_;
    Ptr<IDXGISwapChain> swap_;
    Ptr<ID3D11RenderTargetView> backbuffer_, outputRtv_;
    Ptr<ID3D11Texture2D> source_, output_;
    Ptr<ID3D11ShaderResourceView> sourceSrv_, outputSrv_;
    Ptr<ID3D11VertexShader> vertex_;
    Ptr<ID3D11PixelShader> sharpen_, display_;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11SamplerState> sampler_;
    Ptr<ID3D11RasterizerState> rasterizer_;
    std::array<Timer, 8> timers_;
    size_t nextTimer_ = 0;
    double gpuMs_ = -1;
    Settings filteredSettings_{}, presentedSettings_{};
    bool outputValid_ = false, presentationValid_ = false;
    std::uint64_t timingRevision_ = 0, filterDrawCount_ = 0, displayDrawCount_ = 0;
    unsigned imageWidth_ = 0, imageHeight_ = 0, viewWidth_ = 1, viewHeight_ = 1;
    bool software_ = false, debugLayer_ = false;
    std::wstring adapter_;
    std::filesystem::path shaderPath_;
};
