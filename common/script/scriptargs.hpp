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

#ifndef SCRIPTARGS_HPP
#define SCRIPTARGS_HPP

#include <cstddef>

struct ParseState;
struct Value;

class ScriptArgs {
public:

    static constexpr int kMaxListItems = 65536;

    ScriptArgs(struct ParseState* parser, struct Value** param, int numArgs, const char* binding)
        : mParser(parser), mParam(param), mNumArgs(numArgs), mBinding(binding)
    {
    }

    int num(int i) const;

    int numInRange(int i, int min, int max) const;

    size_t byteCount(int i) const;

    int count(int i) const;

    const char* str(int i) const;

    const char* strOr(int i, const char* fallback) const;

    void* ptr(int i) const;
    void* ptrOrNull(int i) const;

    char** outStr(int i) const;

    char** outStrOrNull(int i) const;
    int* outInt(int i) const;

    char* outBuf(int i, int size) const;

    char* const* strArray(int i, int count) const;
    int* intArray(int i, int count) const;

    [[noreturn]]
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    void fail(const char* fmt, ...) const;

private:
    struct Value* at(int i) const;
    int intAt(int i) const;
    void* ptrAt(int i, bool allowNull) const;

    struct ParseState* mParser;
    struct Value** mParam;
    int mNumArgs;
    const char* mBinding;
};

#endif
