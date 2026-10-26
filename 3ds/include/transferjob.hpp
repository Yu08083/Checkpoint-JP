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

#ifndef TRANSFERJOB_HPP
#define TRANSFERJOB_HPP

#include "title.hpp"
#include "transfer.hpp"
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class TransferJob {
public:

    struct JobResult {
        bool isRestore        = false;
        bool ok               = true;
        bool cancelled        = false;
        Result res            = 0;
        io::BackupStage stage = io::BackupStage::Copy;
        std::string successMsg;
        std::string dataType;

        std::optional<Transfer::SendOutcome> send;

        std::vector<std::string> skippedInUse;

        size_t failed = 0;
        size_t total  = 0;
    };

    static TransferJob& get(void)
    {
        static TransferJob instance;
        return instance;
    }

    void enqueueBackup(Title title, BackupKind kind, std::u16string dstPath, std::string dataType);
    void enqueueRestore(Title title, BackupKind kind, std::u16string srcPath, std::string dataType, std::string successMsg);

    void enqueueSend(
        Title title, std::u16string backupPath, std::string backupName, std::string dataType, std::string ip, u16 port, std::string token);

    void start(void);

    bool active(void) const { return mState.load() == State::Running; }

    std::optional<JobResult> takeResult(void);

    static void runThread(void);

private:
    TransferJob(void) = default;

    void run(void);

    enum class Kind { Backup, Restore, Send };

    struct WorkItem {
        Kind op = Kind::Backup;
        Title title;
        BackupKind kind = BackupKind::Save;
        std::u16string path;
        std::string dataType;
        std::string successMsg;

        std::string backupName;
        std::string ip;
        u16 port = 0;
        std::string token;
    };

    enum class State { Idle, Running, Done };

    std::deque<WorkItem> mQueue;
    std::mutex mMutex;
    std::atomic<State> mState{State::Idle};
    JobResult mResult;
};

#endif
