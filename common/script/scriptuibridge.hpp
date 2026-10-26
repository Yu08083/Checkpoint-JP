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

#ifndef SCRIPTUIBRIDGE_HPP
#define SCRIPTUIBRIDGE_HPP

#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct UiRequest {
    enum class Kind { Message, Confirm, PickOne, PickMany, Keyboard, Numpad };
    Kind kind = Kind::Message;
    std::string prompt;
    std::vector<std::string> items;
    std::vector<bool> preselected;
    int maxChars = 0;
    int numMin   = 0;
    int numMax   = 0;
};

struct UiResponse {
    bool confirmed = false;
    int index      = -1;
    std::vector<bool> selected;
    std::string text;
};

class ScriptUiBridge {
public:

    UiResponse request(UiRequest req);

    void setStatus(std::string text);

    std::optional<UiRequest> take(void);

    void respond(UiResponse resp);

    void cancelAll(void);

    std::string statusText(void);

    void reset(void);

private:
    std::mutex mMutex;
    std::condition_variable mCv;
    std::optional<UiRequest> mReq;
    std::optional<UiResponse> mResp;
    std::string mStatus;

    bool mTaken     = false;
    bool mCancelled = false;
};

#endif
