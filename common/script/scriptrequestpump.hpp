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

#ifndef SCRIPTREQUESTPUMP_HPP
#define SCRIPTREQUESTPUMP_HPP

#include "scriptuibridge.hpp"
#include <cstddef>
#include <string>
#include <vector>

class ScriptRequestSink {
public:
    virtual ~ScriptRequestSink(void) = default;

    virtual void showMessage(const std::string& prompt)                                                                               = 0;
    virtual void showConfirm(const std::string& prompt)                                                                               = 0;
    virtual void showPickOne(const std::string& prompt, const std::vector<std::string>& items)                                        = 0;
    virtual void showPickMany(const std::string& prompt, const std::vector<std::string>& items, const std::vector<bool>& preselected) = 0;

    virtual std::string keyboard(const std::string& prompt, size_t maxChars) = 0;
    virtual int numpad(const std::string& prompt, int min, int max)          = 0;
};

void pumpScriptRequests(ScriptUiBridge& bridge, ScriptRequestSink& sink);

#endif
