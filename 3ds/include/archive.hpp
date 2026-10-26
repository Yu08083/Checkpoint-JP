/*
 *   This file is part of Checkpoint
 *   Copyright (C) 2017-2025 Bernardo Giordano, FlagBrew
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

#ifndef ARCHIVE_HPP
#define ARCHIVE_HPP

#include "KeyboardManager.hpp"
#include "fsstream.hpp"
#include "util.hpp"
#include <3ds.h>
#include <utility>

enum class BackupKind { Save, Extdata };

class ArchiveHandle {
public:
    ArchiveHandle() = default;
    ~ArchiveHandle() { close(); }

    static ArchiveHandle fromFs(FS_Archive a)
    {
        ArchiveHandle h;
        h.mValid = true;
        h.mRaw   = false;
        h.mFs    = a;
        return h;
    }
    static ArchiveHandle fromPxi(FSPXI_Archive a)
    {
        ArchiveHandle h;
        h.mValid = true;
        h.mRaw   = true;
        h.mPxi   = a;
        return h;
    }

    ArchiveHandle(ArchiveHandle&& o) noexcept { *this = std::move(o); }
    ArchiveHandle& operator=(ArchiveHandle&& o) noexcept
    {
        if (this != &o) {
            close();
            mValid   = o.mValid;
            mRaw     = o.mRaw;
            mFs      = o.mFs;
            mPxi     = o.mPxi;
            o.mValid = false;
        }
        return *this;
    }
    ArchiveHandle(const ArchiveHandle&)            = delete;
    ArchiveHandle& operator=(const ArchiveHandle&) = delete;

    explicit operator bool(void) const { return mValid; }
    bool isRaw(void) const { return mRaw; }
    FS_Archive fs(void) const { return mFs; }
    FSPXI_Archive pxi(void) const { return mPxi; }
    void close(void);

private:
    bool mValid        = false;
    bool mRaw          = false;
    FS_Archive mFs     = 0;
    FSPXI_Archive mPxi = 0;
};

class SaveDataSource {
public:
    static SaveDataSource ctrSave(FS_MediaType media, u32 lowid, u32 highid);
    static SaveDataSource rawGba(FS_MediaType media, u32 lowid, u32 highid);
    static SaveDataSource extdata(u32 extdataId);
    static SaveDataSource twlSave(u32 lowid, u32 highid);

    ArchiveHandle open(Result& res) const;
    bool accessible(void) const;

private:
    enum class Kind { CtrSave, RawGbaSave, Extdata, TwlSave };
    SaveDataSource(Kind kind, FS_MediaType media, u32 a, u32 b) : mKind(kind), mMedia(media), mA(a), mB(b) {}

    Kind mKind;
    FS_MediaType mMedia;
    u32 mA;
    u32 mB;
};

namespace Archive {
    Result init(void);
    void exit(void);

    FS_Archive sdmc(void);

    Result save(FS_Archive* archive, FS_MediaType mediatype, u32 lowid, u32 highid);
    Result rawSave(FSPXI_Archive* archive, FS_MediaType mediatype, u32 lowid, u32 highid);
    Result extdata(FS_Archive* archive, u32 extdata);

    std::u16string twlSaveDataPath(u32 lowid, u32 highid);
}

#endif
