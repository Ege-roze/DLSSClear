#include "Filter.h"
#include <d3d11.h>
#include "NativeVS.h"
#include "NativePS.h"
#include "MenuPS.h"

namespace native {
class Filter11 final : public Filter {
    Ptr<ID3D11Device> device_;
    Ptr<ID3D11DeviceContext> immediate_, deferred_;
    Ptr<ID3D11VertexShader> vs_;
    Ptr<ID3D11PixelShader> ps_;
    Ptr<ID3D11PixelShader> menuPs_;
    Ptr<ID3D11BlendState> menuBlend_;
    Ptr<ID3D11Texture2D> menuTexture_;
    Ptr<ID3D11ShaderResourceView> menuView_;
    UINT menuWidth_=0,menuHeight_=0;
    UINT64 menuRevision_=0;
    Ptr<ID3D11Buffer> constants_;
    Ptr<ID3D11RasterizerState> raster_;
    Ptr<ID3D11Texture2D> source_;
    Ptr<ID3D11ShaderResourceView> srv_;
    UINT width_ = 0, height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
public:
    explicit Filter11(ID3D11Device* device) : device_(device) {
        device_->GetImmediateContext(&immediate_);
        Check(device_->CreateDeferredContext(0, &deferred_));
        Check(device_->CreateVertexShader(NativeVS, sizeof(NativeVS), nullptr, &vs_));
        Check(device_->CreatePixelShader(NativePS, sizeof(NativePS), nullptr, &ps_));
        Check(device_->CreatePixelShader(MenuPS,sizeof(MenuPS),nullptr,&menuPs_));
        D3D11_BLEND_DESC blend{}; auto& target=blend.RenderTarget[0];
        target.BlendEnable=TRUE; target.SrcBlend=D3D11_BLEND_SRC_ALPHA; target.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;
        target.BlendOp=target.BlendOpAlpha=D3D11_BLEND_OP_ADD; target.SrcBlendAlpha=D3D11_BLEND_ZERO; target.DestBlendAlpha=D3D11_BLEND_ONE;
        target.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        Check(device_->CreateBlendState(&blend,&menuBlend_));
        D3D11_BUFFER_DESC desc{}; desc.ByteWidth = sizeof(Constants); desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Check(device_->CreateBuffer(&desc, nullptr, &constants_));
        D3D11_RASTERIZER_DESC raster{}; raster.FillMode = D3D11_FILL_SOLID; raster.CullMode = D3D11_CULL_NONE;
        raster.DepthClipEnable = TRUE; raster.ScissorEnable = TRUE;
        Check(device_->CreateRasterizerState(&raster, &raster_));
    }
    State Render(IDXGISwapChain4* swap, const Options& options, UINT& width, UINT& height, const MenuImage* menu) override {
        Ptr<ID3D11Texture2D> back;
        Check(swap->GetBuffer(0, IID_PPV_ARGS(&back)));
        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc); width = desc.Width; height = desc.Height;
        if (!Sdr(desc.Format) || desc.SampleDesc.Count != 1 || desc.ArraySize != 1) return Unsupported;
        if (options.enabled && (!source_ || width != width_ || height != height_ || desc.Format != format_)) {
            source_.Reset(); srv_.Reset();
            desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; desc.CPUAccessFlags = 0; desc.MiscFlags = 0;
            Check(device_->CreateTexture2D(&desc, nullptr, &source_));
            Check(device_->CreateShaderResourceView(source_.Get(), nullptr, &srv_));
            width_ = width; height_ = height; format_ = desc.Format;
        }
        Ptr<ID3D11RenderTargetView> rtv;
        Check(device_->CreateRenderTargetView(back.Get(), nullptr, &rtv));
        deferred_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        deferred_->VSSetShader(vs_.Get(), nullptr, 0);
        deferred_->PSSetConstantBuffers(0, 1, constants_.GetAddressOf());
        deferred_->RSSetState(raster_.Get());
        deferred_->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
        if (options.enabled) {
        const auto data = Data(width, height, options);
        deferred_->CopyResource(source_.Get(), back.Get());
        deferred_->UpdateSubresource(constants_.Get(), 0, nullptr, &data, 0, 0);
        deferred_->PSSetShader(ps_.Get(), nullptr, 0);
        deferred_->PSSetShaderResources(0, 1, srv_.GetAddressOf());
        D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        D3D11_RECT rect{options.split ? static_cast<LONG>(width / 2) : 0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        deferred_->RSSetViewports(1, &viewport); deferred_->RSSetScissorRects(1, &rect);
        deferred_->Draw(3, 0);
        }
        if (menu) {
            if(!menuTexture_ || menuWidth_!=menu->width || menuHeight_!=menu->height) {
                menuView_.Reset(); menuTexture_.Reset(); menuRevision_=0;
                D3D11_TEXTURE2D_DESC texture{}; texture.Width=menu->width; texture.Height=menu->height;
                texture.MipLevels=texture.ArraySize=texture.SampleDesc.Count=1; texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
                texture.Usage=D3D11_USAGE_DEFAULT; texture.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                Check(device_->CreateTexture2D(&texture,nullptr,&menuTexture_));
                Check(device_->CreateShaderResourceView(menuTexture_.Get(),nullptr,&menuView_));
                menuWidth_=menu->width; menuHeight_=menu->height;
            }
            if(menuRevision_!=menu->revision) {
                deferred_->UpdateSubresource(menuTexture_.Get(),0,nullptr,menu->rgba.data(),menu->width*4,0);
                menuRevision_=menu->revision;
            }
            const float origin[16]{static_cast<float>(menu->x),static_cast<float>(menu->y)};
            deferred_->UpdateSubresource(constants_.Get(),0,nullptr,origin,0,0);
            deferred_->PSSetShader(menuPs_.Get(),nullptr,0);
            deferred_->PSSetShaderResources(0,1,menuView_.GetAddressOf());
            deferred_->OMSetBlendState(menuBlend_.Get(),nullptr,UINT_MAX);
            D3D11_VIEWPORT viewport{static_cast<float>(menu->x),static_cast<float>(menu->y),static_cast<float>(menu->width),static_cast<float>(menu->height),0,1};
            D3D11_RECT rect{static_cast<LONG>(menu->x),static_cast<LONG>(menu->y),static_cast<LONG>(menu->x+menu->width),static_cast<LONG>(menu->y+menu->height)};
            deferred_->RSSetViewports(1,&viewport); deferred_->RSSetScissorRects(1,&rect); deferred_->Draw(3,0);
        }
        Ptr<ID3D11CommandList> commands;
        Check(deferred_->FinishCommandList(FALSE, &commands));
        // The D3D runtime restores ALL of the game's immediate context state.
        immediate_->ExecuteCommandList(commands.Get(), TRUE);
        return options.enabled?Active11:Disabled;
    }
    bool Drain() override { return true; } // No swap-chain buffer references survive Render.
};
std::unique_ptr<Filter> Create11(IUnknown* object) {
    Ptr<ID3D11Device> device;
    if (!object || FAILED(object->QueryInterface(IID_PPV_ARGS(&device)))) return {};
    return std::make_unique<Filter11>(device.Get());
}
}
