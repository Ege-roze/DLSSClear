#include "GameProfiles.h"
#include <bcrypt.h>
#include <shlobj.h>
#include <array>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
constexpr size_t kMaxRecordBytes = 256 * 1024;
std::string Identity(const std::filesystem::path& executable) {
    if (!executable.is_absolute()) throw std::runtime_error("Oyun dosyasinin tam yolu gerekli.");
    const auto path = executable.lexically_normal().wstring();
    const int count = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
        path.data(), static_cast<int>(path.size()), nullptr, 0, nullptr, nullptr, 0);
    if (!count) throw std::runtime_error("Oyun kimligi olusturulamadi.");
    std::wstring lower(static_cast<size_t>(count), L'\0');
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, path.data(), static_cast<int>(path.size()),
        lower.data(), count, nullptr, nullptr, 0)) throw std::runtime_error("Oyun kimligi olusturulamadi.");
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, lower.data(), count, nullptr, 0, nullptr, nullptr);
    if (!bytes) throw std::runtime_error("Oyun yolu okunamadi.");
    std::string text(static_cast<size_t>(bytes), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, lower.data(), count, text.data(), bytes, nullptr, nullptr))
        throw std::runtime_error("Oyun yolu okunamadi.");
    return text;
}
void Validate(const native::Options& options) {
    if (!SameGameOptions(options, native::Clamp(options)))
        throw std::runtime_error("Oyun kaydindaki ayarlar gecersiz. Canli ayarlar korundu.");
}
std::string Serialize(const std::string& identity, native::Options o) {
    std::ostringstream out;
    out << "DLSSClear.GameProfile 1\n" << std::quoted(identity) << '\n'
        << o.enabled << ' ' << o.strength << ' ' << o.detail << ' ' << o.noise << ' '
        << o.antiRinging << ' ' << o.split << '\n';
    return out.str();
}
}
bool SameGameOptions(const native::Options& a, const native::Options& b) {
    return a.enabled == b.enabled && a.strength == b.strength && a.detail == b.detail &&
        a.noise == b.noise && a.antiRinging == b.antiRinging && a.split == b.split;
}
GameProfiles::GameProfiles(std::filesystem::path directory) : directory_(std::move(directory)) {}
std::filesystem::path GameProfiles::Directory() const {
    if (!directory_.empty()) return directory_;
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &local)))
        throw std::runtime_error("Kullanici kayit klasoru bulunamadi.");
    const std::filesystem::path path(local);
    CoTaskMemFree(local);
    return path / L"DLSSClear" / L"profiles";
}
std::filesystem::path GameProfiles::FileFor(const std::filesystem::path& executable) const {
    auto identity = Identity(executable);
    std::array<UCHAR, 32> digest{};
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0, reinterpret_cast<PUCHAR>(identity.data()),
        static_cast<ULONG>(identity.size()), digest.data(), static_cast<ULONG>(digest.size())) < 0)
        throw std::runtime_error("Oyun kayit adi olusturulamadi.");
    std::ostringstream name;
    for (const auto value : digest) name << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(value);
    return Directory() / (name.str() + ".profile");
}
std::optional<native::Options> GameProfiles::Load(const std::filesystem::path& executable) const {
    const auto file = FileFor(executable);
    if (!std::filesystem::exists(file)) return std::nullopt;
    if (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) > kMaxRecordBytes)
        throw std::runtime_error("Oyun kaydi okunamadi. Canli ayarlar korundu.");
    std::ifstream in(file, std::ios::binary);
    std::string magic, identity;
    int version = 0;
    native::Options o;
    if (!(in >> magic >> version >> std::quoted(identity) >> o.enabled >> o.strength >> o.detail >> o.noise >> o.antiRinging >> o.split)
        || magic != "DLSSClear.GameProfile" || version != 1 || identity != Identity(executable))
        throw std::runtime_error("Oyun kaydi bozuk veya farkli bir surume ait. Canli ayarlar korundu.");
    in >> std::ws;
    if (!in.eof()) throw std::runtime_error("Oyun kaydinda beklenmeyen veri var. Canli ayarlar korundu.");
    Validate(o);
    return o;
}
void GameProfiles::Save(const std::filesystem::path& executable, native::Options options) const {
    Validate(options);
    const auto target = FileFor(executable);
    const auto content = Serialize(Identity(executable), options);
    std::filesystem::create_directories(target.parent_path());
    static std::atomic<unsigned long> serial = 0;
    std::filesystem::path temp;
    HANDLE file = INVALID_HANDLE_VALUE;
    for (unsigned tries = 0; tries < 32; ++tries) {
        temp = target;
        temp += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++serial);
        file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) break;
    }
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Oyun kaydi yazilamadi. Onceki kayit korundu.");
    DWORD written = 0;
    const bool complete = WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr)
        && written == content.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!complete || !MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str()); // Only the temporary file we successfully created.
        throw std::runtime_error("Oyun kaydi tamamlanamadi. Onceki kayit korundu.");
    }
}
int RunGameProfileValidation(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    std::ofstream report(directory / "game-profile-tests.txt");
    if (!report) return 1;
    auto require = [](bool ok, const char* message) { if (!ok) throw std::runtime_error(message); };
    try {
        // A unique subdirectory avoids overwriting fixtures from earlier runs.
        const auto fixtures = directory / (L"fixtures-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        GameProfiles store(fixtures);
        const std::filesystem::path game = L"C:\\Oyunlar\\Türkçe örnek\\Game.exe";
        const std::filesystem::path other = L"C:\\Oyunlar\\Başka oyun\\Game.exe";
        const native::Options saved{0, 1150, 350, 8, 1, 1};
        require(!store.Load(game), "Missing record must return empty");
        store.Save(game, saved);
        auto loaded = GameProfiles(fixtures).Load(game);
        require(loaded && SameGameOptions(*loaded, saved), "All six options must survive a new store instance");
        require(store.Load(L"c:\\oyunlar\\türkçe örnek\\GAME.EXE").has_value(), "Case-insensitive Unicode identity failed");
        require(store.FileFor(game) == store.FileFor(L"C:\\Oyunlar\\Türkçe örnek\\sub\\..\\Game.exe"), "Normalized identity failed");
        store.Save(other, native::Options{});
        require(SameGameOptions(*store.Load(game), saved) && SameGameOptions(*store.Load(other), native::Options{}), "Game isolation failed");
        report << "PASS: missing record, Unicode path, all six options, restart, case/lexical normalization, different games\n";
        auto reject = [&](const std::string& content) {
            { std::ofstream bad(store.FileFor(game), std::ios::binary | std::ios::trunc); bad << content; }
            bool rejected = false;
            try { (void)store.Load(game); } catch (const std::exception&) { rejected = true; }
            require(rejected, "Malformed record accepted");
        };
        reject("DLSSClear.GameProfile 1\n");
        auto text = Serialize(Identity(game), saved);
        auto wrongVersion = text; wrongVersion.replace(wrongVersion.find(" 1\n"), 3, " 9\n"); reject(wrongVersion);
        reject(Serialize(Identity(other), saved));
        auto badOptions = saved; badOptions.strength = 9999; reject(Serialize(Identity(game), badOptions));
        badOptions = saved; badOptions.enabled = 2; reject(Serialize(Identity(game), badOptions));
        reject(text + "unexpected"); reject(std::string(kMaxRecordBytes + 1, 'x'));
        store.Save(game, saved);
        bool invalidRejected = false;
        try { store.Save(game, badOptions); } catch (const std::exception&) { invalidRejected = true; }
        require(invalidRejected && SameGameOptions(*store.Load(game), saved), "Invalid save replaced good record");
        report << "PASS: truncated/version/identity/range/boolean/trailing/oversized data rejected; invalid save preserves record\n";
        // Deny rename/delete to exercise a real failed atomic replacement.
        HANDLE lock = CreateFileW(store.FileFor(game).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        require(lock != INVALID_HANDLE_VALUE, "Could not lock record for failure test");
        bool failed = false;
        try { store.Save(game, native::Options{}); } catch (const std::exception&) { failed = true; }
        CloseHandle(lock);
        require(failed && SameGameOptions(*store.Load(game), saved), "Failed replacement did not preserve old record");
        for (const auto& entry : std::filesystem::directory_iterator(fixtures))
            require(entry.path().extension() == L".profile", "Abandoned temporary file");
        store.Save(game, native::Options{});
        require(SameGameOptions(*store.Load(game), native::Options{}), "Overwrite failed");
        report << "PASS: failed replacement preserves previous record, no temporary files, successful overwrite\nALL PASSED\n";
        return 0;
    } catch (const std::exception& error) { report << "FAIL: " << error.what() << '\n'; return 1; }
}
