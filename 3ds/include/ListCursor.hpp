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

#ifndef LISTCURSOR_HPP
#define LISTCURSOR_HPP

#include <3ds.h>
#include <cstddef>

class ListCursor {
public:

    static constexpr u64 REPEAT_DELAY_TICKS = 50000000;

    explicit ListCursor(size_t visibleRows) : mVisibleRows(visibleRows) {}

    void setCount(size_t count)
    {
        mCount = count;
        clamp();
    }

    void reset(void)
    {
        mIndex  = 0;
        mOffset = 0;
    }

    void setIndex(size_t i)
    {
        mIndex = i;
        clamp();
    }

    size_t index(void) const { return mIndex; }
    size_t offset(void) const { return mOffset; }

    void update(u32 kDown, u32 kHeld, u64 nowTick)
    {
        if (mCount == 0) {
            return;
        }

        auto moveDown = [&]() { mIndex = (mIndex + 1 < mCount) ? mIndex + 1 : 0; };
        auto moveUp   = [&]() { mIndex = (mIndex > 0) ? mIndex - 1 : mCount - 1; };

        if (kDown & KEY_DOWN) {
            moveDown();
            mLastTick = nowTick;
        }
        else if (kDown & KEY_UP) {
            moveUp();
            mLastTick = nowTick;
        }
        else if ((kHeld & KEY_DOWN) && nowTick > mLastTick + REPEAT_DELAY_TICKS) {
            moveDown();
            mLastTick = nowTick;
        }
        else if ((kHeld & KEY_UP) && nowTick > mLastTick + REPEAT_DELAY_TICKS) {
            moveUp();
            mLastTick = nowTick;
        }

        clamp();
    }

private:

    void clamp(void)
    {
        if (mCount == 0) {
            mIndex  = 0;
            mOffset = 0;
            return;
        }
        if (mIndex >= mCount) {
            mIndex = mCount - 1;
        }
        if (mIndex < mOffset) {
            mOffset = mIndex;
        }
        else if (mIndex >= mOffset + mVisibleRows) {
            mOffset = mIndex - mVisibleRows + 1;
        }
        const size_t maxOffset = mCount > mVisibleRows ? mCount - mVisibleRows : 0;
        if (mOffset > maxOffset) {
            mOffset = maxOffset;
        }
    }

    size_t mVisibleRows;
    size_t mCount  = 0;
    size_t mIndex  = 0;
    size_t mOffset = 0;
    u64 mLastTick  = 0;
};

#endif
