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

#include "titlequirks.hpp"

bool TitleQuirks::isSystemExcluded(u64 id)
{
    switch ((u32)id) {

        case 0x00008602:
        case 0x00009202:
        case 0x00009B02:
        case 0x0000A402:
        case 0x0000AC02:
        case 0x0000B402:

        case 0x00021A00:
            return true;
    }

    if ((id >> 32) == 0x0004000E) {
        return true;
    }

    if ((id >> 32) == 0x0004800F) {
        return true;
    }

    return false;
}

u32 TitleQuirks::extdataIdFor(u64 id)
{
    u32 low = (u32)id;
    switch (low) {
        case 0x00055E00:
            return 0x055D;
        case 0x0011C400:
            return 0x11C5;
        case 0x00175E00:
            return 0x1648;
        case 0x00179600:
        case 0x00179800:
            return 0x1794;
        case 0x00179700:
        case 0x0017A800:
            return 0x1795;
        case 0x0012DD00:
        case 0x0012DE00:
            return 0x12DC;
        case 0x001B5100:
            return 0x1B50;

    }

    return low >> 8;
}
