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

#ifndef HTTPCALL_HPP
#define HTTPCALL_HPP

#include <string>

namespace Http {
    enum class Result {
        Ok,
        Unavailable,
        FileError,
        TransferFailed,
        OutOfMemory,
    };

    struct Request {

        const char* method = nullptr;
        const char* url    = nullptr;

        const char* headers = nullptr;

        const char* body = nullptr;
        long bodySize    = 0;

        const char* uploadPath = nullptr;

        bool followRedirects = true;

        bool captureHeaders = false;
    };

    struct Response {
        Result result   = Result::Ok;
        int code        = 0;
        long httpStatus = 0;
        std::string body;
        std::string headers;
    };

    Response perform(const Request& req);

    std::string encode(const char* s);

    const char* strerror(int code);
}

#endif
