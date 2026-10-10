#include "../src/Localization.h"
#include "Protocol.h"
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <array>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
bool requireDebug = true;
void Check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("HRESULT " + std::to_string(static_cast<UINT>(hr))); }
void Require(bool okay, const char* text) { if (!okay) throw std::runtime_error(text); }
std::vector<BYTE> Pixels(UINT w, UINT h, UINT time = 0) {
    std::vector<BYTE> pixels(static_cast<size_t>(w) * h * 4);
    for (UINT y = 0; y < h; ++y) for (UINT x = 0; x < w; ++x) {
        const size_t i = (static_cast<size_t>(y) * w + x) * 4;
        pixels[i] = static_cast<BYTE>(60 + ((x + time) * 5 + y * 3) % 150);
        pixels[i+1] = static_cast<BYTE>(70 + ((x + time) * 7 + y * 2) % 140);
        pixels[i+2] = static_cast<BYTE>(50 + (x * 3 + y * 5) % 170); pixels[i+3] = 255;
    }
    return pixels;
}
struct Host {
    ComPtr<IDXGISwapChain4> swap;
    UINT width=65, height=37;
    virtual ~Host() = default;
    virtual void Upload(const std::vector<BYTE>& input) = 0;
    virtual std::vector<BYTE> Read() = 0;
    virtual void ReleaseBack() = 0;
    virtual void DebugCheck() = 0;
    virtual void StateCheck() {}
    void Process() { ComPtr<native::Diagnostics> diagnostics; Check(swap.As(&diagnostics)); Check(diagnostics->ProcessForValidation()); }
    void Resize(UINT w, UINT h) { ReleaseBack(); Check(swap->ResizeBuffers(2,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,0)); width=w; height=h; }
};
ComPtr<IDXGISwapChain4> MakeSwap(IDXGIFactory7* factory, IUnknown* device, HWND window, UINT w, UINT h, bool dx12 = false) {
    DXGI_SWAP_CHAIN_DESC1 desc{}; desc.Width=w; desc.Height=h; desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1; desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2;
    desc.SwapEffect=dx12?DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL:DXGI_SWAP_EFFECT_SEQUENTIAL;
    ComPtr<IDXGISwapChain1> first; Check(factory->CreateSwapChainForHwnd(device,window,&desc,nullptr,nullptr,&first));
    ComPtr<IDXGISwapChain4> result; Check(first.As(&result));
    ComPtr<IUnknown> a,b; Check(first.As(&a)); Check(result.As(&b)); Require(a==b,"Swap COM identity changed");
    ComPtr<IUnknown> parent; Check(result->GetParent(IID_PPV_ARGS(&parent))); Check(factory->QueryInterface(IID_PPV_ARGS(&b)));
    Require(parent==b,"Factory parent COM identity changed"); return result;
}
struct Host11 : Host {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> back, staging;
    ComPtr<ID3D11InfoQueue> debug;
    Host11(IDXGIFactory7* factory, HWND window) {
        D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
        HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_DEBUG,&level,1,D3D11_SDK_VERSION,&device,nullptr,&context);
        if(hr==DXGI_ERROR_SDK_COMPONENT_MISSING && !requireDebug) hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,nullptr,&context);
        Check(hr); hr=device.As(&debug); if(requireDebug) Check(hr); swap=MakeSwap(factory,device.Get(),window,width,height);
    }
    void Upload(const std::vector<BYTE>& pixels) override {
        ReleaseBack(); Check(swap->GetBuffer(0,IID_PPV_ARGS(&back)));
        context->UpdateSubresource(back.Get(),0,nullptr,pixels.data(),width*4,0);
        D3D11_TEXTURE2D_DESC desc{}; back->GetDesc(&desc); desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=desc.MiscFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        Check(device->CreateTexture2D(&desc,nullptr,&staging));
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        D3D11_VIEWPORT sentinel{7,9,17,19,.1f,.9f}; context->RSSetViewports(1,&sentinel);
    }
    void StateCheck() override {
        D3D11_PRIMITIVE_TOPOLOGY topology{}; context->IAGetPrimitiveTopology(&topology);
        Require(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST,"DX11 topology leaked from native filter");
        D3D11_VIEWPORT viewport{}; UINT count=1; context->RSGetViewports(&count,&viewport);
        Require(count==1 && viewport.TopLeftX==7 && viewport.Width==17,"DX11 viewport leaked from native filter");
    }
    std::vector<BYTE> Read() override {
        context->CopyResource(staging.Get(),back.Get()); D3D11_MAPPED_SUBRESOURCE map{}; Check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
        std::vector<BYTE> result(static_cast<size_t>(width)*height*4);
        for(UINT y=0;y<height;++y) memcpy(result.data()+static_cast<size_t>(y)*width*4,static_cast<BYTE*>(map.pData)+static_cast<size_t>(y)*map.RowPitch,width*4);
        context->Unmap(staging.Get(),0); return result;
    }
    void ReleaseBack() override { back.Reset(); staging.Reset(); }
    void DebugCheck() override {
        if(!debug) return;
        for(UINT64 i=0;i<debug->GetNumStoredMessages();++i) {
            SIZE_T size=0; Check(debug->GetMessage(i,nullptr,&size)); std::vector<BYTE> data(size);
            auto* msg=reinterpret_cast<D3D11_MESSAGE*>(data.data()); Check(debug->GetMessage(i,msg,&size));
            if(msg->Severity<=D3D11_MESSAGE_SEVERITY_WARNING) throw std::runtime_error(msg->pDescription);
        }
    }
};
D3D12_RESOURCE_BARRIER Barrier(ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
    D3D12_RESOURCE_BARRIER b{}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after}; return b;
}
struct Host12 : Host {
    ComPtr<ID3D12Device> device; ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Resource> back, upload, readback; ComPtr<ID3D12Fence> fence;
    ComPtr<ID3D12InfoQueue> debug; HANDLE event=nullptr; UINT64 serial=0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 total=0;
    Host12(IDXGIFactory7* factory,HWND window) {
        Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))); HRESULT debugHr=device.As(&debug); if(requireDebug) Check(debugHr);
        D3D12_COMMAND_QUEUE_DESC desc{}; Check(device->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue)));
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
        Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))); Check(list->Close());
        Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))); event=CreateEventW(nullptr,FALSE,FALSE,nullptr); Require(event!=nullptr,"Fence event failed");
        swap=MakeSwap(factory,queue.Get(),window,width,height,true);
    }
    ~Host12() override { if(event) CloseHandle(event); }
    void Begin() { Check(allocator->Reset()); Check(list->Reset(allocator.Get(),nullptr)); }
    void Submit() {
        Check(list->Close()); ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1,lists);
        Check(queue->Signal(fence.Get(),++serial)); Check(fence->SetEventOnCompletion(serial,event));
        Require(WaitForSingleObject(event,5000)==WAIT_OBJECT_0,"Test GPU fence timeout");
    }
    ComPtr<ID3D12Resource> Buffer(D3D12_HEAP_TYPE heapType,D3D12_RESOURCE_STATES state) {
        D3D12_HEAP_PROPERTIES heap{}; heap.Type=heapType;
        D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width=total; desc.Height=1;
        desc.DepthOrArraySize=desc.MipLevels=1; desc.SampleDesc.Count=1; desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> buffer; Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&buffer))); return buffer;
    }
    void Upload(const std::vector<BYTE>& pixels) override {
        ReleaseBack(); Check(swap->GetBuffer(swap->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&back)));
        const auto desc=back->GetDesc(); device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&total);
        upload=Buffer(D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        readback=Buffer(D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        BYTE* mapped=nullptr; D3D12_RANGE empty{0,0}; Check(upload->Map(0,&empty,reinterpret_cast<void**>(&mapped)));
        for(UINT y=0;y<height;++y) memcpy(mapped+footprint.Offset+static_cast<size_t>(y)*footprint.Footprint.RowPitch,pixels.data()+static_cast<size_t>(y)*width*4,width*4);
        upload->Unmap(0,nullptr);
        Begin(); auto before=Barrier(back.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST); list->ResourceBarrier(1,&before);
        D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource=upload.Get(); source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; source.PlacedFootprint=footprint;
        D3D12_TEXTURE_COPY_LOCATION target{}; target.pResource=back.Get(); target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        list->CopyTextureRegion(&target,0,0,0,&source,nullptr);
        auto after=Barrier(back.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT); list->ResourceBarrier(1,&after); Submit();
    }
    std::vector<BYTE> Read() override {
        Begin(); auto before=Barrier(back.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE); list->ResourceBarrier(1,&before);
        D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource=back.Get(); source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION target{}; target.pResource=readback.Get(); target.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; target.PlacedFootprint=footprint;
        list->CopyTextureRegion(&target,0,0,0,&source,nullptr);
        auto after=Barrier(back.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT); list->ResourceBarrier(1,&after); Submit();
        BYTE* mapped=nullptr; D3D12_RANGE range{0,static_cast<SIZE_T>(total)}; Check(readback->Map(0,&range,reinterpret_cast<void**>(&mapped)));
        std::vector<BYTE> result(static_cast<size_t>(width)*height*4);
        for(UINT y=0;y<height;++y) memcpy(result.data()+static_cast<size_t>(y)*width*4,mapped+footprint.Offset+static_cast<size_t>(y)*footprint.Footprint.RowPitch,width*4);
        D3D12_RANGE empty{0,0}; readback->Unmap(0,&empty); return result;
    }
    void ReleaseBack() override { back.Reset(); upload.Reset(); readback.Reset(); }
    void DebugCheck() override {
        if(!debug) return;
        for(UINT64 i=0;i<debug->GetNumStoredMessages();++i) {
            SIZE_T size=0; Check(debug->GetMessage(i,nullptr,&size)); std::vector<BYTE> data(size);
            auto* msg=reinterpret_cast<D3D12_MESSAGE*>(data.data()); Check(debug->GetMessage(i,msg,&size));
            if(msg->Severity<=D3D12_MESSAGE_SEVERITY_WARNING) throw std::runtime_error(msg->pDescription);
        }
    }
};
void Bounds(const std::vector<BYTE>& before,const std::vector<BYTE>& after,UINT w,UINT h) {
    for(UINT y=0;y<h;++y) for(UINT x=0;x<w;++x) for(UINT c=0;c<4;++c) {
        const auto index=(static_cast<size_t>(y)*w+x)*4+c;
        if(c==3) { Require(before[index]==after[index],"Alpha changed"); continue; }
        BYTE lo=255,hi=0;
        for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
            const auto sx=std::clamp<int>(static_cast<int>(x)+dx,0,static_cast<int>(w)-1), sy=std::clamp<int>(static_cast<int>(y)+dy,0,static_cast<int>(h)-1);
            const auto value=before[(static_cast<size_t>(sy)*w+sx)*4+c]; lo=std::min(lo,value); hi=std::max(hi,value);
        }
        if(after[index]<lo || after[index]>hi) throw std::runtime_error("Native bounds x="+std::to_string(x)+" y="+std::to_string(y)+" c="+std::to_string(c)+" result="+std::to_string(after[index])+" lo="+std::to_string(lo)+" hi="+std::to_string(hi));
    }
}
void Test(Host& host,native::Shared* shared,std::ostream& log,int api) {
    const auto input=Pixels(host.width,host.height);
    native::Options options{};
    options.enabled=0; Require(native::Write(shared,options),"Initial bypass write failed");
    host.Upload(input); host.Process(); Require(host.Read()==input,"Bypass changed pixels");
    options.enabled=1;
    Require(native::Write(shared,options),"IPC write failed");
    host.Upload(input); Require(host.Read()==input,"Host upload/readback mismatch before native filter"); const auto count=shared->frames; Check(host.swap->Present(0,DXGI_PRESENT_TEST));
    Require(shared->frames==count,"Present(TEST) ran filter");
    Check(host.swap->Present(0,0)); Require(shared->frames==count+1,"Actual Present did not invoke native filter");
    host.Upload(input); host.Process(); host.StateCheck(); auto output=host.Read();
    host.DebugCheck();
    Require(shared->state==(api==11?native::Active11:native::Active12),"Native backend did not become active");
    Require(output!=input,"Native filter did not change pixels"); Bounds(input,output,host.width,host.height);
    log<<"PASS DX"<<api<<": actual Present intercepted; pixels changed, bounds/alpha and COM identity preserved\n";
    options.enabled=0; Require(native::Write(shared,options),"IPC disable failed"); host.Upload(input); host.Process();
    Require(host.Read()==input,"Disabled filter changed pixels");
    options.enabled=1; options.strength=options.detail=0; native::Write(shared,options); host.Upload(input); host.Process();
    Require(host.Read()==input,"Zero gain changed pixels");
    options={}; options.split=1; native::Write(shared,options); host.Upload(input); DXGI_PRESENT_PARAMETERS params{};
    const auto present1Count=shared->frames; Check(host.swap->Present1(0,0,&params)); Require(shared->frames==present1Count+1,"Present1 did not invoke native filter");
    host.Upload(input); host.Process(); output=host.Read();
    Require(output!=input,"Present1 split had no effect");
    for(UINT y=0;y<host.height;++y) for(UINT x=0;x<host.width/2;++x) for(UINT c=0;c<4;++c) {
        auto i=(static_cast<size_t>(y)*host.width+x)*4+c; Require(output[i]==input[i],"Split changed original half");
    }
    options={}; native::Write(shared,options);
    for(const auto size: {std::pair{91u,51u},std::pair{32u,17u},std::pair{65u,37u}}) {
        host.Resize(size.first,size.second); auto pixels=Pixels(size.first,size.second); host.Upload(pixels); host.Process();
        auto result=host.Read(); Require(result!=pixels,"Filter stopped after resize"); Bounds(pixels,result,size.first,size.second);
    }
    host.ReleaseBack(); Check(host.swap->ResizeBuffers(2,host.width,host.height,DXGI_FORMAT_R10G10B10A2_UNORM,0));
    host.Upload(input); host.Process(); Require(shared->state==native::Unsupported,"Unsupported format was not bypassed");
    Require(host.Read()==input,"Unsupported format was modified");
    host.Resize(65,37);
    // The panel must remain usable with sharpening OFF, and preserve every pixel
    // outside its bounds as well as the backbuffer's alpha channel.
    ComPtr<native::Diagnostics> diagnostics; Check(host.swap.As(&diagnostics));
    options={}; options.enabled=0; Require(native::Write(shared,options),"Panel bypass write failed");
    for(const auto size: {std::pair{640u,360u},std::pair{900u,600u},std::pair{1280u,720u}}) {
        host.Resize(size.first,size.second);
        const auto pixels=Pixels(host.width,host.height);
        Check(diagnostics->MenuForValidation(TRUE,0)); host.Upload(pixels); host.Process(); host.StateCheck();
        auto panel=host.Read(); Require(shared->menuVisible==1,"Panel did not open");
        Require(shared->state==native::Disabled,"Panel unexpectedly enabled sharpening");
        Require(panel!=pixels,"Panel did not draw while filter was disabled");
        // These sizes all use the 0.75 scale: (18,18), 270 x 264 pixels.
        for(UINT y=0;y<host.height;++y) for(UINT x=0;x<host.width;++x) for(UINT c=0;c<4;++c) {
            const auto i=(static_cast<size_t>(y)*host.width+x)*4+c;
            if(c==3 || x<18 || x>=288 || y<18 || y>=282) Require(panel[i]==pixels[i],"Panel changed alpha or pixels outside its bounds");
        }
        host.Upload(pixels); host.Process(); Require(host.Read()==panel,"Cached panel changed unexpectedly");
        Check(diagnostics->MenuForValidation(TRUE,2)); host.Upload(pixels); host.Process();
        Require(host.Read()!=panel,"Panel selection did not redraw");
        Check(diagnostics->MenuForValidation(FALSE,0)); host.Upload(pixels); host.Process();
        Require(shared->menuVisible==0 && host.Read()==pixels,"Closing panel left artifacts");
    }
    options={}; options.split=1; Require(native::Write(shared,options),"Combined panel/filter write failed");
    const auto combinedInput=Pixels(host.width,host.height);
    host.Upload(combinedInput); host.Process(); const auto filtered=host.Read();
    Check(diagnostics->MenuForValidation(TRUE,2)); host.Upload(combinedInput); host.Process(); host.StateCheck();
    const auto combined=host.Read(); Require(combined!=filtered,"Combined panel/filter did not draw");
    for(UINT y=0;y<host.height;++y) for(UINT x=0;x<host.width;++x) for(UINT c=0;c<4;++c) {
        const auto i=(static_cast<size_t>(y)*host.width+x)*4+c;
        if(c==3 || x<18 || x>=288 || y<18 || y>=282) Require(combined[i]==filtered[i],"Panel changed split filtering outside its bounds");
    }
    Check(diagnostics->MenuForValidation(FALSE,0));
    host.DebugCheck();
    log<<"PASS DX"<<api<<": live IPC enable/disable, zero gain, Present1 split, three resizes, unsupported-format passthrough, no debug-layer warnings/errors\n";
    log<<"PASS DX"<<api<<": panel draws with filter disabled or split enabled, stays inside bounds, preserves alpha/state, updates selection/cache, survives resizes, and closes without artifacts\n";
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int show) {
    wchar_t executable[32768]{}; GetModuleFileNameW(nullptr,executable,32768); const auto folder=std::filesystem::path(executable).parent_path();
    i18n::RefreshPreference();
    const std::wstring args=command; const bool demo=args.find(L"--demo")!=std::wstring::npos;
    std::ofstream log(folder/(demo?L"native-demo.txt":L"native-validation.txt"));
    try {
        const bool twelve=demo && args.find(L"--dx11")==std::wstring::npos;
        requireDebug=!demo;
        ComPtr<ID3D12Debug> debug; HRESULT debugHr=D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
        if(SUCCEEDED(debugHr)) debug->EnableDebugLayer(); else if(requireDebug) Check(debugHr);
        HMODULE library=LoadLibraryExW((folder/L"dxgi.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        Require(library && GetProcAddress(library,"DLSSClearNativeVersion"),"Own native DLL missing");
        using Create=HRESULT(WINAPI*)(UINT,REFIID,void**); auto create=reinterpret_cast<Create>(GetProcAddress(library,"CreateDXGIFactory2")); Require(create!=nullptr,"Factory export missing");
        ComPtr<IDXGIFactory7> factory; Check(create(debug?DXGI_CREATE_FACTORY_DEBUG:0,IID_PPV_ARGS(&factory)));
        WNDCLASSW cls{}; cls.hInstance=instance; cls.lpszClassName=L"DLSSClearNativeTest"; cls.lpfnWndProc=WindowProc;
        Require(RegisterClassW(&cls)!=0,"Window registration failed");
        for(int api : demo ? std::vector<int>{twelve?12:11} : std::vector<int>{11,12}) {
            HWND window=CreateWindowW(cls.lpszClassName,(std::wstring(L"DLSSClear · ")+i18n::Text(api==11?L"Kendi bağlantımız · DX11":L"Kendi bağlantımız · DX12")).c_str(),WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,960,600,nullptr,nullptr,instance,nullptr);
            Require(window!=nullptr,"Window creation failed");
            std::unique_ptr<Host> host;
            if(api==11) host=std::make_unique<Host11>(factory.Get(),window); else host=std::make_unique<Host12>(factory.Get(),window);
            HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,native::MappingName(GetCurrentProcessId()).c_str()); Require(mapping!=nullptr,"Runtime IPC mapping absent");
            auto* shared=static_cast<native::Shared*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(native::Shared))); Require(native::Valid(shared),"IPC protocol invalid");
            if(demo) {
                host->Resize(1280,720); ShowWindow(window,show); bool running=true; UINT frame=0;
                while(running) {
                    MSG message{}; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { if(message.message==WM_QUIT) running=false; TranslateMessage(&message); DispatchMessageW(&message); }
                    if(!running) break;
                    host->Upload(Pixels(host->width,host->height,frame++/4)); Check(host->swap->Present(1,0)); host->Read();
                    Sleep(16); // Test host CPU uploads/readbacks are deliberately not a performance benchmark.
                }
            } else { ShowWindow(window,SW_SHOWNOACTIVATE); Test(*host,shared,log,api); }
            host.reset(); UnmapViewOfFile(shared); CloseHandle(mapping); if(IsWindow(window)) DestroyWindow(window);
        }
        log<<(demo?"Demo closed. Automated validations were not run.\n":"All native validation checks passed. No game quality/FPS claim.\n"); return 0;
    } catch(const std::exception& error) { log<<"FAIL: "<<error.what()<<'\n'; return 1; }
}
