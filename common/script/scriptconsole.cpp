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

#include "scriptconsole.hpp"
#include "scriptconsole_c.h"
#include <algorithm>

namespace {

    constexpr size_t TAB_WIDTH = 4;

    constexpr size_t MIN_WIDTH = 16, MAX_WIDTH = 200;
}

ScriptConsole& ScriptConsole::get(void)
{
    static ScriptConsole instance;
    return instance;
}

void ScriptConsole::setWidth(size_t columns)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mWidth = std::clamp(columns, MIN_WIDTH, MAX_WIDTH);
}

void ScriptConsole::reset(void)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mLines.clear();
    mOpen = false;
    mGen++;
    for (auto& layer : mLayers) {
        layer = Layer{};
    }
    mIo = Layer{};
    mIoNote.clear();
    mIoRateDone = 0;
    mLayerSlots = 0;
    mIoSlot     = false;
}

void ScriptConsole::idle(Layer& layer)
{
    layer.active = false;
    layer.rate   = 0.0;
}

void ScriptConsole::idleIo(void)
{
    mIo       = Layer{};
    mIo.label = mIoNote;
}

void ScriptConsole::appendChar(char c)
{
    if (!mOpen || mLines.empty() || mLines.back().size() >= mWidth) {
        mLines.push_back(std::string());
        mLines.back().reserve(mWidth);
        mOpen = true;
        while (mLines.size() > MAX_LINES) {
            mLines.pop_front();
        }
    }
    mLines.back().push_back(c);
}

void ScriptConsole::write(const char* data, size_t len)
{
    if (data == nullptr || len == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    for (size_t i = 0; i < len; i++) {
        const char c = data[i];
        if (c == '\n') {

            if (!mOpen) {
                appendChar(' ');
                mLines.back().clear();
            }
            mOpen = false;
        }
        else if (c == '\t') {
            const size_t col = (mOpen && !mLines.empty()) ? mLines.back().size() : 0;
            for (size_t pad = TAB_WIDTH - (col % TAB_WIDTH); pad > 0; pad--) {
                appendChar(' ');
            }
        }
        else if (c == '\r' || (unsigned char)c < 0x20 || c == 0x7F) {
            continue;
        }
        else {
            appendChar(c);
        }
    }
    mGen++;
}

void ScriptConsole::log(const std::string& text)
{
    write(text.c_str(), text.size());
    write("\n", 1);
}

void ScriptConsole::beginLayer(size_t layer, std::string label, long long total)
{
    if (layer >= MAX_LAYERS) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    mLayers[layer]        = Layer{};
    mLayers[layer].label  = std::move(label);
    mLayers[layer].total  = total;
    mLayers[layer].active = true;
    mLayerSlots           = std::max(mLayerSlots, layer + 1);

    for (size_t deeper = layer + 1; deeper < MAX_LAYERS; deeper++) {
        mLayers[deeper] = Layer{};
    }
    idleIo();
}

void ScriptConsole::setLayer(size_t layer, long long done)
{
    if (layer >= MAX_LAYERS) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    if (mLayers[layer].active) {
        mLayers[layer].done = done;
    }
}

void ScriptConsole::setLayerLabel(size_t layer, std::string label)
{
    if (layer >= MAX_LAYERS) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    mLayers[layer].label = std::move(label);
}

void ScriptConsole::endLayer(size_t layer)
{
    if (layer >= MAX_LAYERS) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    for (size_t i = layer; i < MAX_LAYERS; i++) {
        idle(mLayers[i]);
    }
    idleIo();
}

void ScriptConsole::clearProgress(void)
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& layer : mLayers) {
        layer = Layer{};
    }
    mIo = Layer{};
    mIoNote.clear();

    mLayerSlots = 0;
    mIoSlot     = false;
}

void ScriptConsole::setIoNote(std::string note)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mIoNote = std::move(note);

    mIoSlot = true;
    if (!mIo.active) {
        idleIo();
    }
}

void ScriptConsole::beginIo(std::string label, long long total)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mIo         = Layer{};
    mIo.label   = std::move(label);
    mIo.total   = total;
    mIo.active  = true;
    mIo.bytes   = true;
    mIoSlot     = true;
    mIoRateDone = 0;
    mIoRateAt   = std::chrono::steady_clock::now();
}

void ScriptConsole::sampleIoRate(void)
{

    const auto now    = std::chrono::steady_clock::now();
    const double secs = std::chrono::duration<double>(now - mIoRateAt).count();
    if (secs < 0.5) {
        return;
    }
    const double sample = (double)(mIo.done - mIoRateDone) / secs;

    mIo.rate    = mIo.rate > 0.0 ? mIo.rate * 0.6 + sample * 0.4 : sample;
    mIoRateAt   = now;
    mIoRateDone = mIo.done;
}

void ScriptConsole::setIo(long long done)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mIo.active) {
        mIo.done = done;
        sampleIoRate();
    }
}

void ScriptConsole::addIo(long long delta)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (mIo.active) {
        mIo.done += delta;
        sampleIoRate();
    }
}

void ScriptConsole::endIo(void)
{
    std::lock_guard<std::mutex> lock(mMutex);
    idleIo();
}

unsigned ScriptConsole::generation(void) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mGen;
}

size_t ScriptConsole::lineCount(void) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mLines.size();
}

void ScriptConsole::copyWindow(size_t first, size_t count, std::vector<std::string>& out) const
{
    out.clear();
    std::lock_guard<std::mutex> lock(mMutex);
    if (first >= mLines.size()) {
        return;
    }
    const size_t last = std::min(mLines.size(), first + count);
    out.reserve(last - first);
    for (size_t i = first; i < last; i++) {
        out.push_back(mLines[i]);
    }
}

ScriptConsole::ProgressSnapshot ScriptConsole::progress(void) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    ProgressSnapshot snap;
    for (size_t i = 0; i < MAX_LAYERS; i++) {
        snap.layers[i] = mLayers[i];
        if (mLayers[i].active) {
            snap.layerCount = i + 1;
        }
    }
    snap.io         = mIo;
    snap.layerSlots = mLayerSlots;
    snap.ioSlot     = mIoSlot;
    return snap;
}

std::string ScriptConsole::tail(size_t maxBytes) const
{
    std::lock_guard<std::mutex> lock(mMutex);

    size_t bytes = 0, first = mLines.size();
    while (first > 0) {
        const size_t cost = mLines[first - 1].size() + 1;
        if (bytes + cost > maxBytes) {
            break;
        }
        bytes += cost;
        first--;
    }

    std::string out;
    out.reserve(bytes);
    for (size_t i = first; i < mLines.size(); i++) {
        out += mLines[i];
        out += '\n';
    }
    return out;
}

extern "C" {

void ckpt_console_write(const char* data, int len)
{
    if (len > 0) {
        ScriptConsole::get().write(data, (size_t)len);
    }
}

static int consoleStreamWrite(void* cookie, const char* buf, _READ_WRITE_BUFSIZE_TYPE n)
{
    (void)cookie;
    ScriptConsole::get().write(buf, (size_t)n);
    return (int)n;
}

FILE* ckpt_console_stdout(void)
{

    static FILE* stream = nullptr;
    if (stream == nullptr) {
        stream = funopen(nullptr, nullptr, consoleStreamWrite, nullptr, nullptr);
        if (stream != nullptr) {
            setvbuf(stream, nullptr, _IONBF, 0);
        }
    }

    return stream != nullptr ? stream : stdout;
}
}
