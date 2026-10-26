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

#include "scriptargs.hpp"
#include "scriptheap.hpp"
#include "scripthost.hpp"
#include <cstdint>
#include <cstring>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>
#include <string>

extern "C" {
#include "checkpoint_api.h"
#include "interpreter.h"
}

namespace {

    constexpr char kMagic[8]      = {'C', 'K', 'P', 'T', 'S', 'E', 'A', 'L'};
    constexpr uint8_t kVersion    = 1;
    constexpr uint8_t kFlagPasswd = 0x01;
    constexpr size_t kSaltSize    = 16;
    constexpr size_t kNonceSize   = 12;
    constexpr size_t kTagSize     = 16;
    constexpr size_t kKeySize     = 32;
    constexpr size_t kHeaderSize  = 60;
    constexpr size_t kAadSize     = 44;
    constexpr size_t kOffFlags    = 9;
    constexpr size_t kOffSource   = 10;
    constexpr size_t kOffIters    = 12;
    constexpr size_t kOffSalt     = 16;
    constexpr size_t kOffNonce    = 32;
    constexpr size_t kOffTag      = 44;

    constexpr uint32_t kIterations = 60000;

    enum SealResult : int {
        SealOk         = 0,
        SealBadArgs    = -1,
        SealBadVersion = -2,
        SealNoMemory   = -3,
        SealNoKey      = -4,
        SealAuthFailed = -5,
        SealNoRandom   = -6,
    };

    void putU32(uint8_t* p, uint32_t v)
    {
        p[0] = (uint8_t)(v & 0xFF);
        p[1] = (uint8_t)((v >> 8) & 0xFF);
        p[2] = (uint8_t)((v >> 16) & 0xFF);
        p[3] = (uint8_t)((v >> 24) & 0xFF);
    }

    uint32_t getU32(const uint8_t* p)
    {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    bool looksSealed(const uint8_t* blob, size_t size)
    {
        return blob != nullptr && size >= kHeaderSize && memcmp(blob, kMagic, sizeof(kMagic)) == 0;
    }

    bool deriveKey(int source, const char* passphrase, const uint8_t* salt, uint32_t iterations, uint8_t* outKey)
    {
        const bool hasPass = passphrase != nullptr && passphrase[0] != '\0';
        if (source == DeviceKeySourceNone && !hasPass) {
            return false;
        }

        uint8_t material[9 + kKeySize + kKeySize];
        memset(material, 0, sizeof(material));
        memcpy(material, "CKPTSEAL1", 9);
        bool ok = true;

        if (source != DeviceKeySourceNone && ScriptHost::get().deviceSecret(material + 9, source) < 0) {
            ok = false;
        }

        if (ok && hasPass) {
            const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
            mbedtls_md_context_t md_ctx;
            mbedtls_md_init(&md_ctx);
            ok = md != nullptr && mbedtls_md_setup(&md_ctx, md, 1  ) == 0 &&
                 mbedtls_pkcs5_pbkdf2_hmac(&md_ctx, (const unsigned char*)passphrase, strlen(passphrase), salt, kSaltSize, iterations,
                     (uint32_t)kKeySize, material + 9 + kKeySize) == 0;
            mbedtls_md_free(&md_ctx);
        }

        if (ok) {

            const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
            ok                          = md != nullptr && mbedtls_md_hmac(md, salt, kSaltSize, material, sizeof(material), outKey) == 0;
        }

        mbedtls_platform_zeroize(material, sizeof(material));
        return ok;
    }

    bool handOver(const uint8_t* data, size_t size, char** out, int* outSize)
    {
        void* copy = ScriptHeap::get().dupString(std::string((const char*)data, size));
        if (copy == nullptr) {
            return false;
        }
        *out     = (char*)copy;
        *outSize = (int)size;
        return true;
    }
}

void ckpt_device_seal(struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    const ScriptArgs args(Parser, Param, NumArgs, "device_seal");
    const size_t plainSize = args.byteCount(1);
    const char* plain      = plainSize > 0 ? args.str(0) : args.strOr(0, "");
    const char* passphrase = args.strOr(2, "");
    char** out             = args.outStr(3);
    int* outSize           = args.outInt(4);
    *out                   = nullptr;
    *outSize               = 0;

    if (plainSize == 0) {
        ReturnValue->Val->Integer = SealBadArgs;
        return;
    }

    std::string blob(kHeaderSize + plainSize, '\0');
    uint8_t* buf = (uint8_t*)&blob[0];
    memcpy(buf, kMagic, sizeof(kMagic));
    buf[8]           = kVersion;
    const bool hasPw = passphrase[0] != '\0';
    buf[kOffFlags]   = hasPw ? kFlagPasswd : 0;
    putU32(buf + kOffIters, hasPw ? kIterations : 0);

    if (!ScriptHost::get().randomBytes(buf + kOffSalt, kSaltSize) || !ScriptHost::get().randomBytes(buf + kOffNonce, kNonceSize)) {
        ReturnValue->Val->Integer = SealNoRandom;
        return;
    }

    uint8_t probe[kKeySize];
    int source = ScriptHost::get().deviceSecret(probe, DeviceKeySourceBest);
    mbedtls_platform_zeroize(probe, sizeof(probe));

    if (source < 0) {
        source = DeviceKeySourceNone;
    }
    buf[kOffSource] = (uint8_t)source;

    uint8_t key[kKeySize];
    if (!deriveKey(source, passphrase, buf + kOffSalt, getU32(buf + kOffIters), key)) {
        mbedtls_platform_zeroize(key, sizeof(key));
        ReturnValue->Val->Integer = SealNoKey;
        return;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, (unsigned int)(kKeySize * 8));
    if (rc == 0) {
        rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plainSize, buf + kOffNonce, kNonceSize, buf, kAadSize, (const unsigned char*)plain,
            buf + kHeaderSize, kTagSize, buf + kOffTag);
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));

    if (rc != 0) {
        ReturnValue->Val->Integer = SealBadArgs;
        return;
    }
    if (!handOver(buf, blob.size(), out, outSize)) {
        ReturnValue->Val->Integer = SealNoMemory;
        return;
    }
    ReturnValue->Val->Integer = SealOk;
}

void ckpt_device_unseal(struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    const ScriptArgs args(Parser, Param, NumArgs, "device_unseal");
    const size_t blobSize  = args.byteCount(1);
    const char* blobArg    = blobSize > 0 ? args.str(0) : args.strOr(0, "");
    const char* passphrase = args.strOr(2, "");
    char** out             = args.outStr(3);
    int* outSize           = args.outInt(4);
    *out                   = nullptr;
    *outSize               = 0;

    const uint8_t* blob = (const uint8_t*)blobArg;
    if (!looksSealed(blob, blobSize)) {
        ReturnValue->Val->Integer = SealBadArgs;
        return;
    }
    if (blob[8] != kVersion) {
        ReturnValue->Val->Integer = SealBadVersion;
        return;
    }

    uint8_t key[kKeySize];
    if (!deriveKey((int)blob[kOffSource], passphrase, blob + kOffSalt, getU32(blob + kOffIters), key)) {
        mbedtls_platform_zeroize(key, sizeof(key));
        ReturnValue->Val->Integer = SealNoKey;
        return;
    }

    const size_t cipherSize = blobSize - kHeaderSize;
    std::string plain(cipherSize == 0 ? 1 : cipherSize, '\0');
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, (unsigned int)(kKeySize * 8));
    if (rc == 0) {
        rc = mbedtls_gcm_auth_decrypt(
            &gcm, cipherSize, blob + kOffNonce, kNonceSize, blob, kAadSize, blob + kOffTag, kTagSize, blob + kHeaderSize, (unsigned char*)&plain[0]);
    }
    mbedtls_gcm_free(&gcm);
    mbedtls_platform_zeroize(key, sizeof(key));

    if (rc != 0) {

        mbedtls_platform_zeroize(&plain[0], plain.size());
        ReturnValue->Val->Integer = SealAuthFailed;
        return;
    }
    const bool ok = handOver((const uint8_t*)plain.data(), cipherSize, out, outSize);
    mbedtls_platform_zeroize(&plain[0], plain.size());
    ReturnValue->Val->Integer = ok ? SealOk : SealNoMemory;
}

void ckpt_seal_needs_passphrase(struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)
{
    const ScriptArgs args(Parser, Param, NumArgs, "seal_needs_passphrase");
    const size_t blobSize = args.byteCount(1);
    const char* blob      = blobSize > 0 ? args.str(0) : args.strOr(0, "");

    if (!looksSealed((const uint8_t*)blob, blobSize)) {
        ReturnValue->Val->Integer = SealBadArgs;
        return;
    }
    ReturnValue->Val->Integer = (((const uint8_t*)blob)[kOffFlags] & kFlagPasswd) != 0 ? 1 : 0;
}
