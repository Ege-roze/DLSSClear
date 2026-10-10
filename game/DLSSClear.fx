// DLSSClear: SDR spatial sharpening after the game's final image.
// Independent ReShade FX port; no DLSS DLL or proprietary model is modified.
// Algorithm matches Clear.hlsl at radius 1. Alpha and sRGB code values preserved.
texture2D DLSSClearBackbuffer : COLOR;
sampler2D DLSSClearSampler { Texture = DLSSClearBackbuffer; SRGBTexture = false; };

uniform int Profile <
    ui_type = "combo"; ui_label = "Profil";
    ui_items = "Yumusak\0Dengeli\0Belirgin\0Ozel\0";
> = 1;
uniform float Strength < ui_type = "slider"; ui_min = 0.0; ui_max = 2.0;
    ui_label = "Ozel: keskinlik"; ui_tooltip = "Profil = Ozel iken kullanilir.";
> = 0.65;
uniform float Detail < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0;
    ui_label = "Ozel: ince ayrinti kontrasti";
> = 0.20;
uniform float NoiseFloor < ui_type = "slider"; ui_min = 0.0; ui_max = 0.05; ui_step = 0.001;
    ui_label = "Ozel: gurultu korumasi";
> = 0.012;
uniform bool AntiRinging < ui_label = "Kenar halelerini sinirla"; > = true;
uniform bool SplitView < ui_label = "Karsilastir: sol orijinal / sag islenmis"; > = false;
uniform float Split < ui_type = "slider"; ui_min = 0.0; ui_max = 1.0;
    ui_label = "Karsilastirma cizgisi";
> = 0.5;

float4 DLSSClearVS(uint id : SV_VertexID) : SV_POSITION {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float DLSSClearLuma(float3 rgb) { return dot(rgb, float3(.2126, .7152, .0722)); }
float4 DLSSClearRead(int2 p) {
    return tex2Dfetch(DLSSClearSampler, clamp(p, int2(0, 0), int2(BUFFER_WIDTH, BUFFER_HEIGHT) - 1));
}
float4 DLSSClearPS(float4 position : SV_POSITION) : SV_TARGET {
    int2 p = int2(position.xy);
    float4 center = DLSSClearRead(p);
    // A safe passthrough for HDR/scRGB. Do not clamp HDR to SDR.
#if BUFFER_COLOR_SPACE > 1 || BUFFER_COLOR_BIT_DEPTH > 10
    return center;
#else
    if (SplitView && position.x < BUFFER_WIDTH * Split) return center;
    float3 parameters = float3(Strength, Detail, NoiseFloor);
    if (Profile == 0) parameters = float3(.35, .10, .020);
    if (Profile == 1) parameters = float3(.65, .20, .012);
    if (Profile == 2) parameters = float3(1.15, .35, .008);
    float3 lo = center.rgb, hi = center.rgb;
    float centerLuma = DLSSClearLuma(center.rgb);
    float lmin = centerLuma, lmax = centerLuma, sum = 0;
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            float3 c = (x == 0 && y == 0) ? center.rgb : DLSSClearRead(p + int2(x, y)).rgb;
            float weight = (x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0);
            lo = min(lo, c); hi = max(hi, c);
            float l = DLSSClearLuma(c); lmin = min(lmin, l); lmax = max(lmax, l);
            sum += l * weight;
        }
    }
    float range = lmax - lmin;
    float residual = centerLuma - sum / 16.0;
    float gate = smoothstep(parameters.z, parameters.z * 3.0 + .0001, range);
    float microContrast = 1.0 - smoothstep(.04, .25, range);
    float gain = (parameters.x + parameters.y * microContrast) * gate / (1.0 + range * 4.0);
    float3 candidate = center.rgb + residual * gain;
    if (AntiRinging) candidate = clamp(candidate, lo, hi);
    return float4(saturate(candidate), center.a);
#endif
}
technique DLSSClear < ui_label = "DLSSClear (SDR)";
    ui_tooltip = "SDR netlik filtresi. HDR cikista otomatik pasif. Home: panel; End: ac/kapat.";
> {
    pass { VertexShader = DLSSClearVS; PixelShader = DLSSClearPS; SRGBWriteEnable = false; }
}
