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

#ifndef TRANSFERPROTOCOL_HPP
#define TRANSFERPROTOCOL_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace TransferProto {

    struct ZipEntry {
        std::string name;
        uint32_t crc;
        uint32_t size;
        uint32_t offset;
        bool isDirectory;
    };

    uint32_t updateCrc(uint32_t crc, const uint8_t* data, size_t len);

    void appendLe16(std::string& out, uint16_t v);
    void appendLe32(std::string& out, uint32_t v);

    struct SendFile {
        std::string absPath;
        std::string relPath;
        uint32_t size;
    };

    static constexpr uint64_t kZipMaxSize = 0xFFFFFFFFull;

    std::optional<uint64_t> zipStreamSize(const std::vector<SendFile>& files, const std::vector<std::string>& dirs);

    bool isSafeZipRelativePath(const std::string& relPath);

    std::string sanitizeFileName(const std::string& name);

    std::string headerValue(const std::string& headers, const std::string& key);

    bool constantTimeEquals(const std::string& a, const std::string& b);

    struct HostPort {
        std::string ip;
        uint16_t port;
    };
    std::optional<HostPort> parseTarget(const std::string& ipPort);

    bool validPin(const std::string& pin);

    static constexpr uint16_t kDefaultServerPort = 8000;
    static constexpr uint16_t kMinServerPort     = 1024;

    bool validServerPort(int64_t port);

    struct ByteReader {
        virtual ~ByteReader()                    = default;
        virtual size_t read(void* dst, size_t n) = 0;
    };

    struct ExtractSink {
        virtual ~ExtractSink()                                                = default;
        virtual bool makeDir(const std::string& relPath)                      = 0;
        virtual bool beginFile(const std::string& relPath, uint32_t sizeHint) = 0;
        virtual bool writeFile(const void* data, size_t n)                    = 0;
        virtual void endFile()                                                = 0;
    };

    struct FileReader {
        virtual ~FileReader()                         = default;
        virtual bool open(const std::string& absPath) = 0;
        virtual size_t read(void* dst, size_t n)      = 0;
        virtual void close()                          = 0;
    };

    struct ByteSink {
        virtual ~ByteSink()                                = default;
        virtual bool sendAll(const void* data, size_t len) = 0;
    };

    using CancelFn = std::function<bool()>;

    using ProgressFn = std::function<void(size_t)>;

    bool parseMultipart(const std::string& head, uint64_t bodyLen, const std::string& boundary, std::string& outMeta, uint64_t& outFileOffset,
        uint64_t& outFileLen, std::string& outError);

    bool extractZip(ByteReader& in, uint64_t limit, ExtractSink& sink, const CancelFn& cancelled, const ProgressFn& onBytes, std::string& outError);

    bool sendZipStream(ByteSink& out, const std::vector<SendFile>& files, const std::vector<std::string>& dirs, FileReader& src,
        const CancelFn& cancelled, const ProgressFn& onBytes, bool& wasCancelled);
}

#endif
