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

#ifndef PROGRESS_HPP
#define PROGRESS_HPP

#include <3ds.h>
#include <string>

struct ProgressSink {
    virtual ~ProgressSink() = default;

    virtual void begin(const std::string& mode, size_t totalFiles) = 0;

    virtual void startFile(const std::u16string& name, u32 size) = 0;

    virtual void advanceBytes(u32 offset) = 0;

    virtual void finishFile() = 0;

    virtual void end() = 0;

    virtual bool cancelled() const { return false; }
};

class UiProgressSink : public ProgressSink {
public:
    explicit UiProgressSink(bool cancellable = false) : mCancellable(cancellable) {}

    void begin(const std::string& mode, size_t totalFiles) override;
    void startFile(const std::u16string& name, u32 size) override;
    void advanceBytes(u32 offset) override;
    void finishFile() override;
    void end() override;
    bool cancelled() const override;

private:
    bool mCancellable;
};

struct RecordingProgressSink : public ProgressSink {
    std::string mode;
    size_t totalFiles = 0;
    size_t filesDone  = 0;
    u32 lastOffset    = 0;
    bool ended        = false;

    void begin(const std::string& m, size_t total) override
    {
        mode       = m;
        totalFiles = total;
    }
    void startFile(const std::u16string&, u32) override { lastOffset = 0; }
    void advanceBytes(u32 offset) override { lastOffset = offset; }
    void finishFile() override { filesDone++; }
    void end() override { ended = true; }
};

#endif
