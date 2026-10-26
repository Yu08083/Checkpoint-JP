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

#ifndef ICONSTORE_HPP
#define ICONSTORE_HPP

#include <3ds.h>
#include <citro2d.h>
#include <unordered_map>
#include <vector>

class IconStore {
public:
    IconStore() = default;
    ~IconStore();

    IconStore(const IconStore&)            = delete;
    IconStore& operator=(const IconStore&) = delete;
    IconStore(IconStore&& o) noexcept { mMap.swap(o.mMap); }
    IconStore& operator=(IconStore&& o) noexcept
    {
        if (this != &o) {
            clear();
            mMap.swap(o.mMap);
        }
        return *this;
    }

    void storeCtrIcon(u64 id, const u16* bigIconData);

    void storeDsIcon(u64 id, const u8* banner);

    void storeDsPixels(u64 id, const u16* pixels);

    C2D_Image get(u64 id);

    bool has(u64 id) const { return mMap.find(id) != mMap.end(); }

    bool copyCtrPixels(u64 id, u16* out) const;

    bool copyDsPixels(u64 id, u16* out) const;
    void clear();
    void swap(IconStore& o) noexcept { mMap.swap(o.mMap); }

    void mergeFrom(IconStore& o);

    void erase(u64 id);

private:
    struct Entry {
        std::vector<u16> pixels;
        bool isDs    = false;
        C3D_Tex* tex = nullptr;
    };

    void realize(Entry& e);

    std::unordered_map<u64, Entry> mMap;
};

#endif
