#pragma once
#include "../native/Protocol.h"
#include <filesystem>
#include <optional>

bool SameGameOptions(const native::Options& a, const native::Options& b);

// File I/O is limited to connection changes and explicit Save/Load actions.
class GameProfiles {
public:
    explicit GameProfiles(std::filesystem::path directory = {});
    std::optional<native::Options> Load(const std::filesystem::path& executable) const;
    void Save(const std::filesystem::path& executable, native::Options options) const;
    std::filesystem::path FileFor(const std::filesystem::path& executable) const;
private:
    std::filesystem::path Directory() const;
    std::filesystem::path directory_;
};

// Uses a caller-provided, isolated directory; never reads normal user profiles.
int RunGameProfileValidation(const std::filesystem::path& directory);
