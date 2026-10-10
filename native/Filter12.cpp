#include "Filter.h"
#include <d3d12.h>
#include <vector>
#include "NativeVS.h"
#include "NativePS.h"
#include "MenuPS.h"

namespace native {
namespace {
D3D12_RESOURCE_BARRIER Barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after}; return b;
}
class Filter12 final : public Filter {
    struct Frame {
        Ptr<ID3D12CommandAllocator> allocator;
        Ptr<ID3D12Resource> back, source, menu, upload;
        UINT64 fence=0, menuRevision=0;
        UINT menuWidth=0,menuHeight=0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    };
    Ptr<ID3D12Device> device_;
    Ptr<ID3D12CommandQueue> queue_;
    Ptr<ID3D12RootSignature> root_;
    Ptr<ID3D12PipelineState> pipeline_;
    Ptr<ID3D12PipelineState> menuPipeline_;
    Ptr<ID3D12DescriptorHeap> srv_, rtv_;
    Ptr<ID3D12GraphicsCommandList> commands_;
    Ptr<ID3D12Fence> fence_;
    HANDLE event_ = nullptr;
    UINT64 nextFence_ = 0;
    std::vector<Frame> frames_;
    UINT width_ = 0, height_ = 0, srvStep_ = 0, rtvStep_ = 0;
    void Initialize(IDXGISwapChain4* swap) {
        DXGI_SWAP_CHAIN_DESC1 desc{}; Check(swap->GetDesc1(&desc));
        width_ = desc.Width; height_ = desc.Height;
        if (!desc.BufferCount || desc.BufferCount > 16) throw HRESULT(E_INVALIDARG);
        frames_.resize(desc.BufferCount);
        D3D12_DESCRIPTOR_HEAP_DESC heap{}; heap.NumDescriptors = desc.BufferCount*2; heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; Check(device_->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&srv_)));
        heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap.NumDescriptors=desc.BufferCount; heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        Check(device_->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtv_)));
        srvStep_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        rtvStep_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_HEAP_PROPERTIES properties{}; properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        for (UINT i = 0; i < desc.BufferCount; ++i) {
            auto& frame = frames_[i]; Check(swap->GetBuffer(i, IID_PPV_ARGS(&frame.back)));
            auto resource = frame.back->GetDesc(); resource.Flags = D3D12_RESOURCE_FLAG_NONE;
            Check(device_->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &resource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                nullptr, IID_PPV_ARGS(&frame.source)));
            Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)));
            auto cpu = srv_->GetCPUDescriptorHandleForHeapStart(); cpu.ptr += static_cast<SIZE_T>(i) * srvStep_;
            D3D12_SHADER_RESOURCE_VIEW_DESC view{}; view.Format = desc.Format; view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; view.Texture2D.MipLevels = 1;
            device_->CreateShaderResourceView(frame.source.Get(), &view, cpu);
            cpu = rtv_->GetCPUDescriptorHandleForHeapStart(); cpu.ptr += static_cast<SIZE_T>(i) * rtvStep_;
            device_->CreateRenderTargetView(frame.back.Get(), nullptr, cpu);
        }
        Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].allocator.Get(), nullptr, IID_PPV_ARGS(&commands_)));
        Check(commands_->Close());
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{}; pso.pRootSignature = root_.Get();
        pso.VS = {NativeVS, sizeof(NativeVS)}; pso.PS = {NativePS, sizeof(NativePS)};
        pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso.BlendState.RenderTarget[0].SrcBlend = pso.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        pso.BlendState.RenderTarget[0].DestBlend = pso.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        pso.BlendState.RenderTarget[0].BlendOp = pso.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        pso.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        pso.SampleMask = UINT_MAX; pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pso.RasterizerState.DepthClipEnable = TRUE;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1; pso.RTVFormats[0] = desc.Format; pso.SampleDesc.Count = 1;
        Check(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline_)));
        pso.PS={MenuPS,sizeof(MenuPS)};
        auto& blend=pso.BlendState.RenderTarget[0]; blend.BlendEnable=TRUE;
        blend.SrcBlend=D3D12_BLEND_SRC_ALPHA; blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
        blend.SrcBlendAlpha=D3D12_BLEND_ZERO; blend.DestBlendAlpha=D3D12_BLEND_ONE;
        Check(device_->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&menuPipeline_)));
    }
    void UploadMenu(Frame& frame,UINT index,const MenuImage& menu) {
        // Called only after this frame's fence completes; descriptors/resources cannot be in flight.
        if(!frame.menu || frame.menuWidth!=menu.width || frame.menuHeight!=menu.height) {
            frame.menu.Reset(); frame.upload.Reset(); frame.menuRevision=0;
            D3D12_RESOURCE_DESC texture{}; texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture.Width=menu.width; texture.Height=menu.height; texture.DepthOrArraySize=texture.MipLevels=1;
            texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM; texture.SampleDesc.Count=1;
            D3D12_HEAP_PROPERTIES heap{}; heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            Check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&texture,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&frame.menu)));
            UINT64 total=0; device_->GetCopyableFootprints(&texture,0,1,0,&frame.footprint,nullptr,nullptr,&total);
            heap.Type=D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width=total;
            buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1; buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            Check(device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&frame.upload)));
            auto cpu=srv_->GetCPUDescriptorHandleForHeapStart(); cpu.ptr+=(static_cast<SIZE_T>(frames_.size())+index)*srvStep_;
            D3D12_SHADER_RESOURCE_VIEW_DESC view{}; view.Format=texture.Format; view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
            view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; view.Texture2D.MipLevels=1;
            device_->CreateShaderResourceView(frame.menu.Get(),&view,cpu);
            frame.menuWidth=menu.width; frame.menuHeight=menu.height;
        }
        if(frame.menuRevision==menu.revision) return;
        BYTE* pixels=nullptr; D3D12_RANGE empty{0,0}; Check(frame.upload->Map(0,&empty,reinterpret_cast<void**>(&pixels)));
        for(UINT y=0;y<menu.height;++y) memcpy(pixels+frame.footprint.Offset+static_cast<size_t>(y)*frame.footprint.Footprint.RowPitch,
            menu.rgba.data()+static_cast<size_t>(y)*menu.width,menu.width*4);
        frame.upload->Unmap(0,nullptr);
        auto before=Barrier(frame.menu.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        commands_->ResourceBarrier(1,&before);
        D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource=frame.upload.Get(); source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; source.PlacedFootprint=frame.footprint;
        D3D12_TEXTURE_COPY_LOCATION target{}; target.pResource=frame.menu.Get(); target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        commands_->CopyTextureRegion(&target,0,0,0,&source,nullptr);
        auto after=Barrier(frame.menu.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commands_->ResourceBarrier(1,&after); frame.menuRevision=menu.revision;
    }
public:
    explicit Filter12(ID3D12CommandQueue* queue) : queue_(queue) {
        Check(queue_->GetDevice(IID_PPV_ARGS(&device_)));
        Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr); if (!event_) throw HRESULT(E_FAIL);
        D3D12_DESCRIPTOR_RANGE range{}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants = {0, 0, 16}; parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[1].DescriptorTable = {1, &range}; parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC desc{}; desc.NumParameters = 2; desc.pParameters = parameters;
        Ptr<ID3DBlob> blob, error;
        Check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
        Check(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root_)));
    }
    ~Filter12() override { if (event_) CloseHandle(event_); }
    bool Drain() override {
        if (!nextFence_ || fence_->GetCompletedValue() >= nextFence_) return true;
        if (FAILED(fence_->SetEventOnCompletion(nextFence_, event_))) return false;
        return WaitForSingleObject(event_, 5000) == WAIT_OBJECT_0;
    }
    State Render(IDXGISwapChain4* swap, const Options& options, UINT& width, UINT& height, const MenuImage* menu) override {
        DXGI_SWAP_CHAIN_DESC1 desc{}; Check(swap->GetDesc1(&desc)); width = desc.Width; height = desc.Height;
        if (!Sdr(desc.Format) || desc.SampleDesc.Count != 1 || (desc.Flags & DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED)) return Unsupported;
        if (frames_.empty()) Initialize(swap);
        const UINT index = swap->GetCurrentBackBufferIndex();
        if (index >= frames_.size()) throw HRESULT(E_FAIL);
        auto& frame = frames_[index];
        if (frame.fence && fence_->GetCompletedValue() < frame.fence) return GpuBusy; // Never stall Present on a busy allocator.
        Check(frame.allocator->Reset()); Check(commands_->Reset(frame.allocator.Get(), pipeline_.Get()));
        if(menu) UploadMenu(frame,index,*menu);
        if(options.enabled) {
        D3D12_RESOURCE_BARRIER before[] = {Barrier(frame.back.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_SOURCE),
            Barrier(frame.source.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST)};
        commands_->ResourceBarrier(2, before); commands_->CopyResource(frame.source.Get(), frame.back.Get());
        D3D12_RESOURCE_BARRIER after[] = {Barrier(frame.back.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
            Barrier(frame.source.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
        commands_->ResourceBarrier(2, after);
        } else {
            auto before=Barrier(frame.back.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
            commands_->ResourceBarrier(1,&before);
        }
        commands_->SetGraphicsRootSignature(root_.Get());
        ID3D12DescriptorHeap* heaps[]{srv_.Get()}; commands_->SetDescriptorHeaps(1, heaps);
        const auto data = Data(width_, height_, options); commands_->SetGraphicsRoot32BitConstants(0, 16, &data, 0);
        auto gpu = srv_->GetGPUDescriptorHandleForHeapStart(); gpu.ptr += static_cast<UINT64>(index) * srvStep_;
        commands_->SetGraphicsRootDescriptorTable(1, gpu);
        auto cpu = rtv_->GetCPUDescriptorHandleForHeapStart(); cpu.ptr += static_cast<SIZE_T>(index) * rtvStep_;
        commands_->OMSetRenderTargets(1, &cpu, FALSE, nullptr);
        D3D12_VIEWPORT viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_), 0, 1};
        D3D12_RECT rect{options.split ? static_cast<LONG>(width_ / 2) : 0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        commands_->RSSetViewports(1, &viewport); commands_->RSSetScissorRects(1, &rect);
        commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if(options.enabled) commands_->DrawInstanced(3, 1, 0, 0);
        if(menu) {
            commands_->SetPipelineState(menuPipeline_.Get());
            const float origin[16]{static_cast<float>(menu->x),static_cast<float>(menu->y)};
            commands_->SetGraphicsRoot32BitConstants(0,16,origin,0);
            auto menuGpu=srv_->GetGPUDescriptorHandleForHeapStart(); menuGpu.ptr+=(static_cast<UINT64>(frames_.size())+index)*srvStep_;
            commands_->SetGraphicsRootDescriptorTable(1,menuGpu);
            D3D12_VIEWPORT view{static_cast<float>(menu->x),static_cast<float>(menu->y),static_cast<float>(menu->width),static_cast<float>(menu->height),0,1};
            D3D12_RECT clip{static_cast<LONG>(menu->x),static_cast<LONG>(menu->y),static_cast<LONG>(menu->x+menu->width),static_cast<LONG>(menu->y+menu->height)};
            commands_->RSSetViewports(1,&view); commands_->RSSetScissorRects(1,&clip); commands_->DrawInstanced(3,1,0,0);
        }
        auto done = Barrier(frame.back.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        commands_->ResourceBarrier(1, &done); Check(commands_->Close());
        ID3D12CommandList* lists[]{commands_.Get()}; queue_->ExecuteCommandLists(1, lists);
        Check(queue_->Signal(fence_.Get(), ++nextFence_)); frame.fence = nextFence_;
        return options.enabled?Active12:Disabled;
    }
};
}
std::unique_ptr<Filter> Create12(IUnknown* object) {
    Ptr<ID3D12CommandQueue> queue;
    if (!object || FAILED(object->QueryInterface(IID_PPV_ARGS(&queue))) || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return {};
    return std::make_unique<Filter12>(queue.Get());
}
}
