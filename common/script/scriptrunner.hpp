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

#ifndef SCRIPTRUNNER_HPP
#define SCRIPTRUNNER_HPP

#include "scriptuibridge.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>

class ScriptRunner {
public:
    struct Outcome {
        std::string scriptName;
        int exitValue = 0;
        std::string output;
        bool cancelled = false;
    };

    static ScriptRunner& get(void)
    {
        static ScriptRunner instance;
        return instance;
    }

    bool start(std::string scriptPath, std::string displayName, std::string titleIdHex);

    bool active(void) const { return mState.load() != State::Idle; }

    void requestCancel(void);

    bool cancelRequested(void) const;

    void shutdown(void);

    std::optional<Outcome> takeResult(void);

    ScriptUiBridge& bridge(void) { return mBridge; }

    const std::string& selectedTitle(void) const { return mTitleIdHex; }

    const std::string& scriptName(void) const { return mName; }

    static void runThread(void);

private:
    ScriptRunner(void) = default;

    void run(void);

    static constexpr size_t THREAD_STACK = 128 * 1024;

    enum class State { Idle, Running, Done };

    std::atomic<State> mState{State::Idle};
    std::mutex mMutex;
    std::condition_variable mDoneCv;
    Outcome mOutcome;
    std::string mPath, mName, mTitleIdHex;
    ScriptUiBridge mBridge;
};

#endif
