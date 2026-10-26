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

#ifndef SCRIPTLOGVIEW_HPP
#define SCRIPTLOGVIEW_HPP

#include <cstddef>
#include <string>
#include <vector>

class ScriptLogView {
public:
    static ScriptLogView& get(void);

    struct Window {

        size_t first = 0;
        size_t total = 0;

        bool scrollbar = false;

        float thumbSpan = 1.0f;

        float thumbTravel = 0.0f;
    };

    void reset(void);

    void setViewportRows(int rows);
    int viewportRows(void) const { return mViewport; }

    void scrollLines(int lines);

    void scrollPages(int pages);

    int scrollBack(void) const { return mScrollBack; }

    Window window(void);

    const std::vector<std::string>& rows(void) const { return mRows; }

private:
    ScriptLogView(void) = default;

    int mViewport   = 1;
    int mScrollBack = 0;

    std::vector<std::string> mRows;
    unsigned mSeenGeneration = 0;
    size_t mSeenFirst        = 0;
};

#endif
