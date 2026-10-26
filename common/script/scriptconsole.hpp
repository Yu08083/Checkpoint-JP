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

#ifndef SCRIPTCONSOLE_HPP
#define SCRIPTCONSOLE_HPP

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class ScriptConsole {
public:

    static constexpr size_t MAX_LAYERS = 3;

    struct Layer {
        std::string label;
        long long done  = 0;
        long long total = 0;
        bool active     = false;

        bool bytes = false;

        double rate = 0.0;
    };

    struct ProgressSnapshot {
        Layer layers[MAX_LAYERS];

        Layer io;

        size_t layerCount = 0;

        size_t layerSlots = 0;
        bool ioSlot       = false;
    };

    static ScriptConsole& get(void);

    void setWidth(size_t columns);

    void reset(void);

    void write(const char* data, size_t len);

    void log(const std::string& text);

    void beginLayer(size_t layer, std::string label, long long total);
    void setLayer(size_t layer, long long done);
    void setLayerLabel(size_t layer, std::string label);

    void endLayer(size_t layer);
    void clearProgress(void);

    void setIoNote(std::string note);

    void beginIo(std::string label, long long total);
    void setIo(long long done);
    void addIo(long long delta);
    void endIo(void);

    unsigned generation(void) const;

    size_t lineCount(void) const;

    void copyWindow(size_t first, size_t count, std::vector<std::string>& out) const;

    ProgressSnapshot progress(void) const;

    std::string tail(size_t maxBytes) const;

private:
    ScriptConsole(void) = default;

    void appendChar(char c);

    static constexpr size_t MAX_LINES = 250;

    static constexpr size_t DEFAULT_WIDTH = 56;

    mutable std::mutex mMutex;

    std::deque<std::string> mLines;

    bool mOpen    = false;
    size_t mWidth = DEFAULT_WIDTH;
    unsigned mGen = 0;
    Layer mLayers[MAX_LAYERS];
    Layer mIo;

    std::string mIoNote;

    std::chrono::steady_clock::time_point mIoRateAt;
    long long mIoRateDone = 0;

    size_t mLayerSlots = 0;
    bool mIoSlot       = false;

    static void idle(Layer& layer);

    void idleIo(void);

    void sampleIoRate(void);
};

#endif
