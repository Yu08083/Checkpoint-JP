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

#ifndef SCRIPTHOST_HPP
#define SCRIPTHOST_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct HostTitle {
    uint64_t id = 0;
    std::string name;
    std::string productCode;
    bool isCart     = false;
    bool hasSave    = false;
    bool hasExtdata = false;
};

struct HostDirEntry {
    std::string name;
    bool folder = false;
};

enum DeviceKeySource : int {
    DeviceKeySourceNone    = 0,
    DeviceKeySourceBest    = 1,
    DeviceKeySourceCtrNand = 2,
    DeviceKeySourceNxSpl   = 3,
    DeviceKeySourceNxCal   = 4,
};

class ScriptHost {
public:
    virtual ~ScriptHost(void) = default;

    static ScriptHost& get(void);

    virtual int titleCount(void) = 0;

    virtual bool titleAt(int idx, HostTitle& out) = 0;

    virtual int titleIndexOf(uint64_t id) = 0;

    virtual std::string titleBackupPath(int idx, int kind) = 0;

    virtual int savOpen(int titleIdx, int kind) = 0;

    virtual int savOpenShared(uint64_t id) = 0;

    virtual int savOpenExtdata(uint32_t extdataId) = 0;

    virtual int savOpenSystem(uint32_t saveId) = 0;

    virtual bool savValid(int handle) = 0;

    virtual int savRead(int handle, const char* path, char** outBuf, int* outSize) = 0;

    virtual int savWrite(int handle, const char* path, const void* data, size_t size) = 0;

    virtual int savDelete(int handle, const char* path) = 0;

    virtual int savMkdir(int handle, const char* path)                  = 0;
    virtual int savRmdir(int handle, const char* path)                  = 0;
    virtual int savRename(int handle, const char* from, const char* to) = 0;

    virtual bool savList(int handle, const std::string& dir, std::vector<HostDirEntry>& out) = 0;

    virtual int savCommit(int handle) = 0;

    virtual void savClose(int handle) = 0;

    virtual void savCloseAll(void) = 0;

    virtual int extdataDefaultId(uint64_t titleId) = 0;

    virtual int extdataCreate(uint64_t titleId, uint32_t extdataId, int maxDirs, int maxFiles) = 0;

    virtual int extdataDelete(uint32_t extdataId) = 0;

    virtual int deviceSecret(uint8_t* out, int want) = 0;

    virtual bool randomBytes(void* out, size_t size) = 0;

    virtual void lowerPriority(void) = 0;
};

#endif
