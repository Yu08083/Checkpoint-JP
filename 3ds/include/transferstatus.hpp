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

#ifndef TRANSFERSTATUS_HPP
#define TRANSFERSTATUS_HPP

#include <3ds.h>
#include <string>

enum class TransferKind { Local, Network };

struct TransferSnapshot {
    bool active       = false;
    TransferKind kind = TransferKind::Local;
    std::string mode;

    std::u16string currentFile;
    u32 currentFileSize   = 0;
    u32 currentFileOffset = 0;
    size_t copyCount      = 0;
    size_t copyTotal      = 0;
    size_t saveCount      = 0;
    size_t saveTotal      = 0;

    u64 bytesDone  = 0;
    u64 bytesTotal = 0;
};

namespace TransferStatus {

    void beginLocalBatch(size_t totalSaves);
    void setSaveCount(size_t count);
    void beginLocalRun(const std::string& mode, size_t totalFiles);
    void startFile(const std::u16string& name, u32 size);
    void setFileOffset(u32 offset);
    void finishFile();

    void beginNetwork(const std::string& mode, u64 totalBytes);
    void setMode(const std::string& mode);
    void setBytes(u64 done, u64 total);
    void setBytesDone(u64 done);
    void addBytesDone(u64 delta);

    void end();

    void requestCancel();
    bool cancelRequested();

    TransferSnapshot snapshot();

    std::string bytesToMB(u64 done, u64 total);
}

#endif
