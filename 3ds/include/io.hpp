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

#ifndef IO_HPP
#define IO_HPP

#include "directory.hpp"
#include "fsstream.hpp"
#include "progress.hpp"
#include "spi.hpp"
#include "title.hpp"
#include "util.hpp"
#include <3ds.h>
#include <vector>

#define BUFFER_SIZE 0x50000

class BackupTarget;

namespace io {

    enum class BackupStage {
        OpenArchive,
        DeleteDst,
        CreateDst,
        Copy,
        ReadSpi,
        ReadFile,
        WriteFile,
        Commit,
        SecureValue,
        PathTooLong,
        CardNandSave,
        EmptyBackup
    };

    struct IoOutcome {
        bool ok;
        Result res;
        BackupStage stage;
        bool cancelled = false;

        std::vector<std::string> skippedInUse;
    };

    IoOutcome backup(const BackupTarget& target, const std::u16string& dstPath, ProgressSink& sink);

    IoOutcome restore(const BackupTarget& target, const std::u16string& srcPath, ProgressSink& sink);

    struct TreeEntry {
        std::u16string rel;
        bool folder;
    };

    Result collectTree(FS_Archive arch, const std::u16string& path, std::vector<TreeEntry>& out);

    inline constexpr size_t MAX_PATH_UNITS = 0x100;

    Result checkPathLengths(const std::vector<TreeEntry>& entries, const std::u16string& srcRoot, const std::u16string& dstRoot);

    size_t dropHostMetadata(std::vector<TreeEntry>& entries);

    std::vector<std::string> dropInUse(FS_Archive arch, const std::u16string& root, std::vector<TreeEntry>& entries);

    Result copyTree(FS_Archive srcArch, FS_Archive dstArch, const std::u16string& srcRoot, const std::u16string& dstRoot,
        const std::vector<TreeEntry>& entries, ProgressSink& sink);

    Result copyFile(
        FS_Archive srcArch, FS_Archive dstArch, const std::u16string& srcPath, const std::u16string& dstPath, ProgressSink& sink, u8* buffer);
    Result createDirectory(FS_Archive archive, const std::u16string& path);
    void deleteBackupFolder(const std::u16string& path);

    size_t countFilesRecursively(FS_Archive arch, const std::u16string& path);

    Result deleteFolderRecursively(FS_Archive arch, const std::u16string& path, ProgressSink* sink = nullptr);

    Result deleteFolderContentsRecursively(FS_Archive arch, const std::u16string& path, ProgressSink* sink = nullptr);
    bool directoryExists(FS_Archive archive, const std::u16string& path);
    bool fileExists(FS_Archive archive, const std::u16string& path);
    bool fileExists(const std::string& path);
}

#endif
