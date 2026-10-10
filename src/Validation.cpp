#include "Localization.h"
#include <set>
#include "Validation.h"
#include "Renderer.h"
#include "Games.h"
#include "NativeLink.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>

namespace {
void Require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void CheckBounds(const Image& source, const Image& output, unsigned radius = 1) {
    Require(source.width == output.width && source.height == output.height, "Output dimensions changed");
    for (unsigned y = 0; y < source.height; ++y) for (unsigned x = 0; x < source.width; ++x) {
        size_t index = (static_cast<size_t>(y) * source.width + x) * 4;
        Require(output.pixels[index + 3] == source.pixels[index + 3], "Alpha changed");
        for (unsigned c = 0; c < 3; ++c) {
            int lo = 255, hi = 0;
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                int sx = std::clamp(static_cast<int>(x) + dx * static_cast<int>(radius), 0, static_cast<int>(source.width) - 1);
                int sy = std::clamp(static_cast<int>(y) + dy * static_cast<int>(radius), 0, static_cast<int>(source.height) - 1);
                int v = source.pixels[(static_cast<size_t>(sy) * source.width + sx) * 4 + c];
                lo = std::min(lo, v); hi = std::max(hi, v);
            }
            int v = output.pixels[index + c];
            Require(v >= lo && v <= hi, "Anti-ringing created a local extremum");
        }
    }
}
void CheckCache(Renderer& renderer, const Image& input, std::ostream& log) {
    Settings s;
    renderer.Upload(input); renderer.Process(s);
    const auto originalOutput = renderer.Readback();
    const auto firstCount = renderer.FilterDrawCount();
    for (unsigned i = 0; i < 120; ++i) renderer.Process(s);
    Require(renderer.FilterDrawCount() == firstCount, "Unchanged input reran the filter");
    s.mode = 3; s.split = .25f; s.panX = 13; s.panY = -7; s.scale = 2; s.viewWidth = 711; s.viewHeight = 503;
    renderer.Process(s);
    Require(renderer.FilterDrawCount() == firstCount, "Display-only change reran the filter");
    Require(renderer.Readback().pixels == originalOutput.pixels, "Cached pixels changed");
    log << "PASS: 120 unchanged requests and display-only changes submit zero extra filter draws\n";
    for (float* parameter : {&s.strength, &s.detail, &s.noise, &s.antiRinging}) {
        *parameter += .01f;
        auto before = renderer.FilterDrawCount(); renderer.Process(s);
        Require(renderer.FilterDrawCount() == before + 1, "Filter-setting change failed to invalidate cache");
    }
    auto before = renderer.FilterDrawCount();
    s.radius = 2; renderer.Process(s);
    Require(renderer.FilterDrawCount() == before + 1, "Photo radius did not invalidate cache");
    before = renderer.FilterDrawCount();
    renderer.ReloadShaders(); renderer.Process(s);
    Require(renderer.FilterDrawCount() == before + 1, "Shader reload failed to invalidate cache");
    Image replacement = input; std::fill(replacement.pixels.begin(), replacement.pixels.end(), static_cast<BYTE>(127));
    before = renderer.FilterDrawCount(); renderer.Upload(replacement); renderer.Process(s);
    Require(renderer.FilterDrawCount() == before + 1, "Same-size upload failed to invalidate cache");
    Require(renderer.Readback().pixels == replacement.pixels, "Same-size upload retained stale pixels");
    renderer.Upload(input); s = Settings{}; renderer.Process(s);
    Settings other = s; other.strength = 1.7f;
    before = renderer.FilterDrawCount(); renderer.Measure(other, 3);
    Require(renderer.FilterDrawCount() == before + 15, "Benchmark did not execute all 12 warmup + 3 timed passes");
    before = renderer.FilterDrawCount(); renderer.Process(s);
    Require(renderer.FilterDrawCount() == before + 1, "Benchmark left a stale cache entry");
    Require(renderer.Readback().pixels == originalOutput.pixels, "Benchmark changed subsequent export");
    log << "PASS: settings, reload, same-size upload and benchmark invalidate cached output correctly\n";
}
void ComparePixels(Renderer& optimized, Renderer& reference, std::ostream& log) {
    std::vector<Image> fixtures{MakeTestImage(257,129), MakeTestImage(641,359), MakeTestImage(1,257),
        MakeTestImage(257,1), Image{1,1,{67,133,201,111}}};
    Image random{513,257,std::vector<BYTE>(513 * 257 * 4)};
    std::uint32_t seed = 0x12345678;
    for (auto& value : random.pixels) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; value = static_cast<BYTE>(seed & 255); }
    fixtures.push_back(std::move(random));
    std::vector<Settings> variants;
    for (const auto& profile : kFilterPresets) {
        if (profile.radius != 1) continue; // Reference predates photo controls.
        Settings s; s.strength = profile.strength; s.detail = profile.detail; s.noise = profile.noise; variants.push_back(s);
    }
    Settings bypass; bypass.strength = bypass.detail = 0; variants.push_back(bypass);
    Settings maximum; maximum.strength = 2; maximum.detail = 1; maximum.noise = 0; variants.push_back(maximum);
    maximum.antiRinging = 0; variants.push_back(maximum);
    unsigned maxDifference = 0;
    std::uint64_t differingChannels = 0, comparedChannels = 0;
    for (const auto& fixture : fixtures) {
        optimized.Upload(fixture); reference.Upload(fixture);
        for (const auto& s : variants) {
            optimized.Process(s); reference.Process(s);
            auto a = optimized.Readback(), b = reference.Readback();
            for (size_t i = 0; i < a.pixels.size(); ++i) {
                unsigned delta = static_cast<unsigned>(std::abs(static_cast<int>(a.pixels[i]) - b.pixels[i]));
                if ((i & 3) == 3) Require(delta == 0, "Reference alpha mismatch");
                else { maxDifference = std::max(maxDifference, delta); differingChannels += delta != 0; ++comparedChannels; }
            }
        }
    }
    log << "REFERENCE: " << fixtures.size() * variants.size() << " image/settings cases; max RGB deviation "
        << maxDifference << "/255; differing RGB channels " << differingChannels << '/' << comparedChannels << '\n';
    Require(maxDifference <= 1, "Shader optimization exceeds one 8-bit rounding step");
    log << "PASS: shader comparison within one 8-bit rounding step, exact alpha\n";
}
void CheckPhotoRadius(Renderer& renderer, const std::filesystem::path& destination, std::ostream& log) {
    // A gently varying edge models spatial softness without noise or hard ringing.
    Image soft{129, 33, std::vector<BYTE>(129 * 33 * 4)};
    for (unsigned y = 0; y < soft.height; ++y) for (unsigned x = 0; x < soft.width; ++x) {
        const int t = static_cast<int>(x) - 64;
        const int value = t < -6 ? 70 : t > 6 ? 180 : 125 + t * 8 + t * t / 3;
        const auto index = (static_cast<size_t>(y) * soft.width + x) * 4;
        soft.pixels[index] = soft.pixels[index + 1] = soft.pixels[index + 2] = static_cast<BYTE>(value);
        soft.pixels[index + 3] = static_cast<BYTE>((x + y) % 256);
    }
    renderer.Upload(soft);
    Settings s; s.noise = 0;
    renderer.Process(s); const auto narrow = renderer.Readback();
    for (unsigned radius : {2u, 3u}) {
        s.radius = radius; renderer.Process(s); const auto wide = renderer.Readback(); CheckBounds(soft, wide, radius);
        Require(wide.pixels != narrow.pixels, "Photo radius has no effect on soft edges");
        s.strength = s.detail = 0; renderer.Process(s);
        Require(renderer.Readback().pixels == soft.pixels, "Photo zero gain changes input");
        s = Settings{}; s.noise = 0;
    }
    for (const auto& fixture : {Image{1,1,{53,117,231,127}}, MakeTestImage(1,17), MakeTestImage(17,1)}) {
        renderer.Upload(fixture); s.radius = 3; renderer.Process(s); CheckBounds(fixture, renderer.Readback(), 3);
    }
    SavePng(destination / "photo-soft-fixture.png", soft);
    log << "PASS: photo radii 2/3 affect soft edges, preserve alpha/bounds and zero gain, clamp 1D/1px borders\n";
}
void CheckGamePaths(const std::filesystem::path& destination, std::ostream& log) {
    Require(IsGamePath(L"C:/Steam/common/Game/bin/Game.exe", L"c:/steam/common/game"), "Case-insensitive game path matching failed");
    Require(!IsGamePath(L"C:/Steam/common/GameOther/Game.exe", L"C:/Steam/common/Game"), "Game prefix collision");
    Require(!IsGamePath(L"C:/Steam/common/Game/../Other/Game.exe", L"C:/Steam/common/Game"), "Traversal matched game path");
    Require(!IsGamePath(L"Game.exe", L"C:/Steam/common/Game"), "Relative executable path matched");
    const auto root = std::filesystem::absolute(destination / "steam-fixture");
    std::filesystem::create_directories(root / L"steamapps/common/Test Game");
    std::ofstream(root / L"steamapps/appmanifest_100.acf") << "\"AppState\" { \"appid\" \"100\" \"name\" \"Test Game\" \"installdir\" \"Test Game\" }";
    std::ofstream(root / L"steamapps/appmanifest_000.acf") << "\"AppState\" { \"name\" \"Invalid\" \"installdir\" \"../../outside\" }";
    const auto games = DiscoverSteamGames(root);
    Require(games.size() == 1 && games[0].name == L"Test Game", "Steam manifest discovery failed");
    const auto status = DetectGames(games);
    Require(status.running.empty() && status.activeGame.empty(), "Installed fixture falsely reported running");
    log << "PASS: Steam manifest discovery, case-insensitive path matching, prefix/traversal rejection, no false live state\n";
}
void CheckInstaller(const std::filesystem::path& destination, std::ostream& log) {
    std::array<wchar_t,32768> path{}; GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
    const std::filesystem::path executable(path.data()), dll=executable.parent_path()/L"native/dxgi.dll";
    Require(std::filesystem::exists(dll),"Native DLL missing from application package");
    const auto folder=std::filesystem::absolute(destination/L"native-install-fixture"); std::filesystem::create_directories(folder);
    const auto game=folder/L"TestGame.exe", target=folder/L"dxgi.dll";
    std::filesystem::copy_file(executable,game,std::filesystem::copy_options::overwrite_existing);
    { std::ofstream unknown(target); unknown<<"unrelated existing graphics wrapper"; }
    bool refused=false; try { InstallNative(dll,game); } catch(...) { refused=true; }
    Require(refused && std::filesystem::file_size(target)==35,"Installer overwrote an unrelated wrapper");
    std::filesystem::remove(target); InstallNative(dll,game);
    Require(std::filesystem::file_size(target)==std::filesystem::file_size(dll),"Native install failed");
    InstallNative(dll,game); // Reinstall must be idempotent.
    { std::ofstream modified(target,std::ios::app); modified<<"changed"; }
    refused=false; try { RemoveNative(game); } catch(...) { refused=true; }
    Require(refused && std::filesystem::exists(target),"Uninstall deleted a modified DLL");
    std::filesystem::copy_file(dll,target,std::filesystem::copy_options::overwrite_existing); RemoveNative(game);
    Require(!std::filesystem::exists(target) && !std::filesystem::exists(folder/L"DLSSClearNative.install.txt"),"Owned uninstall incomplete");
    log<<"PASS: native install refuses foreign DLLs, is idempotent and removes only hash-verified owned files\n";
}
}
int RunValidation(const std::filesystem::path& shader, const std::filesystem::path& destination, bool warp,
    const std::filesystem::path& referenceShader) {
    std::filesystem::create_directories(destination);
    std::ofstream log(destination / "validation.txt");
    try {
        Renderer renderer; renderer.Initialize(nullptr, shader, warp);
        std::unique_ptr<Renderer> reference;
        if (!referenceShader.empty()) {
            reference = std::make_unique<Renderer>(); reference->Initialize(nullptr, referenceShader, warp);
            Require(renderer.Adapter() == reference->Adapter(), "Reference and optimized adapters differ");
        }
        log << "DLSSClear validation\nAdapter: " << std::filesystem::path(renderer.Adapter()).string()
            << "\nBackend: " << (renderer.Software() ? "WARP software" : "hardware")
            << "\nD3D11 debug layer: " << (renderer.DebugLayer() ? "enabled" : "unavailable or release build") << "\n";
        Settings settings;
        auto input = MakeTestImage(257, 129); // Odd sizes exercise row pitch and borders.
        for (size_t i = 3; i < input.pixels.size(); i += 4) input.pixels[i] = static_cast<BYTE>((i * 17) % 256);
        SavePng(destination / "roundtrip.png", input);
        Require(LoadImageFile(destination / "roundtrip.png").pixels == input.pixels, "PNG round-trip mismatch");
        log << "PASS: PNG encode/decode exact RGBA round-trip\n";
        renderer.Upload(input); settings.strength = settings.detail = 0;
        renderer.Process(settings);
        Require(renderer.Readback().pixels == input.pixels, "Zero-strength pass modified input");
        log << "PASS: zero strength + detail is byte-exact, including transparent pixels\n";
        settings = Settings{}; renderer.Process(settings);
        auto output = renderer.Readback(); CheckBounds(input, output);
        Require(output.pixels != input.pixels, "Enhancement has no effect");
        log << "PASS: default effect changes texture, preserves alpha and 3x3 RGB bounds\n";
        settings.mode = 3;
        renderer.Process(settings);
        Require(renderer.Readback().pixels == output.pixels, "Difference display mode changed export pixels");
        log << "PASS: difference view does not affect the exported image\n";
        std::uint64_t previousDelta = 0;
        for (const auto& profile : kFilterPresets) {
            settings = Settings{};
            settings.strength = profile.strength; settings.detail = profile.detail; settings.noise = profile.noise; settings.radius = profile.radius;
            renderer.Process(settings);
            const auto profileOutput = renderer.Readback(); CheckBounds(input, profileOutput, settings.radius);
            std::uint64_t delta = 0;
            for (size_t i = 0; i < input.pixels.size(); ++i)
                delta += static_cast<unsigned>(std::abs(static_cast<int>(profileOutput.pixels[i]) - input.pixels[i]));
            if (profile.radius == 1) {
                Require(delta > previousDelta, "Game preset intensity is not increasing on the test chart");
                previousDelta = delta;
            } else Require(delta > 0, "Photo preset has no effect");
        }
        log << "PASS: all five presets preserve bounds/alpha; three game profiles progressively increase test-chart contrast\n";
        CheckPhotoRadius(renderer, destination, log);
        CheckGamePaths(destination, log);
        CheckInstaller(destination, log);
        renderer.Upload(input);
        settings = Settings{};
        settings.strength = 2.f; settings.detail = 1.f; settings.noise = 0;
        renderer.Process(settings); CheckBounds(input, renderer.Readback());
        log << "PASS: maximum gain anti-ringing bounds, odd dimensions and clamped borders\n";
        Image flat{31, 19, std::vector<BYTE>(31 * 19 * 4)};
        for (size_t i = 0; i < flat.pixels.size(); i += 4) {
            flat.pixels[i] = 64; flat.pixels[i + 1] = 137; flat.pixels[i + 2] = 203; flat.pixels[i + 3] = 79;
        }
        renderer.Upload(flat); renderer.Process(settings);
        Require(renderer.Readback().pixels == flat.pixels, "Flat color changed");
        log << "PASS: flat-field invariance and texture replacement\n";
        Image single{1, 1, {53, 117, 231, 127}};
        renderer.Upload(single); renderer.Process(settings);
        Require(renderer.Readback().pixels == single.pixels, "Single pixel changed");
        log << "PASS: 1x1 boundary case\n";
        bool rejected = false;
        try { LoadImageFile(destination / "does-not-exist.png"); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "Missing image was not rejected");
        log << "PASS: missing file reports an error\n";
        CheckCache(renderer, input, log);
        if (reference) ComparePixels(renderer, *reference, log);
        settings = Settings{};
        auto demo = MakeTestImage(); renderer.Upload(demo); renderer.Process(settings);
        auto enhanced = renderer.Readback();
        SavePng(destination / "demo-original.png", demo); SavePng(destination / "demo-enhanced.png", enhanced);
        Image split = demo;
        for (unsigned y = 0; y < demo.height; ++y) for (unsigned x = demo.width / 2; x < demo.width; ++x) {
            size_t i = (static_cast<size_t>(y) * demo.width + x) * 4;
            std::copy_n(enhanced.pixels.data() + i, 4, split.pixels.data() + i);
        }
        SavePng(destination / "demo-split.png", split);
        std::ofstream csv(destination / "benchmark.csv");
        csv << "width,height,backend,mean_pass_ms,min_batch_ms,max_batch_ms,batches,passes_per_batch\n";
        std::ofstream comparison;
        if (reference) { comparison.open(destination / "comparison.csv"); comparison << "width,height,batch,reference_ms,optimized_ms\n"; }
        const unsigned sizes[][2] = {{1920,1080},{2560,1440},{3840,2160}};
        for (const auto& size : sizes) {
            auto benchmarkImage = MakeTestImage(size[0], size[1]); renderer.Upload(benchmarkImage);
            if (reference) reference->Upload(benchmarkImage);
            double sum = 0, minimum = 1e9, maximum = 0, referenceSum = 0;
            constexpr unsigned batches = 7, repeats = 64;
            for (unsigned batch = 0; batch < batches; ++batch) {
                double ms = 0, referenceMs = 0;
                // Alternate order to reduce systematic clock/thermal ordering bias.
                if (reference && batch % 2 == 0) referenceMs = reference->Measure(settings, repeats);
                ms = renderer.Measure(settings, repeats);
                if (reference && batch % 2 != 0) referenceMs = reference->Measure(settings, repeats);
                if (reference) comparison << size[0] << ',' << size[1] << ',' << batch << ',' << std::fixed
                    << std::setprecision(6) << referenceMs << ',' << ms << '\n';
                referenceSum += referenceMs;
                sum += ms; minimum = std::min(minimum, ms); maximum = std::max(maximum, ms);
            }
            csv << size[0] << ',' << size[1] << ',' << (renderer.Software() ? "WARP" : "hardware") << ','
                << std::fixed << std::setprecision(6) << sum / batches << ',' << minimum << ',' << maximum << ',' << batches << ',' << repeats << '\n';
            log << "BENCHMARK " << size[0] << 'x' << size[1] << ": " << std::fixed << std::setprecision(4)
                << sum / batches << " ms/pass (7 warmed batches x 64 passes)\n";
            if (reference) log << "REFERENCE " << size[0] << 'x' << size[1] << ": " << referenceSum / batches << " ms/pass\n";
        }
        Require(renderer.DebugErrors() == 0, "D3D11 debug layer reported warnings/errors");
        if (reference) Require(reference->DebugErrors() == 0, "Reference D3D11 debug layer reported warnings/errors");
        log << "PASS: no D3D11 warnings/errors" << (renderer.DebugLayer() ? " (debug layer active)" : " (debug layer not active; not verified)")
            << "\nAll checks passed.\nTimings cover the enhancement pass only, not image I/O, display, Present, or a game.\n";
        return 0;
    } catch (const std::exception& error) {
        log << "FAIL: " << error.what() << '\n'; return 1;
    }
}

int RunLocalizationValidation(const std::filesystem::path& destination) {
    std::filesystem::create_directories(destination);
    std::ofstream log(destination / "localization-validation.txt");
    const auto settings = std::filesystem::absolute(destination / "test-settings.ini");
    try {
        std::set<std::wstring> keys;
        for(const auto& entry : i18n::kTranslations) {
            Require(*entry.tr && *entry.en && *entry.zh, "An interface translation is empty");
            Require(keys.insert(entry.tr).second, "Duplicate translation key");
        }
        Require(WritePrivateProfileStringW(L"Hotkeys",L"ToggleGame",L"119",settings.c_str())!=FALSE,"Cannot create isolated preference fixture");
        for(auto language : {i18n::Language::Turkish,i18n::Language::English,i18n::Language::SimplifiedChinese}) {
            i18n::SavePreference(language,settings);
            Require(i18n::ReadPreference(settings)==language,"Language preference did not round-trip");
            Require(GetPrivateProfileIntW(L"Hotkeys",L"ToggleGame",0,settings.c_str())==119,"Language save changed the toggle shortcut");
        }
        Require(wcscmp(i18n::Text(L"Uygulamayı kapat",i18n::Language::English),L"Exit application")==0,"English label unavailable");
        Require(wcscmp(i18n::Text(L"Uygulamayı kapat",i18n::Language::SimplifiedChinese),L"退出应用")==0,"Chinese label unavailable");
        Require(wcscmp(i18n::Text(L"Home",i18n::Language::SimplifiedChinese),L"Home")==0,"Unknown identifiers should be preserved");
        WritePrivateProfileStringW(L"UI",L"Language",L"unsupported",settings.c_str());
        Require(i18n::ReadPreference(settings)==i18n::DefaultLanguage(),"Invalid preference did not fall back safely");
        // Saving to a directory must fail without silently applying another language.
        const auto before=i18n::Current(); bool failed=false;
        try { i18n::SavePreference(i18n::Language::English,std::filesystem::absolute(destination)); } catch(...) { failed=true; }
        Require(failed && i18n::Current()==before,"Failed save changed the active language");
        log << "PASS: catalog complete and unique, Turkish/English/Chinese preference round-trip, hotkey preservation, invalid preference fallback, failed-save safety.\n";
        return 0;
    } catch(const std::exception& error) { log << "FAIL: " << error.what() << '\n'; return 1; }
}
