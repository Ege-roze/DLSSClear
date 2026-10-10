// Mechanical forwarding declarations from Windows SDK COM interfaces.
HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override { return real_->SetPrivateData(Name, DataSize, pData); }
HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override { return real_->SetPrivateDataInterface(Name, pUnknown); }
HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override { return real_->GetPrivateData(Name, pDataSize, pData); }
HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) override { return real_->GetParent(riid, ppParent); }
HRESULT STDMETHODCALLTYPE EnumAdapters(UINT Adapter, IDXGIAdapter **ppAdapter) override { return real_->EnumAdapters(Adapter, ppAdapter); }
HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags) override { return real_->MakeWindowAssociation(WindowHandle, Flags); }
HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND *pWindowHandle) override { return real_->GetWindowAssociation(pWindowHandle); }
HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter **ppAdapter) override { return real_->CreateSoftwareAdapter(Module, ppAdapter); }
HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT Adapter, IDXGIAdapter1 **ppAdapter) override { return real_->EnumAdapters1(Adapter, ppAdapter); }
BOOL STDMETHODCALLTYPE IsCurrent(void) override { return real_->IsCurrent(); }
BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled(void) override { return real_->IsWindowedStereoEnabled(); }
HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(HANDLE hResource, LUID *pLuid) override { return real_->GetSharedResourceAdapterLuid(hResource, pLuid); }
HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie) override { return real_->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie); }
HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(HANDLE hEvent, DWORD *pdwCookie) override { return real_->RegisterStereoStatusEvent(hEvent, pdwCookie); }
void STDMETHODCALLTYPE UnregisterStereoStatus(DWORD dwCookie) override { return real_->UnregisterStereoStatus(dwCookie); }
HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD *pdwCookie) override { return real_->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie); }
HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD *pdwCookie) override { return real_->RegisterOcclusionStatusEvent(hEvent, pdwCookie); }
void STDMETHODCALLTYPE UnregisterOcclusionStatus(DWORD dwCookie) override { return real_->UnregisterOcclusionStatus(dwCookie); }
UINT STDMETHODCALLTYPE GetCreationFlags(void) override { return real_->GetCreationFlags(); }
HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(LUID AdapterLuid, REFIID riid, void **ppvAdapter) override { return real_->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter); }
HRESULT STDMETHODCALLTYPE EnumWarpAdapter(REFIID riid, void **ppvAdapter) override { return real_->EnumWarpAdapter(riid, ppvAdapter); }
HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE Feature, void *pFeatureSupportData, UINT FeatureSupportDataSize) override { return real_->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize); }
HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void **ppvAdapter) override { return real_->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter); }
HRESULT STDMETHODCALLTYPE RegisterAdaptersChangedEvent(HANDLE hEvent, DWORD *pdwCookie) override { return real_->RegisterAdaptersChangedEvent(hEvent, pdwCookie); }
HRESULT STDMETHODCALLTYPE UnregisterAdaptersChangedEvent(DWORD dwCookie) override { return real_->UnregisterAdaptersChangedEvent(dwCookie); }
