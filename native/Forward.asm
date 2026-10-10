option casemap:none
EXTERN NativeResolve:PROC
.data
s_ApplyCompatResolutionQuirking DB "ApplyCompatResolutionQuirking",0
s_CompatString DB "CompatString",0
s_CompatValue DB "CompatValue",0
s_DXGIDumpJournal DB "DXGIDumpJournal",0
s_PIXBeginCapture DB "PIXBeginCapture",0
s_PIXEndCapture DB "PIXEndCapture",0
s_PIXGetCaptureState DB "PIXGetCaptureState",0
s_SetAppCompatStringPointer DB "SetAppCompatStringPointer",0
s_UpdateHMDEmulationStatus DB "UpdateHMDEmulationStatus",0
s_DXGID3D10CreateDevice DB "DXGID3D10CreateDevice",0
s_DXGID3D10CreateLayeredDevice DB "DXGID3D10CreateLayeredDevice",0
s_DXGID3D10GetLayeredDeviceSize DB "DXGID3D10GetLayeredDeviceSize",0
s_DXGID3D10RegisterLayers DB "DXGID3D10RegisterLayers",0
s_DXGIDeclareAdapterRemovalSupport DB "DXGIDeclareAdapterRemovalSupport",0
s_DXGIDisableVBlankVirtualization DB "DXGIDisableVBlankVirtualization",0
s_DXGIGetDebugInterface1 DB "DXGIGetDebugInterface1",0
s_DXGIReportAdapterConfiguration DB "DXGIReportAdapterConfiguration",0
.code
Forward_ApplyCompatResolutionQuirking PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_ApplyCompatResolutionQuirking
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_ApplyCompatResolutionQuirking ENDP
Forward_CompatString PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_CompatString
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_CompatString ENDP
Forward_CompatValue PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_CompatValue
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_CompatValue ENDP
Forward_DXGIDumpJournal PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGIDumpJournal
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGIDumpJournal ENDP
Forward_PIXBeginCapture PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_PIXBeginCapture
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_PIXBeginCapture ENDP
Forward_PIXEndCapture PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_PIXEndCapture
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_PIXEndCapture ENDP
Forward_PIXGetCaptureState PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_PIXGetCaptureState
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_PIXGetCaptureState ENDP
Forward_SetAppCompatStringPointer PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_SetAppCompatStringPointer
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_SetAppCompatStringPointer ENDP
Forward_UpdateHMDEmulationStatus PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_UpdateHMDEmulationStatus
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_UpdateHMDEmulationStatus ENDP
Forward_DXGID3D10CreateDevice PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGID3D10CreateDevice
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGID3D10CreateDevice ENDP
Forward_DXGID3D10CreateLayeredDevice PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGID3D10CreateLayeredDevice
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGID3D10CreateLayeredDevice ENDP
Forward_DXGID3D10GetLayeredDeviceSize PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGID3D10GetLayeredDeviceSize
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGID3D10GetLayeredDeviceSize ENDP
Forward_DXGID3D10RegisterLayers PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGID3D10RegisterLayers
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGID3D10RegisterLayers ENDP
Forward_DXGIDeclareAdapterRemovalSupport PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGIDeclareAdapterRemovalSupport
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGIDeclareAdapterRemovalSupport ENDP
Forward_DXGIDisableVBlankVirtualization PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGIDisableVBlankVirtualization
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGIDisableVBlankVirtualization ENDP
Forward_DXGIGetDebugInterface1 PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGIGetDebugInterface1
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGIGetDebugInterface1 ENDP
Forward_DXGIReportAdapterConfiguration PROC FRAME
    sub rsp,0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h],rcx
    mov [rsp+28h],rdx
    mov [rsp+30h],r8
    mov [rsp+38h],r9
    movdqu [rsp+40h],xmm0
    movdqu [rsp+50h],xmm1
    movdqu [rsp+60h],xmm2
    movdqu [rsp+70h],xmm3
    lea rcx,s_DXGIReportAdapterConfiguration
    call NativeResolve
    mov r11,rax
    mov rcx,[rsp+20h]
    mov rdx,[rsp+28h]
    mov r8,[rsp+30h]
    mov r9,[rsp+38h]
    movdqu xmm0,[rsp+40h]
    movdqu xmm1,[rsp+50h]
    movdqu xmm2,[rsp+60h]
    movdqu xmm3,[rsp+70h]
    add rsp,0A8h
    jmp r11
Forward_DXGIReportAdapterConfiguration ENDP
END
