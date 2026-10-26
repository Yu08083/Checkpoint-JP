/*
 *   This file is part of Checkpoint
 *   Copyright (C) 2017-2026 Bernardo Giordano, FlagBrew
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#ifndef AUTOUPDATER_HPP
#define AUTOUPDATER_HPP

#include <cstddef>
#include <optional>
#include <string>

namespace AutoUpdater {
    enum class ArtifactKind {
        Executable,
        Cia,
    };

    struct Update {
        std::string version;
        std::string url;
        std::string target;
        size_t size = 0;
        ArtifactKind kind;
    };

    enum class Outcome {
        Installed,
        Failed,
    };

    struct Progress {
        size_t downloaded = 0;
        size_t total      = 0;
    };
    Progress progress();

    bool busy();

    void init();

    std::optional<Update> check(const std::string& executablePath);

    Outcome install(const Update& update);

    bool requestRelaunch(const std::string& executablePath);
}

#endif
