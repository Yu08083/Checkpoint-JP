/*
 *   This file is part of Checkpoint
 *   Copyright (C) 2017-2026 Bernardo Giordano, FlagBrew
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

#ifndef BACKUPSIZE_HPP
#define BACKUPSIZE_HPP

#include "archive.hpp"
#include <3ds/types.h>
#include <atomic>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>

class BackupSizeCache {
public:
    static BackupSizeCache& get(void)
    {
        static BackupSizeCache instance;
        return instance;
    }

    void request(u64 id, BackupKind kind, const std::u16string& rootPath);

    std::optional<u64> total(u64 id, BackupKind kind);

    std::optional<u64> backupSize(const std::u16string& fullPath);

    void invalidate(u64 id, BackupKind kind, const std::u16string& rootPath);

    void invalidateAll(void);

    u32 generation(void) const { return mGeneration.load(); }

    void shutdown(void);
    static void shutdownStatic(void) { get().shutdown(); }

private:
    BackupSizeCache(void)                              = default;
    ~BackupSizeCache(void)                             = default;
    BackupSizeCache(const BackupSizeCache&)            = delete;
    BackupSizeCache& operator=(const BackupSizeCache&) = delete;

    static u64 key(u64 id, BackupKind kind) { return (id << 1) | (kind == BackupKind::Extdata ? 1u : 0u); }

    void compute(u64 cacheKey, std::u16string rootPath);

    u64 walk(const std::string& path);

    std::mutex mMutex;
    std::map<u64, u64> mTotals;
    std::map<std::u16string, u64> mBackupSizes;
    std::set<u64> mPending;
    std::atomic<bool> mAbort{false};
    std::atomic<u32> mGeneration{1};
};

#endif
