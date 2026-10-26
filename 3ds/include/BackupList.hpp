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

#ifndef BACKUPLIST_HPP
#define BACKUPLIST_HPP

#include "ListCursor.hpp"
#include <citro2d.h>
#include <string>
#include <vector>

class BackupList {
public:
    BackupList(int x, int y, int w, int h, size_t visibleRows);

    enum class RowKind { Existing, New, Receive };

    struct Row {
        std::string name;
        std::string meta;
        RowKind kind = RowKind::Existing;
    };

    void clear(void) { mRows.clear(); }
    void push_back(const std::string& name, const std::string& meta, RowKind kind) { mRows.push_back({name, meta, kind}); }

    size_t size(void) const { return mRows.size(); }
    size_t index(void) const { return mCursor.index(); }
    std::string name(size_t i) const { return i < mRows.size() ? mRows[i].name : std::string(); }

    void setIndex(size_t i);
    void resetIndex(void);

    void update(void);

    void draw(bool focused) const;

private:
    int mx, my, mw, mh;
    size_t mVisibleRows;
    int mRowH;
    ListCursor mCursor;
    std::vector<Row> mRows;
};

#endif
