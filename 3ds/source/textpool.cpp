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

#include "textpool.hpp"
#include "logging.hpp"
#include "util.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

    constexpr const char* MONO_FONT_PATH = "romfs:/fonts/SpaceMono.bcfnt";

    constexpr size_t GLYPH_CAPACITY = 8192;

    constexpr size_t MAX_ENTRIES = 512;

    constexpr u32 TICK_INTERVAL = 300;

    size_t utf8SeqLen(unsigned char b)
    {
        if ((b & 0x80) == 0x00) {
            return 1;
        }
        if ((b & 0xE0) == 0xC0) {
            return 2;
        }
        if ((b & 0xF0) == 0xE0) {
            return 3;
        }
        return 4;
    }
}

TextPool& TextPool::get(void)
{
    static TextPool instance;
    return instance;
}

TextPool::TextPool(void)
{
    mBuf = C2D_TextBufNew(GLYPH_CAPACITY);
}

TextPool::~TextPool(void)
{
    C2D_TextBufDelete(mBuf);
    if (mMono != nullptr) {
        C2D_FontFree(mMono);
    }
}

void TextPool::rebuild(void)
{
    C2D_TextBufClear(mBuf);
    mCache.clear();
    mMonoCache.clear();
}

void TextPool::frameTick(void)
{
    if (++mFrame % TICK_INTERVAL == 0 && mCache.size() + mMonoCache.size() > MAX_ENTRIES) {
        rebuild();
    }
}

const C2D_Text* TextPool::obtain(const std::string& s, C2D_Font font)
{
    auto& cache = font != nullptr ? mMonoCache : mCache;
    auto it     = cache.find(s);
    if (it != cache.end()) {
        return &it->second;
    }

    if (C2D_TextBufGetNumGlyphs(mBuf) + s.size() > GLYPH_CAPACITY) {
        rebuild();
    }
    C2D_Text t;
    C2D_TextFontParse(&t, font, mBuf, s.c_str());
    C2D_TextOptimize(&t);
    return &cache.emplace(s, t).first->second;
}

bool TextPool::monoReady(void)
{
    if (!mMonoTried) {
        mMonoTried = true;
        mMono      = C2D_FontLoad(MONO_FONT_PATH);
        if (mMono == nullptr) {
            Logging::error("Failed to load {}; the script log pane falls back to the system font.", MONO_FONT_PATH);
        }
        else {

            const C2D_Text* probe = obtain("0", mMono);
            C2D_TextGetDimensions(probe, 1.0f, 1.0f, &mMonoAdvance, &mMonoHeight);

            const FINF_s* finf = C2D_FontGetInfo(mMono);
            if (finf != nullptr && finf->lineFeed > 0 && mMonoHeight > 0.0f) {
                mMonoScale = (float)finf->lineFeed / mMonoHeight;
                mMonoAdvance *= mMonoScale;
                mMonoHeight = (float)finf->lineFeed;
            }

            C2D_FontSetFilter(mMono, GPU_NEAREST, GPU_NEAREST);
        }
    }
    return mMono != nullptr && mMonoAdvance > 0.0f;
}

float TextPool::monoAdvance(float scale)
{
    return monoReady() ? mMonoAdvance * scale : StringUtils::textWidth("0", scale);
}

float TextPool::monoLineHeight(float scale)
{
    return monoReady() ? mMonoHeight * scale : StringUtils::textHeight("0", scale);
}

void TextPool::drawMono(const std::string& s, float x, float y, float scale, u32 color, float depth)
{
    if (!monoReady()) {
        draw(s, x, y, scale, color, depth);
        return;
    }
    const C2D_Text* t      = obtain(s, mMono);
    const float atlasScale = scale * mMonoScale;
    C2D_DrawText(t, C2D_WithColor, floorf(x + 0.5f), floorf(y + 0.5f), depth, atlasScale, atlasScale, color);
}

float TextPool::draw(const std::string& s, float x, float y, float scale, u32 color, float depth)
{
    const C2D_Text* t = obtain(s, nullptr);

    C2D_DrawText(t, C2D_WithColor, floorf(x + 0.5f), floorf(y + 0.5f), depth, scale, scale, color);
    return StringUtils::textWidth(*t, scale);
}

void TextPool::drawCentered(const std::string& s, float x, float w, float y, float scale, u32 color, float depth)
{

    const C2D_Text* t = obtain(truncate(s, w, scale), nullptr);
    const float cx    = x + (w - StringUtils::textWidth(*t, scale)) / 2;
    C2D_DrawText(t, C2D_WithColor, floorf(cx + 0.5f), floorf(y + 0.5f), depth, scale, scale, color);
}

void TextPool::drawWrapped(const std::string& s, float x, float y, float scale, u32 color, float maxWidth, float depth, u32 alignFlags)
{
    const C2D_Text* t = obtain(s, nullptr);
    C2D_DrawText(t, C2D_WithColor | C2D_WordWrap | alignFlags, x, y, depth, scale, scale, color, maxWidth);
}

float TextPool::width(const std::string& s, float scale) const
{
    return StringUtils::textWidth(s, scale);
}

float TextPool::fitScale(const std::string& s, float maxWidth, float scale, float minScale) const
{
    const float w = StringUtils::textWidth(s, scale);
    if (w <= maxWidth || w <= 0.0f) {
        return scale;
    }
    return std::max(scale * maxWidth / w, minScale);
}

std::string TextPool::truncate(const std::string& s, float maxWidth, float scale) const
{
    if (StringUtils::textWidth(s, scale) <= maxWidth) {
        return s;
    }
    const float ellipsisWidth = StringUtils::textWidth("...", scale);

    std::vector<size_t> starts;
    for (size_t i = 0; i < s.size(); i += utf8SeqLen((unsigned char)s[i])) {
        starts.push_back(i);
    }

    size_t lo = 0, hi = starts.size();
    while (lo < hi) {
        const size_t mid = (lo + hi + 1) / 2;
        const size_t end = mid < starts.size() ? starts[mid] : s.size();
        if (StringUtils::textWidth(s.substr(0, end), scale) + ellipsisWidth <= maxWidth) {
            lo = mid;
        }
        else {
            hi = mid - 1;
        }
    }
    const size_t end = lo < starts.size() ? starts[lo] : s.size();
    return s.substr(0, end) + "...";
}
