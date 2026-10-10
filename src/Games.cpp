#include "Localization.h"
#include "Games.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#include <map>

namespace {
std::wstring Wide(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), 0);
    if (size) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
std::vector<std::string> Tokens(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {};
    file.seekg(0, std::ios::end); const auto size = file.tellg();
    if (size < 0 || size > 4 * 1024 * 1024) return {};
    file.seekg(0); std::string text(static_cast<size_t>(size), '\0');
    if (!file.read(text.data(), size)) return {};
    std::vector<std::string> tokens;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '"') continue;
        std::string token;
        while (++i < text.size() && text[i] != '"') {
            if (text[i] == '\\' && i + 1 < text.size() && (text[i + 1] == '\\' || text[i + 1] == '"')) ++i;
            token += text[i];
        }
        if (i == text.size()) return {}; // Partial write or malformed manifest.
        tokens.push_back(std::move(token));
    }
    return tokens;
}
std::string Value(const std::vector<std::string>& tokens, const std::string& key) {
    for (size_t i = 0; i + 1 < tokens.size(); ++i) if (tokens[i] == key) return tokens[i + 1];
    return {};
}
std::wstring Normalized(const std::filesystem::path& path) {
    auto result = path.lexically_normal().wstring();
    std::replace(result.begin(), result.end(), L'/', L'\\');
    while (result.size() > 3 && result.back() == L'\\') result.pop_back();
    return result;
}
BOOL CALLBACK CollectGameWindows(HWND window, LPARAM context) {
    // Exclude background helpers: a running game must own a visible main window.
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) || (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) return TRUE;
    DWORD processId = 0; GetWindowThreadProcessId(window, &processId);
    reinterpret_cast<std::set<DWORD>*>(context)->insert(processId);
    return TRUE;
}
}
bool IsGamePath(const std::filesystem::path& executable, const std::filesystem::path& directory) {
    if (!executable.is_absolute() || !directory.is_absolute()) return false;
    auto parent = Normalized(directory) + L'\\', candidate = Normalized(executable);
    return candidate.size() > parent.size() && CompareStringOrdinal(candidate.data(), static_cast<int>(parent.size()),
        parent.data(), static_cast<int>(parent.size()), TRUE) == CSTR_EQUAL;
}
std::vector<GameInstallation> DiscoverSteamGames(const std::filesystem::path& overrideRoot) {
    std::filesystem::path root = overrideRoot;
    if (root.empty()) {
        std::array<wchar_t, 32768> buffer{}; DWORD bytes = static_cast<DWORD>(sizeof(buffer));
        if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ,
            nullptr, buffer.data(), &bytes) != ERROR_SUCCESS) return {};
        root = buffer.data();
    }
    if (!root.is_absolute()) return {};
    std::set<std::filesystem::path> libraries{root};
    auto folders = Tokens(root / L"steamapps" / L"libraryfolders.vdf");
    for (size_t i = 0; i + 1 < folders.size(); ++i) if (folders[i] == "path") {
        std::filesystem::path path(Wide(folders[i + 1]));
        if (path.is_absolute()) libraries.insert(path);
    }
    std::vector<GameInstallation> games;
    for (const auto& library : libraries) {
        const auto steamapps = library / L"steamapps";
        std::error_code error;
        std::filesystem::directory_iterator it(steamapps, std::filesystem::directory_options::skip_permission_denied, error), end;
        for (; !error && it != end; it.increment(error)) {
            auto filename = it->path().filename().wstring();
            if (!filename.starts_with(L"appmanifest_") || it->path().extension() != L".acf") continue;
            auto manifest = Tokens(it->path());
            // Wallpaper Engine runs continuously from Steam but is not a game.
            if (Value(manifest, "appid") == "431960") continue;
            std::filesystem::path subdirectory(Wide(Value(manifest, "installdir")));
            const auto name = Wide(Value(manifest, "name"));
            if (name.empty() || subdirectory.empty() || subdirectory.is_absolute()) continue;
            const auto common = steamapps / L"common", directory = (common / subdirectory).lexically_normal();
            if (!IsGamePath(directory / L"placeholder.exe", common)) continue;
            std::error_code checkError;
            if (std::filesystem::is_directory(directory, checkError)) games.push_back({name, directory});
        }
    }
    return games;
}
GameStatus DetectGames(const std::vector<GameInstallation>& installations, const std::wstring& previousGame) {
    GameStatus status;
    std::set<DWORD> visibleProcesses;
    EnumWindows(CollectGameWindows, reinterpret_cast<LPARAM>(&visibleProcesses));
    DWORD foregroundProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
    // Read-only process discovery. Never attach, inject or install into detected games.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return status;
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    std::set<std::wstring> running;
    std::map<std::wstring, DWORD> processes;
    if (Process32FirstW(snapshot, &entry)) do {
        if (!visibleProcesses.contains(entry.th32ProcessID)) continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (!process) continue;
        std::array<wchar_t, 32768> path{}; DWORD count = static_cast<DWORD>(path.size());
        const bool found = QueryFullProcessImageNameW(process, 0, path.data(), &count) != FALSE;
        CloseHandle(process);
        if (!found) continue;
        for (const auto& game : installations) if (IsGamePath(path.data(), game.directory)) {
            running.insert(game.name);
            if (!processes.contains(game.name) || entry.th32ProcessID == foregroundProcess) processes[game.name] = entry.th32ProcessID;
            if (entry.th32ProcessID == foregroundProcess) status.activeGame = game.name;
            break;
        }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    status.running.assign(running.begin(), running.end());
    // Keep the selected game when the user Alt-Tabs to DLSSClear, until it closes.
    if (status.activeGame.empty() && running.contains(previousGame)) status.activeGame = previousGame;
    if (status.activeGame.empty() && status.running.size() == 1) status.activeGame = status.running.front();
    if (!status.activeGame.empty()) status.activePid = processes[status.activeGame];
    return status;
}
std::wstring DescribeGames(const GameStatus& status) {
    if (!status.activeGame.empty()) return status.activeGame;
    if (!status.running.empty()) return i18n::Text(L"Oyuna geçince adı burada görünür.");
    return i18n::Text(L"Oyun algılanmadı.");
}
