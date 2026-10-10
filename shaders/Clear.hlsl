// One spatial enhancement pass. Input/output are SDR sRGB code values (UNORM),
// deliberately not an HDR or temporal reconstruction pipeline.
Texture2D<float4> Source : register(t0);
Texture2D<float4> Enhanced : register(t1);
SamplerState LinearClamp : register(s0);

cbuffer Settings : register(b0) {
    uint2 ImageSize;
    float Strength;
    float Detail;
    float NoiseFloor;
    float AntiRinging;
    uint ViewMode; // 0 original, 1 enhanced, 2 split, 3 absolute difference x8
    float Split;
    float2 ViewSize;
    float Scale;
    uint Radius; // Photo sample spacing, 1..3 pixels. Games keep radius 1.
    float2 Pan;
    float2 Padding2;
};

float4 FullscreenVS(uint id : SV_VertexID) : SV_POSITION {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
float Luma(float3 rgb) { return dot(rgb, float3(.2126, .7152, .0722)); }
float4 Read(int2 p) { return Source.Load(int3(clamp(p, int2(0, 0), int2(ImageSize) - 1), 0)); }

float4 SharpenPS(float4 position : SV_POSITION) : SV_TARGET {
    int2 p = int2(position.xy);
    float4 center = Read(p);
    float3 lo = center.rgb, hi = center.rgb;
    float centerLuma = Luma(center.rgb);
    float lmin = centerLuma, lmax = centerLuma, sum = 0;
    // Separable [1 2 1] Gaussian expressed as nine reads in one pass.
    [unroll] for (int y = -1; y <= 1; ++y) {
        [unroll] for (int x = -1; x <= 1; ++x) {
            float3 c = (x == 0 && y == 0) ? center.rgb : Read(p + int2(x, y) * int(clamp(Radius, 1u, 3u))).rgb;
            float weight = (x == 0 ? 2.0 : 1.0) * (y == 0 ? 2.0 : 1.0);
            lo = min(lo, c); hi = max(hi, c);
            float l = Luma(c); lmin = min(lmin, l); lmax = max(lmax, l);
            // Luminance is already needed for the range: reuse it for the blur.
            sum += l * weight;
        }
    }
    float range = lmax - lmin;
    float residual = centerLuma - sum / 16.0;
    // Suppress weak texture/noise and reduce gain on strong edges.
    float gate = smoothstep(NoiseFloor, NoiseFloor * 3.0 + .0001, range);
    float microContrast = 1.0 - smoothstep(.04, .25, range);
    float gain = (Strength + Detail * microContrast) * gate / (1.0 + range * 4.0);
    float3 candidate = center.rgb + residual * gain;
    // Per-channel bounds prohibit new local extrema when enabled.
    if (AntiRinging > .5) candidate = clamp(candidate, lo, hi);
    return float4(saturate(candidate), center.a);
}

float4 DisplayPS(float4 position : SV_POSITION) : SV_TARGET {
    float2 pixel = (position.xy - ViewSize * .5 - Pan) / Scale + float2(ImageSize) * .5;
    if (any(pixel < 0) || any(pixel >= float2(ImageSize))) return float4(.028, .035, .05, 1);
    float2 uv = pixel / float2(ImageSize);
    float4 original, processed;
    if (abs(Scale - 1.0) < .0001) {
        original = Source.Load(int3(int2(pixel), 0));
        processed = Enhanced.Load(int3(int2(pixel), 0));
    } else {
        original = Source.SampleLevel(LinearClamp, uv, 0);
        processed = Enhanced.SampleLevel(LinearClamp, uv, 0);
    }
    // Inspect full-resolution pixel changes even in fit view. Downsampling the
    // signed difference first would hide alternating positive/negative changes.
    if (ViewMode == 3) {
        int2 p = clamp(int2(pixel), int2(0, 0), int2(ImageSize) - 1);
        float4 a = Source.Load(int3(p, 0));
        float4 b = Enhanced.Load(int3(p, 0));
        return float4(saturate(abs(b.rgb - a.rgb) * 8.0) * a.a, 1);
    }
    float4 color = ViewMode == 0 ? original : processed;
    if (ViewMode == 2) {
        color = position.x < ViewSize.x * Split ? original : processed;
        if (abs(position.x - ViewSize.x * Split) < 1) return float4(.1, .85, .72, 1);
    }
    // Display straight alpha against a checkerboard; export keeps source alpha.
    float checker = fmod(floor(pixel.x / 16) + floor(pixel.y / 16), 2) ? .22 : .16;
    return float4(lerp(checker.xxx, color.rgb, color.a), 1);
}
