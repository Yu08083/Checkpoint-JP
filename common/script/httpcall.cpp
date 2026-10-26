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

#include "httpcall.hpp"
#include "scriptconsole.hpp"
#include <cstdio>
#include <curl/curl.h>
#include <new>

extern "C" {
#include "checkpoint_api.h"
}

namespace {

    bool curlReady(void)
    {
        static bool ready = false;
        if (!ready) {
            ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        }
        return ready;
    }

    size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        const size_t n = size * nmemb;
        try {
            ((std::string*)userdata)->append(ptr, n);
        }
        catch (...) {
            return 0;
        }
        return n;
    }

    int abortOnScriptCancel(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
    {
        return ckpt_script_abort_requested();
    }

    struct UploadProgress {
        int lastPct = -1;
    };
    int uploadProgress(void* p, curl_off_t, curl_off_t, curl_off_t ultotal, curl_off_t ulnow)
    {
        if (ckpt_script_abort_requested()) {
            return 1;
        }
        UploadProgress* up = (UploadProgress*)p;
        if (up && ultotal > 0) {
            const int pct = (int)((ulnow * 100) / ultotal);
            if (pct != up->lastPct) {
                if (up->lastPct < 0) {

                    ScriptConsole::get().beginIo("upload", (long long)ultotal);
                }
                up->lastPct = pct;
                ScriptConsole::get().setIo((long long)ulnow);
            }
        }
        return 0;
    }

    struct curl_slist* headerSlist(const char* headers)
    {
        struct curl_slist* hl = nullptr;
        if (headers && headers[0]) {
            std::string all(headers);
            size_t start = 0;
            while (start < all.size()) {
                const size_t nl  = all.find('\n', start);
                std::string line = all.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
                if (!line.empty()) {
                    hl = curl_slist_append(hl, line.c_str());
                }
                if (nl == std::string::npos) {
                    break;
                }
                start = nl + 1;
            }
        }
        return hl;
    }

    void applyCommonOptions(CURL* curl, const Http::Request& req, std::string* data, std::string* rhdr)
    {
        curl_easy_setopt(curl, CURLOPT_URL, req.url ? req.url : "");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
        if (rhdr) {
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeToString);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, rhdr);
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, req.followRedirects ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Checkpoint-curl");
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 300L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);
    }
}

namespace Http {
    Response perform(const Request& req)
    {
        Response res;

        FILE* upload    = nullptr;
        long uploadSize = 0;
        if (req.uploadPath) {
            upload = fopen(req.uploadPath, "rb");
            if (!upload) {
                res.result = Result::FileError;
                return res;
            }
            fseek(upload, 0, SEEK_END);
            uploadSize = ftell(upload);
            rewind(upload);
            if (uploadSize < 0) {
                fclose(upload);
                res.result = Result::FileError;
                return res;
            }
        }

        CURL* curl = curlReady() ? curl_easy_init() : nullptr;
        if (!curl) {
            if (upload) {
                fclose(upload);
            }
            res.result = Result::Unavailable;
            return res;
        }

        struct curl_slist* hl = headerSlist(req.headers);
        UploadProgress prog;
        applyCommonOptions(curl, req, &res.body, req.captureHeaders ? &res.headers : nullptr);
        if (req.method && req.method[0]) {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method);
        }
        if (hl) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hl);
        }
        if (upload) {
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_READDATA, upload);
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)uploadSize);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, uploadProgress);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
        }
        else {
            if (req.body && req.bodySize > 0) {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.bodySize);
            }
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortOnScriptCancel);
        }

        const CURLcode code = curl_easy_perform(curl);
        if (code == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res.httpStatus);
        }
        if (hl) {
            curl_slist_free_all(hl);
        }
        curl_easy_cleanup(curl);
        if (upload) {
            fclose(upload);
            ScriptConsole::get().endIo();
        }

        if (code != CURLE_OK) {
            res.code = (int)code;

            res.result = code == CURLE_WRITE_ERROR ? Result::OutOfMemory : Result::TransferFailed;
            res.body.clear();
            res.headers.clear();
        }
        return res;
    }

    std::string encode(const char* s)
    {
        CURL* curl = curlReady() ? curl_easy_init() : nullptr;
        if (!curl) {
            return std::string();
        }
        char* enc = curl_easy_escape(curl, s ? s : "", 0);
        std::string out;
        if (enc) {
            try {
                out.assign(enc);
            }
            catch (...) {
                out.clear();
            }
            curl_free(enc);
        }
        curl_easy_cleanup(curl);
        return out;
    }

    const char* strerror(int code)
    {
        return curl_easy_strerror((CURLcode)code);
    }
}
