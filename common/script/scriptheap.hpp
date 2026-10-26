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

#ifndef SCRIPTHEAP_HPP
#define SCRIPTHEAP_HPP

#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

class ScriptHeap {
public:

    using Deleter = void (*)(void*);

    static ScriptHeap& get(void)
    {
        static ScriptHeap heap;
        return heap;
    }

    void* alloc(size_t size);
    void* allocZeroed(size_t count, size_t size);
    void* reallocate(void* ptr, size_t size);

    void* dupString(const std::string& str);

    void adopt(void* ptr, Deleter deleter);

    bool release(void* ptr);
    bool owns(void* ptr) const;

    void releaseAll(void);

    size_t liveBlocks(void) const;

private:
    ScriptHeap(void) = default;

    mutable std::mutex mMutex;
    std::unordered_map<void*, Deleter> mBlocks;
};

inline void* strToRet(const std::string& str)
{
    return ScriptHeap::get().dupString(str);
}

#endif
