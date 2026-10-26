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

#include "scriptrunner.hpp"
#include "logging.hpp"
#include "scriptconsole.hpp"
#include "scriptengine.hpp"
#include "scriptheap.hpp"
#include "thread.hpp"

extern "C" {
#include "checkpoint_api.h"
}

bool ScriptRunner::start(std::string scriptPath, std::string displayName, std::string titleIdHex)
{
    if (mState.load() != State::Idle) {
        return false;
    }

    mPath       = std::move(scriptPath);
    mName       = std::move(displayName);
    mTitleIdHex = std::move(titleIdHex);
    mBridge.reset();
    ScriptConsole::get().reset();
    ckpt_script_abort_reset();

    Logging::info("[script] starting '{}' ({}), title {}", mName, mPath, mTitleIdHex.empty() ? "<none>" : mTitleIdHex);

    mState.store(State::Running);
    if (!Threads::create(std::optional<size_t>(THREAD_STACK), ScriptRunner::runThread)) {
        mState.store(State::Idle);
        Logging::error("[script] worker thread creation failed for '{}'", mName);
        return false;
    }
    return true;
}

void ScriptRunner::runThread(void)
{
    get().run();
}

void ScriptRunner::run(void)
{

    ckpt_script_lower_priority();

    ScriptEngine::Outcome out = ScriptEngine::run(mPath, {mTitleIdHex});

    ckpt_sav_close_all();
    ScriptHeap::get().releaseAll();
    {
        std::lock_guard<std::mutex> lock(mMutex);

        const bool cancelled = ckpt_script_abort_requested() != 0 && out.exitValue != 0;
        mOutcome             = Outcome{mName, out.exitValue, std::move(out.output), cancelled};

        mState.store(State::Done);
    }
    mDoneCv.notify_all();
}

void ScriptRunner::requestCancel(void)
{
    if (mState.load() != State::Running) {
        return;
    }

    ckpt_script_abort_request();
    mBridge.cancelAll();
}

void ScriptRunner::shutdown(void)
{
    if (mState.load() == State::Idle) {
        return;
    }

    Logging::trace("[script] shutdown: aborting '{}' and reaping the worker", mName);

    requestCancel();

    std::unique_lock<std::mutex> lock(mMutex);
    mDoneCv.wait(lock, [this] { return mState.load() != State::Running; });
    Logging::trace("[script] shutdown: worker finished");
}

bool ScriptRunner::cancelRequested(void) const
{
    return ckpt_script_abort_requested() != 0;
}

std::optional<ScriptRunner::Outcome> ScriptRunner::takeResult(void)
{
    if (mState.load() != State::Done) {
        return std::nullopt;
    }

    Outcome outcome;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        outcome = mOutcome;
    }
    mState.store(State::Idle);
    return outcome;
}
