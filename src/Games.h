#pragma once
#include <filesystem>
#include <string>
#include <vector>

struct GameInstallation {
    std::wstring name;
    std::filesystem::path directory;
};
struct GameStatus {
    std::vector<std::wstring> running;
    std::wstring activeGame;
    unsigned activePid = 0;
};
std::vector<GameInstallation> DiscoverSteamGames(const std::filesystem::path& overrideRoot = {});
GameStatus DetectGames(const std::vector<GameInstallation>& installations, const std::wstring& previousGame = {});
bool IsGamePath(const std::filesystem::path& executable, const std::filesystem::path& directory);
std::wstring DescribeGames(const GameStatus& status);
