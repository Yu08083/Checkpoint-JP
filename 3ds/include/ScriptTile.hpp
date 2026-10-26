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

#ifndef SCRIPTTILE_HPP
#define SCRIPTTILE_HPP

#include "colors.hpp"
#include "scriptconsole.hpp"
#include <citro2d.h>
#include <string>

namespace ScriptTile {
    constexpr int MARGIN = 6;

    constexpr int LOG_X = MARGIN, LOG_Y = MARGIN;
    constexpr int LOG_W = 400 - 2 * MARGIN, LOG_H = 240 - 2 * MARGIN;

    constexpr int UI_X = MARGIN, UI_Y = MARGIN;
    constexpr int UI_W = 320 - 2 * MARGIN, UI_H = 240 - 2 * MARGIN;

    constexpr int PAD = 10, HEADER_H = 30, HINT_H = 22;

    constexpr int LOG_PAD = 6, LOG_HEADER_H = 22;

    constexpr float Z = 0.62f;

    void card(int x, int y, int w, int h, u32 outline, float z);

    int uiFrame(const std::string& title, const std::string& note = "", u32 outline = COLOR_LINE, float z = Z);

    void uiHints(const std::string& hints, float z = Z);

    constexpr int uiBodyBottom(void)
    {
        return UI_Y + UI_H - HINT_H;
    }

    constexpr int BAR_H = 6, BAR_ROW_H = 22;
    void bar(int x, int y, int w, const ScriptConsole::Layer& layer, float z = Z);

    constexpr size_t MAX_BARS = ScriptConsole::MAX_LAYERS + 1;
    size_t collectBars(const ScriptConsole::ProgressSnapshot& snapshot, ScriptConsole::Layer* out);
}

#endif
