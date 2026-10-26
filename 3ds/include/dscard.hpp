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

#ifndef DSCARD_HPP
#define DSCARD_HPP

#include <3ds.h>
#include <cstddef>

namespace DSCard {

    inline constexpr size_t headerSize = 0x3B4;

    struct NandSave {
        bool present      = false;
        bool headerRead   = false;
        char gameCode[5]  = {0};
        u8 unitCode       = 0;
        u16 rawRomEnd     = 0;
        u16 rawRwStart    = 0;
        u32 cardControl13 = 0;
        u32 bannerOffset  = 0;
        u32 romEnd        = 0;
        u32 rwStart       = 0;
        u32 saveSize      = 0;
    };

    NandSave parseNandSave(const u8* header);

    NandSave probeNandSave(FS_MediaType media);

    void logNandSave(const NandSave& info);
}

#endif
