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

#ifndef TRANSFER_HPP
#define TRANSFER_HPP

#include "title.hpp"
#include <optional>
#include <string>

namespace Transfer {

    struct TransferTarget {
        std::string ip;
        u16 port = 0;
    };

    std::optional<TransferTarget> parseTarget(const std::string& ipPort);

    bool validPin(const std::string& pin);

    void sweepTempFiles(void);

    bool startReceiver(std::string& outError, u64 selectedTitleId = 0);
    void stopReceiver(void);
    bool receiverRunning(void);
    bool consumePendingRefresh(void);
    std::string receiverToken(void);
    std::string receiverIp(void);
    int receiverPort(void);
    std::string receiverNotice(void);
    bool receiverHasCompleted(void);
    std::string receiverCompletedName(void);
    void clearReceiverNotice(void);
    void clearReceiverCompletion(void);

    enum class SendStage { EmptyBackup, PayloadTooLarge, Zip, Socket, Resolve, Connect, Send, Response, Cancelled };

    struct SendOutcome {
        bool ok         = false;
        SendStage stage = SendStage::Send;
        std::string detail;
    };

    SendOutcome sendBackup(const Title& title, const std::u16string& backupPath, const std::string& backupName, const std::string& dataType,
        const std::string& ip, u16 port, const std::string& token);
}

#endif
