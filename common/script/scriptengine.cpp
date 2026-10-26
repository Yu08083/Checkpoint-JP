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

#include "scriptengine.hpp"
#include "logging.hpp"
#include "scriptconsole.hpp"
#include <exception>

extern "C" {
#include "picoc.h"
}

namespace {

    constexpr int PICOC_STACKSIZE = 64 * 1024;

    constexpr size_t OUTPUT_TAIL = 4096;
}

ScriptEngine::Outcome ScriptEngine::run(const std::string& path, const std::vector<std::string>& args)
{

    Picoc pc;
    PicocInitialize(&pc, PICOC_STACKSIZE);

    try {
        if (!PicocPlatformSetExitPoint(&pc)) {
            std::vector<char*> argv;
            argv.reserve(args.size());
            for (const auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }

            PicocPlatformScanFile(&pc, path.c_str());
            PicocCallMain(&pc, (int)argv.size(), argv.data());
        }
    }
    catch (const std::exception& e) {
        Logging::error("[script] uncaught exception: {}", e.what());
        pc.PicocExitValue = -1;
    }
    catch (...) {
        Logging::error("[script] uncaught non-standard exception");
        pc.PicocExitValue = -1;
    }

    Outcome outcome;
    outcome.exitValue = pc.PicocExitValue;

    PicocCleanup(&pc);

    outcome.output = ScriptConsole::get().tail(OUTPUT_TAIL);

    Logging::info("[script] {} exited with {}", path, outcome.exitValue);
    if (!outcome.output.empty()) {
        Logging::info("[script] output: {}", outcome.output);
    }

    return outcome;
}
