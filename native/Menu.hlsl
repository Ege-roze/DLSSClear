Texture2D<float4> Panel : register(t0);
cbuffer Position : register(b0) { float2 Origin; float2 Padding; };
float4 MenuPS(float4 position : SV_POSITION) : SV_TARGET {
    return Panel.Load(int3(int2(position.xy)-int2(Origin),0));
}
