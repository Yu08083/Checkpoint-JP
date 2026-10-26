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

#ifndef MODALCHROME_HPP
#define MODALCHROME_HPP

#include "colors.hpp"
#include "gui.hpp"
#include <citro2d.h>
#include <string>

namespace ModalChrome {

    constexpr int CARD_X = 16, CARD_W = 288;

    constexpr int TEXT_MAX_W = 256;

    constexpr int BTN_H      = 32;
    constexpr int BTN_WIDE_X = 28, BTN_WIDE_W = 264;
    constexpr int BTN_LEFT_X = 28, BTN_RIGHT_X = 164, BTN_HALF_W = 128;

    constexpr int LIST_X = 24, LIST_Y = 14, LIST_W = 352, LIST_H = 212;

    constexpr int PAD = 12, HEADER_H = 20, TEXT_GAP = 10;
    constexpr int CARD_CENTER_Y = 120, SCREEN_MARGIN = 8;
    constexpr int CARD_MIN_H = 132, CARD_MAX_H = 2 * (CARD_CENTER_Y - SCREEN_MARGIN);

    struct Layout {
        int cardY, cardH;
        int headerY;
        int textY;
        int btnY;
    };

    Layout fitText(std::string& text, float textSize, bool hasHeader);

    void dimTop(float z = 0.5f);
    void dimBottom(float z = 0.5f);

    void drawCard(const Layout& layout, u32 outlineColor);

    void drawListCard(void);
}

#endif
