#include "Image.h"
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
namespace {
void Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) throw std::runtime_error(std::string(what) + " (HRESULT " + std::to_string(static_cast<unsigned>(hr)) + ")");
}
ComPtr<IWICImagingFactory> Factory() {
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)), "WIC factory");
    return factory;
}
}
Image LoadImageFile(const std::filesystem::path& path) {
    auto factory = Factory();
    ComPtr<IWICBitmapDecoder> decoder;
    Check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder), "Goruntu acilamadi");
    ComPtr<IWICBitmapFrameDecode> frame;
    Check(decoder->GetFrame(0, &frame), "WIC frame");
    Image result;
    Check(frame->GetSize(&result.width, &result.height), "WIC size");
    // Bound CPU and GPU allocations; sufficient for 8K SDR screenshots.
    if (!result.width || !result.height || result.width > 16384 || result.height > 16384 ||
        static_cast<std::uint64_t>(result.width) * result.height > 40000000)
        throw std::runtime_error("Goruntu en fazla 40 megapiksel ve kenar basina 16384 piksel olabilir.");
    ComPtr<IWICFormatConverter> converter;
    Check(factory->CreateFormatConverter(&converter), "WIC converter");
    Check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
        nullptr, 0, WICBitmapPaletteTypeCustom), "WIC RGBA conversion");
    result.pixels.resize(static_cast<size_t>(result.width) * result.height * 4);
    Check(converter->CopyPixels(nullptr, result.width * 4, static_cast<UINT>(result.pixels.size()), result.pixels.data()), "WIC pixels");
    return result;
}
void SavePng(const std::filesystem::path& path, const Image& image) {
    auto factory = Factory();
    ComPtr<IWICStream> stream;
    Check(factory->CreateStream(&stream), "WIC stream");
    Check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "PNG dosyasi acilamadi");
    ComPtr<IWICBitmapEncoder> encoder;
    Check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "PNG encoder");
    Check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache), "PNG initialize");
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    Check(encoder->CreateNewFrame(&frame, &properties), "PNG frame");
    Check(frame->Initialize(properties.Get()), "PNG frame initialize");
    Check(frame->SetSize(image.width, image.height), "PNG size");
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    Check(frame->SetPixelFormat(&format), "PNG format");
    if (format != GUID_WICPixelFormat32bppBGRA) throw std::runtime_error("PNG BGRA desteklenmiyor.");
    auto bgra = image.pixels;
    for (size_t i = 0; i < bgra.size(); i += 4) std::swap(bgra[i], bgra[i + 2]);
    Check(frame->WritePixels(image.height, image.width * 4, static_cast<UINT>(image.pixels.size()),
        bgra.data()), "PNG pixels");
    Check(frame->Commit(), "PNG frame commit");
    Check(encoder->Commit(), "PNG commit");
}
Image MakeTestImage(unsigned width, unsigned height) {
    Image result{width, height, std::vector<std::uint8_t>(static_cast<size_t>(width) * height * 4)};
    for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
        float u = static_cast<float>(x) / width, v = static_cast<float>(y) / height;
        float r, g, b;
        if (v < .5f && u < .5f) { // Soft diagonal edges, useful for halo inspection.
            float edge = .5f + .5f * std::tanh((u - v + .07f) * 170.f);
            r = .14f + .66f * edge; g = .2f + .42f * edge; b = .3f + .25f * edge;
        } else if (v < .5f) { // Increasing spatial frequency, low contrast.
            float wave = std::sin(u * u * 700.f + v * 40.f);
            r = .4f + .07f * wave; g = .5f + .07f * wave; b = .62f + .07f * wave;
        } else if (u < .5f) { // Fine grid and soft structure.
            float grid = std::pow(.5f + .5f * std::cos(u * 350.f), 6.f) * .22f;
            float wave = std::sin(v * 100.f) * .07f;
            r = .22f + grid + wave; g = .4f + grid + wave; b = .35f + grid + wave;
        } else { // Smooth gradient with deterministic faint noise.
            unsigned hash = (x * 1973u + y * 9277u + 89173u) * 60493u;
            float noise = (static_cast<float>(hash & 255) / 255.f - .5f) * .018f;
            r = .1f + .8f * (u - .5f) * 2.f + noise;
            g = .16f + .6f * (v - .5f) * 2.f + noise; b = .3f + noise;
        }
        const size_t i = (static_cast<size_t>(y) * width + x) * 4;
        result.pixels[i] = static_cast<std::uint8_t>(std::clamp(r, 0.f, 1.f) * 255.f + .5f);
        result.pixels[i + 1] = static_cast<std::uint8_t>(std::clamp(g, 0.f, 1.f) * 255.f + .5f);
        result.pixels[i + 2] = static_cast<std::uint8_t>(std::clamp(b, 0.f, 1.f) * 255.f + .5f);
        result.pixels[i + 3] = 255;
    }
    return result;
}
