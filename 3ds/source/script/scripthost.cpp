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

#include "scripthost.hpp"
#include "backuptarget.hpp"
#include "common.hpp"
#include "directory.hpp"
#include "fsstream.hpp"
#include "loader.hpp"
#include "logging.hpp"
#include "scriptheap.hpp"
#include "smdh.hpp"
#include "title.hpp"
#include "titlequirks.hpp"
#include <3ds.h>
#include <cstring>
#include <mbedtls/sha256.h>
#include <string>
#include <vector>

namespace {

    constexpr int MAX_SAV_HANDLES = 8;
    struct SavSlot {
        ArchiveHandle arch;
        bool commitable = false;
        u32 uniqueId    = 0;
    };

    std::u16string archivePath(const char* path)
    {
        std::string p = path;
        if (p.empty() || p[0] != '/') {
            p = "/" + p;
        }
        return StringUtils::UTF8toUTF16(p.c_str());
    }

    bool isArchiveDir(FS_Archive archive, const std::u16string& path)
    {
        Handle dir;
        if (R_FAILED(FSUSER_OpenDirectory(&dir, archive, fsMakePath(PATH_UTF16, path.data())))) {
            return false;
        }
        FSDIR_Close(dir);
        return true;
    }

    bool extdataExists(u32 extdataId)
    {
        FS_Archive archive;
        if (R_FAILED(Archive::extdata(&archive, extdataId))) {
            return false;
        }
        FSUSER_CloseArchive(archive);
        return true;
    }

    FS_ExtSaveDataInfo sdExtdataInfo(u32 extdataId)
    {
        FS_ExtSaveDataInfo info = {};
        info.mediaType          = MEDIATYPE_SD;
        info.saveId             = extdataId;
        return info;
    }

    class CtrScriptHost : public ScriptHost {
    public:
        int titleCount(void) override
        {
            refreshExtras();
            return mSaveCount + (int)mExtras.size();
        }

        bool titleAt(int idx, HostTitle& out) override
        {
            Title title;
            if (!catalogTitle(idx, title)) {
                return false;
            }
            out.id          = title.id();
            out.name        = title.longDescription();
            out.productCode = title.productCode;
            out.isCart      = title.mediaType() == MEDIATYPE_GAME_CARD;
            out.hasSave     = title.accessibleSave();
            out.hasExtdata  = title.accessibleExtdata();
            return true;
        }

        int titleIndexOf(uint64_t id) override
        {
            const int save = TitleCatalog::get().indexById((u64)id, BackupKind::Save);
            if (save >= 0) {
                return save;
            }

            refreshExtras();
            for (size_t i = 0; i < mExtras.size(); i++) {
                if (mExtras[i].id == (u64)id) {
                    return mSaveCount + (int)i;
                }
            }
            return -1;
        }

        std::string titleBackupPath(int idx, int kind) override
        {
            Title title;
            if (!catalogTitle(idx, title)) {
                return "";
            }
            BackupTarget target = title.backup(kind == 0 ? BackupKind::Save : BackupKind::Extdata);
            return StringUtils::UTF16toUTF8(target.rootPath());
        }

        int savOpen(int titleIdx, int kind) override
        {
            Title title;
            if (!catalogTitle(titleIdx, title)) {
                return -1;
            }
            const int slot = freeSlot();
            if (slot < 0) {
                return -2;
            }

            if (kind == 0) {
                const bool spiCart = title.mediaType() == MEDIATYPE_GAME_CARD && title.cardType() != CARD_CTR;
                if (!title.accessibleSave() || title.isGBAVC() || title.isDSiWare() || spiCart) {
                    return -1;
                }
            }
            else if (!title.accessibleExtdata()) {
                return -1;
            }

            Result res           = 0;
            ArchiveHandle handle = title.backup(kind == 0 ? BackupKind::Save : BackupKind::Extdata).open(res);
            if (!handle) {
                return R_FAILED(res) ? (int)res : -1;
            }

            mSlots[slot].arch       = std::move(handle);
            mSlots[slot].commitable = kind == 0;
            mSlots[slot].uniqueId   = title.uniqueId();
            return slot;
        }

        int savOpenShared(uint64_t id) override
        {

            const int slot = freeSlot();
            if (slot < 0) {
                return -2;
            }

            FS_Archive archive;
            const u32 path[3] = {MEDIATYPE_NAND, (u32)id, (u32)(id >> 32)};
            Result res        = FSUSER_OpenArchive(&archive, ARCHIVE_SHARED_EXTDATA, {PATH_BINARY, 0xC, path});
            if (R_FAILED(res)) {
                return (int)res;
            }

            mSlots[slot].arch       = ArchiveHandle::fromFs(archive);
            mSlots[slot].commitable = false;
            mSlots[slot].uniqueId   = 0;
            return slot;
        }

        int savOpenExtdata(u32 extdataId) override
        {
            const int slot = freeSlot();
            if (slot < 0) {
                return -2;
            }

            FS_Archive archive;
            const Result res = Archive::extdata(&archive, extdataId);
            if (R_FAILED(res)) {
                return (int)res;
            }

            mSlots[slot].arch       = ArchiveHandle::fromFs(archive);
            mSlots[slot].commitable = false;
            mSlots[slot].uniqueId   = 0;
            return slot;
        }

        int savOpenSystem(u32 saveId) override
        {
            const int slot = freeSlot();
            if (slot < 0) {
                return -2;
            }

            FS_Archive archive;
            const u32 path[2] = {MEDIATYPE_NAND, saveId};
            const Result res  = FSUSER_OpenArchive(&archive, ARCHIVE_SYSTEM_SAVEDATA, {PATH_BINARY, 8, path});
            if (R_FAILED(res)) {
                return (int)res;
            }

            Logging::warning("[script] opened NAND system savedata 0x{:08X}", saveId);

            mSlots[slot].arch       = ArchiveHandle::fromFs(archive);
            mSlots[slot].commitable = true;
            mSlots[slot].uniqueId   = 0;
            return slot;
        }

        bool savValid(int handle) override { return handle >= 0 && handle < MAX_SAV_HANDLES && mSlots[handle].arch; }

        int savRead(int handle, const char* path, char** outBuf, int* outSize) override
        {
            FSStream stream(mSlots[handle].arch.fs(), archivePath(path), FS_OPEN_READ);
            if (!stream.good()) {
                const Result res = stream.result();
                stream.close();
                return (int)res;
            }

            const u32 size = stream.size();
            char* buf      = (char*)ScriptHeap::get().alloc(size + 1);
            if (!buf) {
                stream.close();
                return -3;
            }

            const u32 read   = stream.read(buf, size);
            const Result res = stream.result();
            stream.close();
            if (read != size) {

                Logging::warning("[script] sav_read '{}': {} of {} bytes (res 0x{:08X})", path, read, size, (u32)res);
                ScriptHeap::get().release(buf);
                return R_FAILED(res) ? (int)res : -4;
            }

            buf[read] = '\0';
            *outBuf   = buf;
            *outSize  = (int)read;
            return 0;
        }

        int savWrite(int handle, const char* path, const void* data, size_t size) override
        {

            const std::u16string p = archivePath(path);
            FSUSER_DeleteFile(mSlots[handle].arch.fs(), fsMakePath(PATH_UTF16, p.data()));

            FSStream stream(mSlots[handle].arch.fs(), p, FS_OPEN_WRITE, (u32)size);
            if (!stream.good()) {
                const Result res = stream.result();
                stream.close();
                return (int)res;
            }

            const u32 written = stream.write(data, (u32)size);
            const Result res  = stream.result();
            stream.close();
            return written == (u32)size ? 0 : (R_FAILED(res) ? (int)res : -3);
        }

        int savDelete(int handle, const char* path) override
        {
            const std::u16string p = archivePath(path);
            return (int)FSUSER_DeleteFile(mSlots[handle].arch.fs(), fsMakePath(PATH_UTF16, p.data()));
        }

        int savMkdir(int handle, const char* path) override
        {
            const std::u16string p = archivePath(path);
            return (int)FSUSER_CreateDirectory(mSlots[handle].arch.fs(), fsMakePath(PATH_UTF16, p.data()), 0);
        }

        int savRmdir(int handle, const char* path) override
        {

            const std::u16string p = archivePath(path);
            return (int)FSUSER_DeleteDirectory(mSlots[handle].arch.fs(), fsMakePath(PATH_UTF16, p.data()));
        }

        int savRename(int handle, const char* from, const char* to) override
        {

            const std::u16string f = archivePath(from);
            const std::u16string t = archivePath(to);
            const FS_Archive arch  = mSlots[handle].arch.fs();
            if (isArchiveDir(arch, f)) {
                return (int)FSUSER_RenameDirectory(arch, fsMakePath(PATH_UTF16, f.data()), arch, fsMakePath(PATH_UTF16, t.data()));
            }
            return (int)FSUSER_RenameFile(arch, fsMakePath(PATH_UTF16, f.data()), arch, fsMakePath(PATH_UTF16, t.data()));
        }

        bool savList(int handle, const std::string& dir, std::vector<HostDirEntry>& out) override
        {
            Directory items(mSlots[handle].arch.fs(), StringUtils::UTF8toUTF16(dir.c_str()));
            if (!items.good()) {
                return false;
            }
            for (size_t i = 0, sz = items.size(); i < sz; i++) {
                out.push_back({StringUtils::UTF16toUTF8(items.entry(i)), items.folder(i)});
            }
            return true;
        }

        int savCommit(int handle) override
        {
            if (!mSlots[handle].commitable) {
                return 0;
            }

            Result res = FSUSER_ControlArchive(mSlots[handle].arch.fs(), ARCHIVE_ACTION_COMMIT_SAVE_DATA, NULL, 0, NULL, 0);
            if (R_SUCCEEDED(res) && mSlots[handle].uniqueId != 0) {

                u8 out;
                u64 secureValue = ((u64)SECUREVALUE_SLOT_SD << 32) | (mSlots[handle].uniqueId << 8);
                res             = FSUSER_ControlSecureSave(SECURESAVE_ACTION_DELETE, &secureValue, 8, &out, 1);
            }
            return (int)res;
        }

        void savClose(int handle) override
        {
            if (handle >= 0 && handle < MAX_SAV_HANDLES) {
                mSlots[handle] = SavSlot{};
            }
        }

        void savCloseAll(void) override
        {
            for (int i = 0; i < MAX_SAV_HANDLES; i++) {
                mSlots[i] = SavSlot{};
            }
        }

        int extdataDefaultId(uint64_t titleId) override
        {

            return (int)TitleQuirks::extdataIdFor((u64)titleId);
        }

        int extdataCreate(uint64_t titleId, u32 extdataId, int maxDirs, int maxFiles) override
        {

            if (extdataExists(extdataId)) {
                return -2;
            }

            Title title;
            const int idx = titleIndexOf(titleId);
            if (idx < 0 || !catalogTitle(idx, title)) {
                return -1;
            }

            smdh_s* smdh = loadSMDH((u32)titleId, (u32)(titleId >> 32), (u8)title.mediaType());
            if (smdh == NULL) {
                Logging::warning("[script] extdata_create: no SMDH for title {:016X}, creating without an icon", (u64)titleId);
            }

            const Result res = FSUSER_CreateExtSaveData(
                sdExtdataInfo(extdataId), (u32)maxDirs, (u32)maxFiles, 0xFFFFFFFFFFFFFFFFULL, smdh ? sizeof(smdh_s) : 0, (u8*)smdh);
            delete smdh;

            if (R_FAILED(res)) {
                Logging::error("[script] extdata_create: title {:016X} id 0x{:08X} failed with result 0x{:08X}.", (u64)titleId, extdataId, res);
                return (int)res;
            }
            Logging::info(
                "[script] extdata_create: title {:016X} now has extdata 0x{:08X} ({} dirs, {} files).", (u64)titleId, extdataId, maxDirs, maxFiles);
            return 0;
        }

        int extdataDelete(u32 extdataId) override
        {

            if (!extdataExists(extdataId)) {
                return -2;
            }

            const Result res = FSUSER_DeleteExtSaveData(sdExtdataInfo(extdataId));
            if (R_FAILED(res)) {
                Logging::error("[script] extdata_delete: id 0x{:08X} failed with result 0x{:08X}.", extdataId, res);
                return (int)res;
            }
            Logging::info("[script] extdata_delete: id 0x{:08X} destroyed.", extdataId);
            return 0;
        }

        int deviceSecret(uint8_t* out, int want) override
        {
            if (want != DeviceKeySourceBest && want != DeviceKeySourceCtrNand) {
                return -1;
            }
            u8 cid[16] = {0};
            if (R_FAILED(FSUSER_GetNandCid(cid, sizeof(cid)))) {
                return -1;
            }

            mbedtls_sha256_context ctx;
            mbedtls_sha256_init(&ctx);
            mbedtls_sha256_starts_ret(&ctx, 0);
            mbedtls_sha256_update_ret(&ctx, (const unsigned char*)"Checkpoint 3DS NAND CID v1", 26);
            mbedtls_sha256_update_ret(&ctx, cid, sizeof(cid));
            mbedtls_sha256_finish_ret(&ctx, out);
            mbedtls_sha256_free(&ctx);
            memset(cid, 0, sizeof(cid));
            return DeviceKeySourceCtrNand;
        }

        bool randomBytes(void* out, size_t size) override
        {

            if (R_SUCCEEDED(psInit())) {
                const Result res = PS_GenerateRandomBytes(out, size);
                psExit();
                if (R_SUCCEEDED(res)) {
                    return true;
                }
            }

            static u32 calls = 0;
            uint8_t* dest    = (uint8_t*)out;
            while (size > 0) {
                const u64 tick = svcGetSystemTick();
                const u64 now  = osGetTime();
                const u32 seq  = calls++;
                uint8_t digest[32];
                mbedtls_sha256_context ctx;
                mbedtls_sha256_init(&ctx);
                mbedtls_sha256_starts_ret(&ctx, 0);
                mbedtls_sha256_update_ret(&ctx, (const unsigned char*)&tick, sizeof(tick));
                mbedtls_sha256_update_ret(&ctx, (const unsigned char*)&now, sizeof(now));
                mbedtls_sha256_update_ret(&ctx, (const unsigned char*)&seq, sizeof(seq));
                mbedtls_sha256_finish_ret(&ctx, digest);
                mbedtls_sha256_free(&ctx);
                const size_t chunk = size < sizeof(digest) ? size : sizeof(digest);
                memcpy(dest, digest, chunk);
                dest += chunk;
                size -= chunk;
            }
            return true;
        }

        void lowerPriority(void) override
        {

            s32 prio = 0;
            if (R_SUCCEEDED(svcGetThreadPriority(&prio, CUR_THREAD_HANDLE))) {
                svcSetThreadPriority(CUR_THREAD_HANDLE, prio + 2);
            }
        }

    private:
        int freeSlot(void) const
        {
            for (int i = 0; i < MAX_SAV_HANDLES; i++) {
                if (!mSlots[i].arch) {
                    return i;
                }
            }
            return -1;
        }

        void refreshExtras(void)
        {
            TitleCatalog& catalog = TitleCatalog::get();
            const u32 generation  = catalog.generation();
            if (mExtrasGeneration == generation) {
                return;
            }
            mExtrasGeneration = generation;
            mSaveCount        = catalog.getTitleCount(BackupKind::Save);
            mExtras.clear();

            const int extdatas = catalog.getTitleCount(BackupKind::Extdata);
            for (int i = 0; i < extdatas; i++) {
                Title title;
                catalog.getTitle(title, i, BackupKind::Extdata);
                if (catalog.indexById(title.id(), BackupKind::Save) < 0) {
                    mExtras.push_back(Extra{i, title.id()});
                }
            }
        }

        bool catalogTitle(int idx, Title& out)
        {
            refreshExtras();
            if (idx < 0) {
                return false;
            }
            if (idx < mSaveCount) {
                TitleCatalog::get().getTitle(out, idx, BackupKind::Save);
                return true;
            }

            const int extra = idx - mSaveCount;
            if (extra >= (int)mExtras.size()) {
                return false;
            }
            TitleCatalog::get().getTitle(out, mExtras[extra].index, BackupKind::Extdata);
            return true;
        }

        struct Extra {
            int index;
            u64 id;
        };

        SavSlot mSlots[MAX_SAV_HANDLES];
        std::vector<Extra> mExtras;
        u32 mExtrasGeneration = 0;
        int mSaveCount        = 0;
    };
}

ScriptHost& ScriptHost::get(void)
{

    static CtrScriptHost* host = new CtrScriptHost();
    return *host;
}
