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

#ifndef COLORS_HPP
#define COLORS_HPP

#include <citro2d.h>
#include <string>

inline u32 COLOR_OVERLAY      = C2D_Color32(0, 0, 0, 200);
inline u32 COLOR_WHITE        = C2D_Color32(255, 255, 255, 255);
inline u32 COLOR_BLACK        = C2D_Color32(0, 0, 0, 255);
inline u32 COLOR_BLACK_MEDIUM = C2D_Color32(43, 43, 46, 255);

inline u32 COLOR_BASE    = C2D_Color32(14, 16, 20, 255);
inline u32 COLOR_SURFACE = C2D_Color32(21, 24, 30, 255);
inline u32 COLOR_CARD    = C2D_Color32(19, 22, 28, 255);
inline u32 COLOR_RAISED  = C2D_Color32(26, 30, 38, 255);
inline u32 COLOR_LINE    = C2D_Color32(32, 36, 44, 255);

inline u32 COLOR_ACCENT     = C2D_Color32(122, 66, 196, 255);
inline u32 COLOR_RING       = C2D_Color32(154, 107, 255, 255);
inline u32 COLOR_TEAL       = C2D_Color32(143, 227, 218, 255);
inline u32 COLOR_BLUE       = C2D_Color32(111, 178, 232, 255);
inline u32 COLOR_GOLD       = C2D_Color32(230, 195, 77, 255);
inline u32 COLOR_DANGER     = C2D_Color32(229, 72, 77, 255);
inline u32 COLOR_ROW_SELECT = C2D_Color32(122, 66, 196, 60);

inline u32 COLOR_TEXT  = C2D_Color32(238, 241, 245, 255);
inline u32 COLOR_MUTED = C2D_Color32(154, 163, 177, 255);
inline u32 COLOR_FAINT = C2D_Color32(111, 119, 133, 255);
inline u32 COLOR_DIM   = C2D_Color32(0, 0, 0, 130);
inline u32 COLOR_SCRIM = C2D_Color32(0, 0, 0, 45);

namespace Colors {

    inline void apply(const std::string& theme)
    {
        if (theme == "light") {

            COLOR_BASE    = C2D_Color32(238, 239, 242, 255);
            COLOR_SURFACE = C2D_Color32(248, 249, 251, 255);
            COLOR_CARD    = C2D_Color32(255, 255, 255, 255);
            COLOR_RAISED  = C2D_Color32(230, 232, 236, 255);
            COLOR_LINE    = C2D_Color32(210, 213, 219, 255);

            COLOR_TEXT  = C2D_Color32(24, 27, 33, 255);
            COLOR_MUTED = C2D_Color32(88, 94, 104, 255);
            COLOR_FAINT = C2D_Color32(128, 134, 144, 255);

            COLOR_ACCENT = C2D_Color32(108, 54, 180, 255);
            COLOR_GOLD   = C2D_Color32(190, 150, 30, 255);
            COLOR_DANGER = C2D_Color32(206, 48, 54, 255);
            COLOR_BLUE   = C2D_Color32(43, 108, 176, 255);

            COLOR_BLACK_MEDIUM = C2D_Color32(212, 214, 219, 255);
            COLOR_OVERLAY      = C2D_Color32(0, 0, 0, 110);
        }
        else {
            COLOR_BASE         = C2D_Color32(14, 16, 20, 255);
            COLOR_SURFACE      = C2D_Color32(21, 24, 30, 255);
            COLOR_CARD         = C2D_Color32(19, 22, 28, 255);
            COLOR_RAISED       = C2D_Color32(26, 30, 38, 255);
            COLOR_LINE         = C2D_Color32(32, 36, 44, 255);
            COLOR_TEXT         = C2D_Color32(238, 241, 245, 255);
            COLOR_MUTED        = C2D_Color32(154, 163, 177, 255);
            COLOR_FAINT        = C2D_Color32(111, 119, 133, 255);
            COLOR_ACCENT       = C2D_Color32(122, 66, 196, 255);
            COLOR_GOLD         = C2D_Color32(230, 195, 77, 255);
            COLOR_DANGER       = C2D_Color32(229, 72, 77, 255);
            COLOR_BLUE         = C2D_Color32(111, 178, 232, 255);
            COLOR_BLACK_MEDIUM = C2D_Color32(43, 43, 46, 255);
            COLOR_OVERLAY      = C2D_Color32(0, 0, 0, 200);
        }
    }
}

#endif
