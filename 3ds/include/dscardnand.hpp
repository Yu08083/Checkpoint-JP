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

#ifndef DSCARDNAND_HPP
#define DSCARDNAND_HPP

#include "dscard.hpp"
#include "ntrcard.hpp"
#include "progress.hpp"
#include <3ds.h>
#include <3ds/types.h>
#include <string>

namespace DSCardNand {

    bool available(void);

    struct Session {
        bool ready              = false;
        bool requiresPowerCycle = false;
        NtrCard::NandSectorReadResult sentinel{};
    };

    Session prepare(FS_MediaType media, const DSCard::NandSave& nandSave, const u8* fsBanner);

    enum class DumpStatus {
        Ok,
        NotAvailable,
        NotReady,
        InvalidSaveSize,
        CartridgeDisturbed,
        FileError,
        OutOfMemory,
        Cancelled,
        ReadFailed
    };

    struct DumpOutcome {
        DumpStatus status = DumpStatus::NotAvailable;

        bool requiresPowerCycle = false;
        u32 windowsDone         = 0;
        u32 windows             = 0;
        u32 bytesWritten        = 0;

        char sha256[65] = {0};

        bool ok(void) const { return status == DumpStatus::Ok; }
    };

    enum class RestoreStatus {
        Ok,
        NotAvailable,
        NotReady,
        InvalidSaveSize,
        SourceSizeMismatch,
        OutOfMemory,
        FileError,
        Cancelled,
        ReadFailed,
        WriteFailed
    };

    struct RestoreOutcome {
        RestoreStatus status    = RestoreStatus::NotAvailable;
        bool requiresPowerCycle = false;

        bool modified   = false;
        u32 windowsDone = 0;
        u32 windows     = 0;

        u32 pagesSkipped = 0;
        u32 pagesWritten = 0;

        char sourceSha256[65] = {0};

        bool ok(void) const { return status == RestoreStatus::Ok; }
    };

    RestoreOutcome restoreFromFile(
        FS_MediaType media, const DSCard::NandSave& nandSave, const Session& session, const std::u16string& path, ProgressSink& sink);

    DumpOutcome dumpToFile(
        FS_MediaType media, const DSCard::NandSave& nandSave, const Session& session, const std::u16string& path, ProgressSink& sink);
}

#endif
