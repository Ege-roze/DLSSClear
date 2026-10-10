// Mechanical forwarding declarations from Windows SDK COM interfaces.
HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) override { return real_->SetPrivateData(Name, DataSize, pData); }
HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) override { return real_->SetPrivateDataInterface(Name, pUnknown); }
HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) override { return real_->GetPrivateData(Name, pDataSize, pData); }
HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice) override { return real_->GetDevice(riid, ppDevice); }
HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void **ppSurface) override { return real_->GetBuffer(Buffer, riid, ppSurface); }
HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput *pTarget) override { return real_->SetFullscreenState(Fullscreen, pTarget); }
HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL *pFullscreen, IDXGIOutput **ppTarget) override { return real_->GetFullscreenState(pFullscreen, ppTarget); }
HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC *pDesc) override { return real_->GetDesc(pDesc); }
HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC *pNewTargetParameters) override { return real_->ResizeTarget(pNewTargetParameters); }
HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput **ppOutput) override { return real_->GetContainingOutput(ppOutput); }
HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS *pStats) override { return real_->GetFrameStatistics(pStats); }
HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT *pLastPresentCount) override { return real_->GetLastPresentCount(pLastPresentCount); }
HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 *pDesc) override { return real_->GetDesc1(pDesc); }
HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pDesc) override { return real_->GetFullscreenDesc(pDesc); }
HRESULT STDMETHODCALLTYPE GetHwnd(HWND *pHwnd) override { return real_->GetHwnd(pHwnd); }
HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void **ppUnk) override { return real_->GetCoreWindow(refiid, ppUnk); }
BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported(void) override { return real_->IsTemporaryMonoSupported(); }
HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput **ppRestrictToOutput) override { return real_->GetRestrictToOutput(ppRestrictToOutput); }
HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA *pColor) override { return real_->SetBackgroundColor(pColor); }
HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA *pColor) override { return real_->GetBackgroundColor(pColor); }
HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override { return real_->SetRotation(Rotation); }
HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION *pRotation) override { return real_->GetRotation(pRotation); }
HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override { return real_->SetSourceSize(Width, Height); }
HRESULT STDMETHODCALLTYPE GetSourceSize(UINT *pWidth, UINT *pHeight) override { return real_->GetSourceSize(pWidth, pHeight); }
HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override { return real_->SetMaximumFrameLatency(MaxLatency); }
HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *pMaxLatency) override { return real_->GetMaximumFrameLatency(pMaxLatency); }
HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject(void) override { return real_->GetFrameLatencyWaitableObject(); }
HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F *pMatrix) override { return real_->SetMatrixTransform(pMatrix); }
HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F *pMatrix) override { return real_->GetMatrixTransform(pMatrix); }
UINT STDMETHODCALLTYPE GetCurrentBackBufferIndex(void) override { return real_->GetCurrentBackBufferIndex(); }
HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT *pColorSpaceSupport) override { return real_->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport); }
HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void *pMetaData) override { return real_->SetHDRMetaData(Type, Size, pMetaData); }
