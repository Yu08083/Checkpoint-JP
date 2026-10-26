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
 *         or requiring that modified versions of such material are marked in
 *         reasonable ways as different from the original version.
 */

#ifndef TEXTPOOL_HPP
#define TEXTPOOL_HPP

#include <citro2d.h>
#include <string>
#include <unordered_map>

class TextPool {
public:
    static TextPool& get(void);

    void frameTick(void);

    float draw(const std::string& s, float x, float y, float scale, u32 color, float depth = 0.5f);

    void drawCentered(const std::string& s, float x, float w, float y, float scale, u32 color, float depth = 0.5f);

    void drawWrapped(const std::string& s, float x, float y, float scale, u32 color, float maxWidth, float depth = 0.5f, u32 alignFlags = 0);

    float width(const std::string& s, float scale) const;

    float fitScale(const std::string& s, float maxWidth, float scale, float minScale) const;

    std::string truncate(const std::string& s, float maxWidth, float scale) const;

    bool monoReady(void);

    float monoAdvance(float scale);

    float monoLineHeight(float scale);

    void drawMono(const std::string& s, float x, float y, float scale, u32 color, float depth = 0.5f);

private:
    TextPool(void);
    ~TextPool(void);
    TextPool(const TextPool&)            = delete;
    TextPool& operator=(const TextPool&) = delete;

    const C2D_Text* obtain(const std::string& s, C2D_Font font);
    void rebuild(void);

    C2D_TextBuf mBuf;

    std::unordered_map<std::string, C2D_Text> mCache, mMonoCache;
    C2D_Font mMono     = nullptr;
    bool mMonoTried    = false;
    float mMonoAdvance = 0.0f;
    float mMonoHeight  = 0.0f;

    float mMonoScale = 1.0f;
    u32 mFrame       = 0;
};

#endif
