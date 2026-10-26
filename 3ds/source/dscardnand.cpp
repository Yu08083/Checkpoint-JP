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

#include "dscardnand.hpp"
#include "archive.hpp"
#include "fsstream.hpp"
#include "io.hpp"
#include "logging.hpp"
#include "ntrcard.hpp"
#include "sha256.h"
#include "util.hpp"
#include <3ds.h>
#include <array>
#include <cstring>
#include <format>
#include <memory>
#include <new>

namespace {
    const char* sectorReadStatusName(NtrCard::SectorReadStatus status)
    {
        switch (status) {
            case NtrCard::SectorReadStatus::Ok:
                return "ok";
            case NtrCard::SectorReadStatus::InvalidArgument:
                return "invalid-argument";
            case NtrCard::SectorReadStatus::ControllerDisabled:
                return "controller-disabled";
            case NtrCard::SectorReadStatus::ControllerBusy:
                return "controller-busy";
            case NtrCard::SectorReadStatus::ControllerStateChanged:
                return "controller-state-changed";
            case NtrCard::SectorReadStatus::ControllerReset:
                return "controller-reset";
            case NtrCard::SectorReadStatus::Timeout:
                return "timeout";
            case NtrCard::SectorReadStatus::WordCountMismatch:
                return "word-count-mismatch";
        }
        return "unknown";
    }

    const char* nandReadStageName(NtrCard::NandReadStage stage)
    {
        switch (stage) {
            case NtrCard::NandReadStage::Preflight:
                return "preflight";
            case NtrCard::NandReadStage::SelectWindow:
                return "select-window";
            case NtrCard::NandReadStage::ReadSector:
                return "read-sector";
            case NtrCard::NandReadStage::ReturnToRom:
                return "return-to-rom";
            case NtrCard::NandReadStage::Complete:
                return "complete";
        }
        return "unknown";
    }

    const char* cardPollStatusName(NtrCard::CardPollStatus status)
    {
        switch (status) {
            case NtrCard::CardPollStatus::Observed:
                return "observed";
            case NtrCard::CardPollStatus::Timeout:
                return "timeout";
            case NtrCard::CardPollStatus::ControllerDisabled:
                return "controller-disabled";
            case NtrCard::CardPollStatus::ControllerReset:
                return "controller-reset";
        }
        return "unknown";
    }

    std::array<char, SHA256_BLOCK_SIZE * 2 + 1> hexDigest(const std::array<BYTE, SHA256_BLOCK_SIZE>& digest)
    {
        constexpr char digits[] = "0123456789abcdef";
        std::array<char, SHA256_BLOCK_SIZE * 2 + 1> output{};
        for (std::size_t i = 0; i < digest.size(); i++) {
            output[i * 2]     = digits[digest[i] >> 4];
            output[i * 2 + 1] = digits[digest[i] & 0x0F];
        }
        return output;
    }

    std::array<char, SHA256_BLOCK_SIZE * 2 + 1> sha256Hex(const void* data, std::size_t size)
    {
        std::array<BYTE, SHA256_BLOCK_SIZE> digest{};
        SHA256_CTX context;
        sha256_init(&context);
        sha256_update(&context, static_cast<const BYTE*>(data), size);
        sha256_final(&context, digest.data());
        return hexDigest(digest);
    }

    struct GuardedPage {
        const DSCard::NandSave& nandSave;
        const NtrCard::NandSectorReadResult& trusted;
        u32 romControl;
        const char* label;

        bool syncedSentinel(const char* step) const
        {
            const auto sync = NtrCard::waitForMappedCardPoll();
            if (!sync || sync->status != NtrCard::CardPollStatus::Observed) {
                Logging::error("NTRCARD {} poll synchronisation failed before {}; no command issued.", label, step);
                return false;
            }
            const auto sentinel = NtrCard::readMappedFirstNandSaveSector(nandSave.rwStart, romControl);
            if (!sentinel || sentinel->status != NtrCard::SectorReadStatus::Ok || sentinel->stage != NtrCard::NandReadStage::Complete) {
                Logging::error("NTRCARD {} sentinel read failed before {}.", label, step);
                return false;
            }
            if (std::memcmp(sentinel->data.data(), trusted.data.data(), NtrCard::sectorSize) != 0) {
                Logging::error("NTRCARD {} sentinel mismatch before {}: the cartridge no longer returns the Gate-3 bytes.", label, step);
                return false;
            }
            return true;
        }

        bool readPageAt(u32 window, u32 firstSector, u32* into, u32* scratch, const char* step) const
        {
            if (!syncedSentinel(step)) {
                return false;
            }
            const auto first = NtrCard::readMappedNandSaveRange(
                nandSave.rwStart, window, firstSector, NtrCard::nandPageSectors, 1, romControl, into, NtrCard::nandPageWords);
            if (!first || first->status != NtrCard::SectorReadStatus::Ok) {
                Logging::error("NTRCARD {} page read failed at {}.", label, step);
                return false;
            }
            if (!syncedSentinel(step)) {
                return false;
            }
            const auto second = NtrCard::readMappedNandSaveRange(
                nandSave.rwStart, window, firstSector, NtrCard::nandPageSectors, 1, romControl, scratch, NtrCard::nandPageWords);
            if (!second || second->status != NtrCard::SectorReadStatus::Ok) {
                Logging::error("NTRCARD {} page re-read failed at {}.", label, step);
                return false;
            }
            if (std::memcmp(into, scratch, NtrCard::nandPageSize) != 0) {
                Logging::error("NTRCARD {} page read disagreed with itself at {}; refusing to continue.", label, step);
                return false;
            }
            return true;
        }

        bool readPage(u32* into, u32* scratch, const char* step) const { return readPageAt(0, 0, into, scratch, step); }
    };

    bool process9RecoveryOk(FS_MediaType media, const DSCard::NandSave& nandSave, Result& result)
    {
        std::array<u8, DSCard::headerSize> header{};
        result = FSUSER_GetLegacyRomHeader(media, 0LL, header.data());
        return R_SUCCEEDED(result) && std::memcmp(header.data() + 0x0C, nandSave.gameCode, sizeof(nandSave.gameCode) - 1) == 0;
    }

    struct ControllerReadiness {
        bool observed    = false;
        u16 mCardControl = 0;
        u32 romControl   = 0;
        bool ready       = false;
    };

    ControllerReadiness waitForProcess9Controller(u32 pollCount = 200)
    {
        ControllerReadiness readiness;
        for (u32 poll = 0; poll < pollCount; poll++) {
            const auto snapshot = NtrCard::captureMapped();
            if (!snapshot) {
                return readiness;
            }
            readiness.observed     = true;
            readiness.mCardControl = snapshot->mCardControl;
            readiness.romControl   = snapshot->romControl;
            if (NtrCard::process9ControllerReady(*snapshot)) {
                readiness.ready = true;
                return readiness;
            }
            svcSleepThread(10'000'000);
        }
        return readiness;
    }

    struct DumpEvent {
        u32 window                                  = 0;
        u32 sector                                  = 0;
        u32 attempt                                 = 0;
        NtrCard::SectorReadStatus status            = NtrCard::SectorReadStatus::Ok;
        NtrCard::NandReadStage stage                = NtrCard::NandReadStage::Preflight;
        NtrCard::SectorReadStatus returnToRomStatus = NtrCard::SectorReadStatus::Ok;
        bool returnToRomAttempted                   = false;
        bool recovered                              = false;
        bool fatal                                  = false;
        u32 sectorsRead                             = 0;
        u32 wordsRead                               = 0;
        u32 drainedWords                            = 0;
        u16 expectedMCardControl                    = 0;
        u16 observedMCardControl                    = 0;
        u32 windowBase                              = 0;
        u32 observedRomControl                      = 0;
        bool commandReadbackAvailable               = false;
        bool commandIdentityChanged                 = false;
        u32 expectedCommandHigh                     = 0;
        u32 expectedCommandLow                      = 0;
        u32 observedCommandHigh                     = 0;
        u32 observedCommandLow                      = 0;
        u32 selectTransfer                          = 0;
        u32 selectCompleted                         = 0;
        u32 readTransfer                            = 0;
        u32 readCompleted                           = 0;
        u32 returnTransfer                          = 0;
        u32 returnCompleted                         = 0;
    };

    void logDumpEvent(const DumpEvent& event, u32 windows)
    {
        const std::string message = std::format(
            "NTRCARD NAND dump {} at window {}/{} (base=0x{:08X}, sector={}, attempt={}): status={}, stage={}, sectors={}, words={}, drained={}, "
            "MCNT expected=0x{:04X} observed=0x{:04X}, ROMCNT observed=0x{:08X}, command-readable={}, command-changed={}, "
            "CMD expected=0x{:08X}{:08X} observed=0x{:08X}{:08X}, "
            "ROM-return-attempted={}, ROM-return={}, recovered={}, "
            "select=0x{:08X}/0x{:08X}, read=0x{:08X}/0x{:08X}, return=0x{:08X}/0x{:08X}.",
            event.fatal ? "failed" : "lost the bus", event.window, windows, event.windowBase, event.sector, event.attempt,
            sectorReadStatusName(event.status), nandReadStageName(event.stage), event.sectorsRead, event.wordsRead, event.drainedWords,
            event.expectedMCardControl, event.observedMCardControl, event.observedRomControl, event.commandReadbackAvailable ? "yes" : "no",
            event.commandIdentityChanged ? "yes" : "no", event.expectedCommandHigh, event.expectedCommandLow, event.observedCommandHigh,
            event.observedCommandLow, event.returnToRomAttempted ? "yes" : "no", sectorReadStatusName(event.returnToRomStatus),
            event.recovered ? "yes" : "no", event.selectTransfer, event.selectCompleted, event.readTransfer, event.readCompleted,
            event.returnTransfer, event.returnCompleted);
        if (event.fatal) {
            Logging::error(message);
        }
        else {
            Logging::info(message);
        }
    }

    DSCardNand::DumpOutcome runDump(FS_MediaType media, const DSCard::NandSave& nandSave, const NtrCard::NandSectorReadResult& gateThreeSector,
        const std::u16string& path, const std::string& name, ProgressSink& sink)
    {
        DSCardNand::DumpOutcome outcome;

        if (nandSave.saveSize == 0 || (nandSave.saveSize % NtrCard::nandWindowSize) != 0) {
            Logging::error("NTRCARD NAND dump skipped: save size 0x{:08X} is not a whole number of 128-KiB windows.", nandSave.saveSize);
            outcome.status = DSCardNand::DumpStatus::InvalidSaveSize;
            return outcome;
        }

        const u32 romControl = NtrCard::tunedRomControl(nandSave.cardControl13);
        const u32 windows    = nandSave.saveSize / NtrCard::nandWindowSize;
        outcome.windows      = windows;

        constexpr std::size_t blockSectors = 1;

        if (io::fileExists(Archive::sdmc(), path)) {
            Logging::error("NTRCARD NAND dump skipped: {} already exists.", name);
            outcome.status = DSCardNand::DumpStatus::FileError;
            return outcome;
        }

        std::unique_ptr<u32[]> image(new (std::nothrow) u32[nandSave.saveSize / sizeof(u32)]);
        if (!image) {
            Logging::error(
                "NTRCARD NAND dump could not reserve 0x{:08X} bytes of RAM; streaming to SD one window at a time instead.", nandSave.saveSize);
        }
        const bool ramMode = image != nullptr;

        std::unique_ptr<u32[]> windowBuffer;
        std::unique_ptr<FSStream> output;
        if (!ramMode) {
            windowBuffer = std::make_unique<u32[]>(NtrCard::nandWindowWords);
            output       = std::make_unique<FSStream>(Archive::sdmc(), path, FS_OPEN_WRITE, nandSave.saveSize);
            if (!output->good()) {
                Logging::error("NTRCARD NAND dump skipped: failed to open {} with result 0x{:08X}.", name, (u32)output->result());
                outcome.status = DSCardNand::DumpStatus::FileError;
                return outcome;
            }
        }

        sink.begin("Backup", 1);
        sink.startFile(StringUtils::UTF8toUTF16(name.c_str()), nandSave.saveSize);
        bool cancelled = false;

        Logging::info("NTRCARD NAND dump start: file={}, offset=0x{:08X}, size=0x{:08X}, windows={}, destination={}, sectors-per-command={}, "
                      "command-readable={}, ROMCNT=0x{:08X}.",
            name, nandSave.rwStart, nandSave.saveSize, windows, ramMode ? "ram" : "sd-stream", blockSectors,
            gateThreeSector.commandReadbackAvailable ? "yes" : "no", romControl);

        constexpr u32 sectorsPerBurst = 32;
        constexpr u32 maxBurstRetries = 16;

        constexpr u32 stallRepairThreshold = 4;
        constexpr u32 maxStallRepairs      = 8;
        static_assert(NtrCard::nandWindowSectors % sectorsPerBurst == 0);

        auto verificationBuffer = std::make_unique<u32[]>(sectorsPerBurst * NtrCard::sectorWords);

        std::array<DumpEvent, 24> events{};
        u32 eventCount    = 0;
        u32 eventsDropped = 0;
        const auto record = [&](const DumpEvent& event) {
            if (eventCount < events.size()) {
                events[eventCount++] = event;
            }
            else {
                eventsDropped++;
            }
        };

        std::array<NtrCard::StalledControllerRecovery, maxStallRepairs> repairs{};
        u32 repairCount = 0;

        constexpr u32 maxResetRecoveries = 64;

        SHA256_CTX context;
        sha256_init(&context);

        const u64 started        = osGetTime();
        bool complete            = true;
        bool firstSectorMatches  = false;
        bool firstSectorChecked  = false;
        bool cartridgeStateLost  = false;
        bool cardSlotUnavailable = false;
        u32 collisions           = 0;
        u32 resets               = 0;
        u32 contended            = 0;
        u32 drained              = 0;
        u32 done                 = 0;
        u32 foreignIdle          = 0;

        u32 busQuietWaits = 0;

        u32 pollSyncs                                  = 0;
        bool pollSyncFailed                            = false;
        u32 failedPollWindow                           = 0;
        NtrCard::CardPollStatus failedPollStatus       = NtrCard::CardPollStatus::Timeout;
        u32 failedPollRomControl                       = 0;
        u64 failedPollElapsedTicks                     = 0;
        u32 verifiedBursts                             = 0;
        u32 verificationMismatches                     = 0;
        u32 mismatchWindow                             = 0;
        u32 mismatchSector                             = 0;
        u32 mismatchByte                               = 0;
        u32 sentinelChecks                             = 0;
        u32 sentinelFailures                           = 0;
        u32 sentinelMismatches                         = 0;
        u32 failedSentinelWindow                       = 0;
        NtrCard::SectorReadStatus failedSentinelStatus = NtrCard::SectorReadStatus::InvalidArgument;
        NtrCard::NandReadStage failedSentinelStage     = NtrCard::NandReadStage::Preflight;
        bool failedSentinelReturnAttempted             = false;
        NtrCard::SectorReadStatus failedSentinelReturn = NtrCard::SectorReadStatus::InvalidArgument;
        u32 sentinelMismatchByte                       = 0;

        const auto verifyTrustedSentinel = [&](u32 window) {
            const auto sentinel = NtrCard::readMappedFirstNandSaveSector(nandSave.rwStart, romControl);
            sentinelChecks++;
            if (!sentinel || sentinel->status != NtrCard::SectorReadStatus::Ok || sentinel->stage != NtrCard::NandReadStage::Complete) {
                sentinelFailures++;
                failedSentinelWindow          = window;
                failedSentinelStatus          = sentinel ? sentinel->status : NtrCard::SectorReadStatus::InvalidArgument;
                failedSentinelStage           = sentinel ? sentinel->stage : NtrCard::NandReadStage::Preflight;
                failedSentinelReturnAttempted = sentinel && sentinel->returnToRomAttempted;
                failedSentinelReturn          = sentinel ? sentinel->returnToRomStatus : NtrCard::SectorReadStatus::InvalidArgument;
                if (sentinel) {
                    cartridgeStateLost = sentinel->stage != NtrCard::NandReadStage::Preflight &&
                                         !(sentinel->returnToRomAttempted && sentinel->returnToRomStatus == NtrCard::SectorReadStatus::Ok) &&
                                         sentinel->status != NtrCard::SectorReadStatus::ControllerReset;
                    cardSlotUnavailable = sentinel->status == NtrCard::SectorReadStatus::ControllerReset;
                }
                return false;
            }

            const auto* sentinelBytes = reinterpret_cast<const u8*>(sentinel->data.data());
            const auto* trustedBytes  = reinterpret_cast<const u8*>(gateThreeSector.data.data());
            if (std::memcmp(sentinelBytes, trustedBytes, NtrCard::sectorSize) != 0) {
                sentinelMismatches++;
                failedSentinelWindow = window;
                while (sentinelMismatchByte < NtrCard::sectorSize && sentinelBytes[sentinelMismatchByte] == trustedBytes[sentinelMismatchByte]) {
                    sentinelMismatchByte++;
                }
                return false;
            }
            return true;
        };

        for (u32 index = 0; index < windows && complete; index++) {
            const auto sync = NtrCard::waitForMappedCardPoll();
            pollSyncs++;
            if (!sync || sync->status != NtrCard::CardPollStatus::Observed) {
                pollSyncFailed         = true;
                failedPollWindow       = index;
                failedPollStatus       = sync ? sync->status : NtrCard::CardPollStatus::Timeout;
                failedPollRomControl   = sync ? sync->observedRomControl : 0;
                failedPollElapsedTicks = sync ? sync->elapsedTicks : 0;
                complete               = false;
                break;
            }

            if (!verifyTrustedSentinel(index)) {
                complete = false;
                break;
            }

            u32* const target = ramMode ? image.get() + static_cast<std::size_t>(index) * NtrCard::nandWindowWords : windowBuffer.get();
            for (u32 sector = 0; sector < NtrCard::nandWindowSectors && complete; sector += sectorsPerBurst) {
                u32 stallStreak      = 0;
                u32 stallRomControl  = 0;
                u32 forgivenAttempts = 0;
                for (u32 attempt = 0;; attempt++) {
                    auto read = NtrCard::readMappedNandSaveRange(nandSave.rwStart, index, sector, sectorsPerBurst, blockSectors, romControl,
                        target + sector * NtrCard::sectorWords, sectorsPerBurst * NtrCard::sectorWords);
                    if (!read) {
                        cardSlotUnavailable = true;
                        complete            = false;
                        break;
                    }

                    if (read->status == NtrCard::SectorReadStatus::Ok && read->stage == NtrCard::NandReadStage::Complete &&
                        read->sectorsRead == sectorsPerBurst) {
                        foreignIdle += read->idleConfigDiffers ? 1 : 0;
                        if (!verifyTrustedSentinel(index)) {
                            complete = false;
                            break;
                        }

                        auto verification = NtrCard::readMappedNandSaveRange(nandSave.rwStart, index, sector, sectorsPerBurst, blockSectors,
                            romControl, verificationBuffer.get(), sectorsPerBurst * NtrCard::sectorWords);
                        if (!verification) {
                            cardSlotUnavailable = true;
                            complete            = false;
                            break;
                        }
                        if (verification->status == NtrCard::SectorReadStatus::Ok && verification->stage == NtrCard::NandReadStage::Complete &&
                            verification->sectorsRead == sectorsPerBurst) {
                            foreignIdle += verification->idleConfigDiffers ? 1 : 0;
                            if (!verifyTrustedSentinel(index)) {
                                complete = false;
                                break;
                            }
                            const std::size_t bytes = sectorsPerBurst * NtrCard::sectorSize;
                            const auto* first       = reinterpret_cast<const u8*>(target + sector * NtrCard::sectorWords);
                            const auto* second      = reinterpret_cast<const u8*>(verificationBuffer.get());
                            if (std::memcmp(first, second, bytes) != 0) {
                                verificationMismatches++;
                                mismatchWindow = index;
                                mismatchSector = sector;
                                while (mismatchByte < bytes && first[mismatchByte] == second[mismatchByte]) {
                                    mismatchByte++;
                                }
                                complete = false;
                                break;
                            }
                            verifiedBursts++;
                            break;
                        }

                        read = std::move(verification);
                    }

                    const bool contendedStart = read->stage == NtrCard::NandReadStage::Preflight &&
                                                read->status != NtrCard::SectorReadStatus::InvalidArgument &&
                                                read->status != NtrCard::SectorReadStatus::ControllerReset;
                    const bool wasReset = read->status == NtrCard::SectorReadStatus::ControllerReset;
                    const bool lostBus =
                        !contendedStart && read->status == NtrCard::SectorReadStatus::ControllerStateChanged && read->recoveredToRomMode;
                    collisions += lostBus ? 1 : 0;
                    resets += wasReset ? 1 : 0;
                    contended += contendedStart ? 1 : 0;
                    drained += (u32)read->drainedWords;

                    DumpEvent event;
                    event.window                   = index;
                    event.sector                   = sector;
                    event.attempt                  = attempt;
                    event.status                   = read->status;
                    event.stage                    = read->stage;
                    event.returnToRomStatus        = read->returnToRomStatus;
                    event.returnToRomAttempted     = read->returnToRomAttempted;
                    event.recovered                = read->recoveredToRomMode;
                    event.sectorsRead              = (u32)read->sectorsRead;
                    event.wordsRead                = (u32)read->wordsRead;
                    event.drainedWords             = (u32)read->drainedWords;
                    event.expectedMCardControl     = read->initialMCardControl;
                    event.observedMCardControl     = read->observedMCardControl;
                    event.windowBase               = read->windowBase;
                    event.observedRomControl       = read->observedRomControl;
                    event.commandReadbackAvailable = read->commandReadbackAvailable;
                    event.commandIdentityChanged   = read->commandIdentityChanged;
                    event.expectedCommandHigh      = read->expectedCommandHigh;
                    event.expectedCommandLow       = read->expectedCommandLow;
                    event.observedCommandHigh      = read->observedCommandHigh;
                    event.observedCommandLow       = read->observedCommandLow;
                    event.selectTransfer           = read->selectTransferRomControl;
                    event.selectCompleted          = read->selectCompletedRomControl;
                    event.readTransfer             = read->readTransferRomControl;
                    event.readCompleted            = read->readCompletedRomControl;
                    event.returnTransfer           = read->returnTransferRomControl;
                    event.returnCompleted          = read->returnCompletedRomControl;

                    if (read->status == NtrCard::SectorReadStatus::ControllerBusy && read->stage == NtrCard::NandReadStage::Preflight) {
                        if (stallStreak != 0 && read->observedRomControl == stallRomControl) {
                            stallStreak++;
                        }
                        else {
                            stallRomControl = read->observedRomControl;
                            stallStreak     = 1;
                        }
                        if (stallStreak >= stallRepairThreshold && repairCount < maxStallRepairs) {
                            const auto repair = NtrCard::recoverMappedStalledController();
                            if (repair) {
                                repairs[repairCount++] = *repair;
                                stallStreak            = 0;
                            }
                        }
                    }
                    else {
                        stallStreak = 0;
                    }

                    bool retryable = false;
                    if (retryable && attempt - forgivenAttempts < maxBurstRetries) {
                        record(event);
                        svcSleepThread(contendedStart ? 10'000'000 : 20'000'000);
                        continue;
                    }

                    event.fatal = true;
                    record(event);
                    cartridgeStateLost  = read->stage != NtrCard::NandReadStage::Preflight && !read->recoveredToRomMode && !read->cartridgeWasReset;
                    cardSlotUnavailable = wasReset;
                    complete            = false;
                    break;
                }
            }

            if (!complete) {
                break;
            }

            if (index == 0) {
                firstSectorChecked = true;
                firstSectorMatches = std::memcmp(target, gateThreeSector.data.data(), NtrCard::sectorSize) == 0;
                if (!firstSectorMatches) {

                    complete = false;
                    break;
                }
            }

            if (!ramMode) {
                sha256_update(&context, reinterpret_cast<const BYTE*>(target), NtrCard::nandWindowSize);
                const u32 written = output->write(target, NtrCard::nandWindowSize);
                if (written != NtrCard::nandWindowSize) {
                    Logging::error("NTRCARD NAND dump failed to write window {} ({} of {} bytes, result 0x{:08X}).", index, written,
                        (u32)NtrCard::nandWindowSize, (u32)output->result());
                    complete = false;
                    break;
                }
            }
            done++;
            sink.advanceBytes(done * (u32)NtrCard::nandWindowSize);

            svcSleepThread(5'000'000);

            if (sink.cancelled()) {
                cancelled = true;
                complete  = false;
                break;
            }

        }

        const u32 cardElapsed = (u32)(osGetTime() - started);

        if (ramMode && !cancelled) {
            output = std::make_unique<FSStream>(Archive::sdmc(), path, FS_OPEN_WRITE, nandSave.saveSize);
            if (!output->good()) {
                Logging::error("NTRCARD NAND dump read {} windows but failed to open {} with result 0x{:08X}.", done, name, (u32)output->result());

                output.reset();
                complete = false;
            }
            else {
                for (u32 index = 0; index < done; index++) {
                    const u32* const source = image.get() + static_cast<std::size_t>(index) * NtrCard::nandWindowWords;
                    sha256_update(&context, reinterpret_cast<const BYTE*>(source), NtrCard::nandWindowSize);
                    const u32 written = output->write(source, NtrCard::nandWindowSize);
                    if (written != NtrCard::nandWindowSize) {
                        Logging::error("NTRCARD NAND dump failed to write window {} ({} of {} bytes, result 0x{:08X}).", index, written,
                            (u32)NtrCard::nandWindowSize, (u32)output->result());
                        complete = false;
                        break;
                    }
                }
            }
        }

        for (u32 index = 0; index < eventCount; index++) {
            logDumpEvent(events[index], windows);
        }
        if (eventsDropped != 0) {
            Logging::info("NTRCARD NAND dump recorded {} further bus events that did not fit the event buffer.", eventsDropped);
        }
        for (u32 index = 0; index < repairCount; index++) {
            const auto& repair = repairs[index];
            Logging::info("NTRCARD NAND stalled-controller repair {}: cleared={}, drained={}, transfer-irq-restored={}, MCNT 0x{:04X}->0x{:04X}, "
                          "ROMCNT 0x{:08X}->0x{:08X}.",
                index + 1, repair.cleared ? "yes" : "no", repair.drainedWords, repair.restoredTransferIrq ? "yes" : "no", repair.mCardControlBefore,
                repair.mCardControlAfter, repair.romControlBefore, repair.romControlAfter);
        }

        if (pollSyncFailed) {
            Logging::error("NTRCARD NAND poll synchronisation failed before window {}: status={}, ROMCNT=0x{:08X}, elapsed-ticks={}; no NAND "
                           "command was issued for that window.",
                failedPollWindow, cardPollStatusName(failedPollStatus), failedPollRomControl, failedPollElapsedTicks);
        }
        if (verificationMismatches != 0) {
            Logging::error("NTRCARD NAND verification mismatch at window {}, sector {}, byte +0x{:04X}; both reads completed and returned to ROM "
                           "mode, but their bytes differ. Dump stopped before accepting the burst.",
                mismatchWindow, mismatchSector, mismatchByte);
        }
        if (sentinelFailures != 0) {
            Logging::error("NTRCARD NAND sentinel read failed before window {}: status={}, stage={}, ROM-return-attempted={}, ROM-return={}; "
                           "dump stopped without starting that window.",
                failedSentinelWindow, sectorReadStatusName(failedSentinelStatus), nandReadStageName(failedSentinelStage),
                failedSentinelReturnAttempted ? "yes" : "no", sectorReadStatusName(failedSentinelReturn));
        }
        if (sentinelMismatches != 0) {
            Logging::error("NTRCARD NAND sentinel mismatch before window {} at byte +0x{:03X}; first save sector no longer matches the trusted "
                           "Gate-3 copy. Dump stopped without starting that window.",
                failedSentinelWindow, sentinelMismatchByte);
        }

        if (firstSectorChecked && !firstSectorMatches) {
            Logging::error("NTRCARD NAND dump stopped at window 0: its first sector differs from the Gate-3 sector read moments earlier, so the "
                           "cartridge is not returning what it returned then. Power the console off before another cartridge operation.");
        }
        if (cartridgeStateLost) {
            Logging::error("NTRCARD NAND dump left the cartridge in an unknown mode; power the console off before another attempt.");
        }
        const u32 unrecoveredLosses = resets;
        if (unrecoveredLosses > 0) {
            Logging::error("NTRCARD NAND dump saw {} card slot loss(es) it could not recover; power the console off before another cartridge "
                           "operation.",
                unrecoveredLosses);
        }

        const Result closeResult = output ? output->close() : 0;
        if (R_FAILED(closeResult)) {
            Logging::error("NTRCARD NAND dump failed to close {} with result 0x{:08X}.", name, (u32)closeResult);
            complete = false;
        }

        std::array<BYTE, SHA256_BLOCK_SIZE> digest{};
        sha256_final(&context, digest.data());
        const auto hex = hexDigest(digest);
        Logging::info("NTRCARD NAND dump: status={}, file={}, windows={}/{}, bytes={}, first-sector-match={}, collisions={}, resets={}, "
                      "contended={}, foreign-idle={}, drained-words={}, "
                      "repairs={}, poll-syncs={}, sentinel-checks={}, sentinel-failures={}, sentinel-mismatches={}, "
                      "verified-bursts={}, verification-mismatches={}, busy-after-recovery={}, card-elapsed={} ms, sha256={}.",
            cancelled ? "cancelled" : (complete ? "ok" : "failed"), name, done, windows, (u32)done * (u32)NtrCard::nandWindowSize,
            firstSectorMatches ? "yes" : "no", collisions, resets, contended, foreignIdle, drained, repairCount, pollSyncs, sentinelChecks,
            sentinelFailures, sentinelMismatches, verifiedBursts, verificationMismatches, busQuietWaits, cardElapsed, hex.data());

        const bool cartridgeUnsafe = cartridgeStateLost || (cardSlotUnavailable && unrecoveredLosses > 0);
        if (cartridgeUnsafe) {
            Logging::error("NTRCARD NAND dump Process9 recovery probe skipped because cartridge state is not safe for another operation.");
        }
        else {
            Result verifyResult          = 0;
            const bool nandProcess9Works = process9RecoveryOk(media, nandSave, verifyResult);
            Logging::info("NTRCARD NAND dump Process9 recovery probe: result=0x{:08X}, game-code-match={}.", (u32)verifyResult,
                nandProcess9Works ? "yes" : "no");
        }

        sink.finishFile();
        sink.end();

        outcome.windowsDone        = done;
        outcome.requiresPowerCycle = cartridgeUnsafe;
        if (cancelled) {
            outcome.status = DSCardNand::DumpStatus::Cancelled;
        }
        else if (complete) {
            outcome.status       = DSCardNand::DumpStatus::Ok;
            outcome.bytesWritten = done * (u32)NtrCard::nandWindowSize;
            std::memcpy(outcome.sha256, hex.data(), sizeof(outcome.sha256) - 1);
        }
        else {
            outcome.status = DSCardNand::DumpStatus::ReadFailed;
        }

        if (!outcome.ok() && io::fileExists(Archive::sdmc(), path)) {
            const Result removeResult = FSUSER_DeleteFile(Archive::sdmc(), fsMakePath(PATH_UTF16, path.data()));
            if (R_FAILED(removeResult)) {
                Logging::error("NTRCARD NAND dump could not remove the incomplete {} (result 0x{:08X}); delete it by hand before restoring "
                               "anything from this folder.",
                    name, (u32)removeResult);
            }
        }
        return outcome;
    }

}

bool DSCardNand::available(void)
{
    return NtrCard::mappedCardAccessAvailable();
}

DSCardNand::Session DSCardNand::prepare(FS_MediaType media, const DSCard::NandSave& nandSave, const u8* fsBanner)
{
    Session session;
    if (!nandSave.present || fsBanner == nullptr || !NtrCard::mappedCardAccessAvailable()) {
        return session;
    }

    bool initialPollReady = true;
    if (NtrCard::mappedCardAccessAvailable()) {
        const auto initialPoll = NtrCard::waitForMappedCardPoll();
        initialPollReady       = initialPoll && initialPoll->status == NtrCard::CardPollStatus::Observed;
        if (initialPoll) {
            Logging::info("NTRCARD initial poll synchronisation: status={}, ROMCNT=0x{:08X}, elapsed-ticks={}.",
                cardPollStatusName(initialPoll->status), initialPoll->observedRomControl, initialPoll->elapsedTicks);
        }
        else {
            Logging::error("NTRCARD initial poll synchronisation unavailable; no direct command issued.");
        }
    }
    if (!initialPollReady) {
        Logging::error("NTRCARD ROM read probe skipped because initial poll synchronisation failed; no direct command issued.");
        return session;
    }

    const auto probe = NtrCard::readMappedMainModeSector(nandSave.bannerOffset, nandSave.cardControl13);
    if (!probe) {
        Logging::error("NTRCARD ROM read probe unavailable; use the installed CIA.");
        return session;
    }

    const bool transferComplete = probe->status == NtrCard::SectorReadStatus::Ok;
    const auto* directBytes     = reinterpret_cast<const u8*>(probe->data.data());
    const bool bannerMatches    = transferComplete && std::memcmp(directBytes, fsBanner, NtrCard::sectorSize) == 0;
    Logging::info("NTRCARD ROM read probe: status={}, offset=0x{:08X}, words={}, MCNT initial=0x{:04X} observed=0x{:04X}, "
                  "ROMCNT before=0x{:08X} observed=0x{:08X}, command-readable={}, command-changed={}, "
                  "CMD expected=0x{:08X}{:08X} observed=0x{:08X}{:08X}, "
                  "transfer=0x{:08X}, complete=0x{:08X}, banner-match={}.",
        sectorReadStatusName(probe->status), nandSave.bannerOffset, probe->wordsRead, probe->initialMCardControl, probe->observedMCardControl,
        probe->initialRomControl, probe->observedRomControl, probe->commandReadbackAvailable ? "yes" : "no",
        probe->commandIdentityChanged ? "yes" : "no", probe->expectedCommandHigh, probe->expectedCommandLow, probe->observedCommandHigh,
        probe->observedCommandLow, probe->transferRomControl, probe->completedRomControl, bannerMatches ? "yes" : "no");

    if (transferComplete && !bannerMatches) {
        size_t mismatch = 0;
        while (mismatch < NtrCard::sectorSize && directBytes[mismatch] == fsBanner[mismatch]) {
            mismatch++;
        }
        Logging::error("NTRCARD ROM read differs from FS banner at +0x{:03X}: direct=0x{:02X}, FS=0x{:02X}.", mismatch, directBytes[mismatch],
            fsBanner[mismatch]);
    }

    bool process9Works = false;
    if (transferComplete) {
        Result verifyResult = 0;
        process9Works       = process9RecoveryOk(media, nandSave, verifyResult);
        Logging::info("NTRCARD Process9 recovery probe: result=0x{:08X}, game-code-match={}.", (u32)verifyResult, process9Works ? "yes" : "no");
    }

    const bool gateThreeAllowed =
        transferComplete && bannerMatches && process9Works && nandSave.saveSize >= NtrCard::sectorSize && NtrCard::mappedCardAccessAvailable();
    if (!gateThreeAllowed) {
        return session;
    }

    bool gateThreePollReady = true;
    if (NtrCard::mappedCardAccessAvailable()) {
        const auto gateThreePoll = NtrCard::waitForMappedCardPoll();
        gateThreePollReady       = gateThreePoll && gateThreePoll->status == NtrCard::CardPollStatus::Observed;
        if (gateThreePoll) {
            Logging::info("NTRCARD Gate-3 poll synchronisation: status={}, ROMCNT=0x{:08X}, elapsed-ticks={}.",
                cardPollStatusName(gateThreePoll->status), gateThreePoll->observedRomControl, gateThreePoll->elapsedTicks);
        }
        else {
            Logging::error("NTRCARD Gate-3 poll synchronisation unavailable; no NAND command issued.");
        }
    }
    if (!gateThreePollReady) {
        Logging::error("NTRCARD NAND read probe skipped because Gate-3 poll synchronisation failed; no NAND command issued.");
        return session;
    }

    const auto nandProbe = NtrCard::readMappedFirstNandSaveSector(nandSave.rwStart, nandSave.cardControl13);
    if (!nandProbe) {
        Logging::error("NTRCARD NAND read probe unavailable; use the installed CIA.");
        return session;
    }

    std::array<char, SHA256_BLOCK_SIZE * 2 + 1> digest{};
    const bool readComplete = nandProbe->status == NtrCard::SectorReadStatus::Ok;
    if (readComplete) {
        digest = sha256Hex(nandProbe->data.data(), NtrCard::sectorSize);
    }

    Logging::info("NTRCARD NAND read probe: status={}, stage={}, offset=0x{:08X}, words={}, ROM-return-attempted={}, ROM-return={}, "
                  "command-readable={}, command-changed={}, CMD expected=0x{:08X}{:08X} observed=0x{:08X}{:08X}, "
                  "select=0x{:08X}/0x{:08X}, read=0x{:08X}/0x{:08X}, return=0x{:08X}/0x{:08X}, sha256={}.",
        sectorReadStatusName(nandProbe->status), nandReadStageName(nandProbe->stage), nandSave.rwStart, nandProbe->wordsRead,
        nandProbe->returnToRomAttempted ? "yes" : "no", sectorReadStatusName(nandProbe->returnToRomStatus),
        nandProbe->commandReadbackAvailable ? "yes" : "no", nandProbe->commandIdentityChanged ? "yes" : "no", nandProbe->expectedCommandHigh,
        nandProbe->expectedCommandLow, nandProbe->observedCommandHigh, nandProbe->observedCommandLow, nandProbe->selectTransferRomControl,
        nandProbe->selectCompletedRomControl, nandProbe->readTransferRomControl, nandProbe->readCompletedRomControl,
        nandProbe->returnTransferRomControl, nandProbe->returnCompletedRomControl, readComplete ? digest.data() : "not-read");

    Result verifyResult          = 0;
    const bool nandProcess9Works = process9RecoveryOk(media, nandSave, verifyResult);
    Logging::info("NTRCARD NAND Process9 recovery probe: result=0x{:08X}, game-code-match={}.", (u32)verifyResult, nandProcess9Works ? "yes" : "no");

    session.sentinel = *nandProbe;
    session.ready = readComplete && nandProbe->stage == NtrCard::NandReadStage::Complete && nandProcess9Works && NtrCard::mappedCardAccessAvailable();
    session.requiresPowerCycle = !readComplete && nandProbe->stage != NtrCard::NandReadStage::Preflight &&
                                 !(nandProbe->returnToRomAttempted && nandProbe->returnToRomStatus == NtrCard::SectorReadStatus::Ok);
    return session;
}

DSCardNand::DumpOutcome DSCardNand::dumpToFile(
    FS_MediaType media, const DSCard::NandSave& nandSave, const Session& session, const std::u16string& path, ProgressSink& sink)
{
    DumpOutcome outcome;
    if (!available()) {
        outcome.status = DumpStatus::NotAvailable;
        return outcome;
    }
    if (!session.ready) {
        Logging::error("NTRCARD NAND dump refused: the gate chain that produces this launch's trusted sentinel did not pass.");
        outcome.status = DumpStatus::NotReady;
        return outcome;
    }

    const std::size_t leaf    = path.find_last_of(u'/');
    const std::u16string tail = leaf == std::u16string::npos ? path : path.substr(leaf + 1);
    return runDump(media, nandSave, session.sentinel, path, StringUtils::UTF16toUTF8(tail), sink);
}

namespace {
    DSCardNand::RestoreOutcome runRestore(FS_MediaType media, const DSCard::NandSave& nandSave, const NtrCard::NandSectorReadResult& gateThreeSector,
        const std::u16string& path, const std::string& name, ProgressSink& sink)
    {
        DSCardNand::RestoreOutcome outcome;

        if (nandSave.saveSize == 0 || (nandSave.saveSize % NtrCard::nandWindowSize) != 0) {
            Logging::error("NTRCARD NAND restore skipped: save size 0x{:08X} is not a whole number of 128-KiB windows.", nandSave.saveSize);
            outcome.status = DSCardNand::RestoreStatus::InvalidSaveSize;
            return outcome;
        }

        const u32 romControl = NtrCard::tunedRomControl(nandSave.cardControl13);
        const u32 windows    = nandSave.saveSize / NtrCard::nandWindowSize;

        std::unique_ptr<u32[]> image(new (std::nothrow) u32[nandSave.saveSize / sizeof(u32)]);
        if (!image) {
            Logging::error("NTRCARD NAND restore skipped: could not reserve 0x{:08X} bytes of RAM for the source image.", nandSave.saveSize);
            outcome.status = DSCardNand::RestoreStatus::OutOfMemory;
            return outcome;
        }

        {
            FSStream input(Archive::sdmc(), path, FS_OPEN_READ);
            if (!input.good()) {
                Logging::error("NTRCARD NAND restore skipped: failed to open {} with result 0x{:08X}.", name, (u32)input.result());
                outcome.status = DSCardNand::RestoreStatus::FileError;
                return outcome;
            }
            const u32 sourceSize = input.size();
            if (sourceSize != nandSave.saveSize) {
                Logging::error("NTRCARD NAND restore refused: {} is {} bytes, but this cartridge's save is {} bytes. A partial or foreign image "
                               "must never be written to on-cart NAND.",
                    name, sourceSize, nandSave.saveSize);
                input.close();
                outcome.status = DSCardNand::RestoreStatus::SourceSizeMismatch;
                return outcome;
            }
            const u32 readBytes     = input.read(image.get(), nandSave.saveSize);
            const Result readResult = input.result();
            input.close();
            if (readBytes != nandSave.saveSize || R_FAILED(readResult)) {
                Logging::error("NTRCARD NAND restore skipped: read {} of {} bytes from {} with result 0x{:08X}.", readBytes, nandSave.saveSize, name,
                    (u32)readResult);
                outcome.status = DSCardNand::RestoreStatus::FileError;
                return outcome;
            }
        }

        const auto sourceHex = sha256Hex(image.get(), nandSave.saveSize);
        std::memcpy(outcome.sourceSha256, sourceHex.data(), sizeof(outcome.sourceSha256) - 1);

        outcome.windows = windows;

        sink.begin("Restore", 1);
        sink.startFile(StringUtils::UTF8toUTF16(name.c_str()), nandSave.saveSize);

        Logging::info("NTRCARD NAND restore start: file={}, offset=0x{:08X}, size=0x{:08X}, windows={}, pages-per-window={}, "
                      "verification=read-back, ROMCNT=0x{:08X}, source sha256={}.",
            name, nandSave.rwStart, nandSave.saveSize, windows, (u32)(NtrCard::nandWindowSize / NtrCard::nandPageSize), romControl,
            outcome.sourceSha256);

        constexpr std::size_t pagesPerWindow = NtrCard::nandWindowSize / NtrCard::nandPageSize;
        static_assert(NtrCard::nandWindowSize % NtrCard::nandPageSize == 0);

        auto current        = std::make_unique<u32[]>(NtrCard::nandWindowWords);
        auto verify         = std::make_unique<u32[]>(NtrCard::nandWindowWords);
        auto readBack       = std::make_unique<u32[]>(NtrCard::nandPageWords);
        auto readBackVerify = std::make_unique<u32[]>(NtrCard::nandPageWords);

        u32 pollSyncs          = 0;
        u32 sentinelChecks     = 0;
        u32 sentinelFailures   = 0;
        u32 sentinelMismatches = 0;
        u32 verifiedWindows    = 0;
        u32 readMismatches     = 0;
        u32 commits            = 0;
        u32 statusPollsTotal   = 0;
        u32 statusPollsMax     = 0;
        u32 resets             = 0;

        constexpr u64 ticksPerMs       = SYSCLOCK_ARM11 / 1000;
        constexpr u64 writeBudgetTicks = 300 * ticksPerMs;

        constexpr u64 minimumReserveTicks = 120 * ticksPerMs;
        u64 slowestBatchedPageTicks       = 0;
        u32 batches                       = 0;
        u32 largestBatch                  = 0;
        bool cartridgeStateLost           = false;
        bool cancelled                    = false;
        bool complete                     = true;
        const char* failedStep            = nullptr;
        u32 failedWindow                  = 0;
        u32 failedPage                    = 0;

        const GuardedPage guarded{nandSave, gateThreeSector, romControl, "NAND restore"};

        const auto verifyTrustedSentinel = [&](void) {
            const auto sentinel = NtrCard::readMappedFirstNandSaveSector(nandSave.rwStart, romControl);
            sentinelChecks++;
            if (!sentinel || sentinel->status != NtrCard::SectorReadStatus::Ok || sentinel->stage != NtrCard::NandReadStage::Complete) {
                sentinelFailures++;
                if (sentinel) {
                    cartridgeStateLost = sentinel->stage != NtrCard::NandReadStage::Preflight &&
                                         !(sentinel->returnToRomAttempted && sentinel->returnToRomStatus == NtrCard::SectorReadStatus::Ok) &&
                                         sentinel->status != NtrCard::SectorReadStatus::ControllerReset;
                }
                return false;
            }
            if (std::memcmp(sentinel->data.data(), gateThreeSector.data.data(), NtrCard::sectorSize) != 0) {
                sentinelMismatches++;
                return false;
            }
            return true;
        };

        constexpr u32 sectorsPerBurst = 32;
        static_assert(NtrCard::nandWindowSectors % sectorsPerBurst == 0);

        const auto readWindow = [&](u32 window) {
            const auto sync = NtrCard::waitForMappedCardPoll();
            pollSyncs++;
            if (!sync || sync->status != NtrCard::CardPollStatus::Observed) {
                failedStep = "poll synchronisation";
                return false;
            }
            if (!verifyTrustedSentinel()) {
                failedStep = "sentinel before the compare read";
                return false;
            }

            for (u32 sector = 0; sector < NtrCard::nandWindowSectors; sector += sectorsPerBurst) {
                const std::size_t wordOffset = static_cast<std::size_t>(sector) * NtrCard::sectorWords;
                const auto first             = NtrCard::readMappedNandSaveRange(nandSave.rwStart, window, sector, sectorsPerBurst, 1, romControl,
                                current.get() + wordOffset, sectorsPerBurst * NtrCard::sectorWords);
                if (!first || first->status != NtrCard::SectorReadStatus::Ok || first->stage != NtrCard::NandReadStage::Complete) {
                    resets += first && first->status == NtrCard::SectorReadStatus::ControllerReset ? 1 : 0;
                    failedStep = "compare read";
                    return false;
                }
                if (!verifyTrustedSentinel()) {
                    failedStep = "sentinel between the compare reads";
                    return false;
                }
                const auto second = NtrCard::readMappedNandSaveRange(nandSave.rwStart, window, sector, sectorsPerBurst, 1, romControl,
                    verify.get() + wordOffset, sectorsPerBurst * NtrCard::sectorWords);
                if (!second || second->status != NtrCard::SectorReadStatus::Ok || second->stage != NtrCard::NandReadStage::Complete) {
                    resets += second && second->status == NtrCard::SectorReadStatus::ControllerReset ? 1 : 0;
                    failedStep = "compare re-read";
                    return false;
                }
                if (!verifyTrustedSentinel()) {
                    failedStep = "sentinel after the compare reads";
                    return false;
                }
                const std::size_t burstBytes = sectorsPerBurst * NtrCard::sectorSize;
                if (std::memcmp(current.get() + wordOffset, verify.get() + wordOffset, burstBytes) != 0) {
                    readMismatches++;
                    failedStep = "compare read disagreed with itself";
                    return false;
                }
            }

            verifiedWindows++;
            return true;
        };

        const auto writePage = [&](u32 window, u32 page, const u32* source, bool sentinelSurvives = true, bool ownInterval = true) {
            const u32 pageOffset  = nandSave.rwStart + window * (u32)NtrCard::nandWindowSize + page * (u32)NtrCard::nandPageSize;
            const u32 firstSector = page * (u32)NtrCard::nandPageSectors;

            const auto bracket = [&](const char* step) {
                if (ownInterval) {

                    sentinelChecks++;
                    pollSyncs++;
                    return guarded.syncedSentinel(step);
                }

                return verifyTrustedSentinel();
            };

            if (!bracket("the write")) {
                failedStep = ownInterval ? "poll-synchronised sentinel before the write" : "sentinel before the write";
                return false;
            }
            const auto written = NtrCard::writeMappedNandSavePage(nandSave.rwStart, pageOffset, romControl, source, NtrCard::nandPageWords);
            if (!written) {
                failedStep = "write unavailable";
                return false;
            }
            outcome.modified = outcome.modified || written->committed;
            commits += written->committed ? 1 : 0;
            statusPollsTotal += (u32)written->statusPolls;
            statusPollsMax = written->statusPolls > statusPollsMax ? (u32)written->statusPolls : statusPollsMax;
            resets += written->cartridgeWasReset ? 1 : 0;
            if (written->status != NtrCard::SectorReadStatus::Ok) {
                cartridgeStateLost = !written->recoveredToRomMode && !written->cartridgeWasReset;
                Logging::error("NTRCARD NAND restore write failed at page 0x{:08X}: status={}, stage={}, committed={}, status-polls={}, "
                               "status=0x{:08X}. {}",
                    pageOffset, sectorReadStatusName(written->status), (int)written->stage, written->committed ? "yes" : "no", written->statusPolls,
                    written->statusRegister,
                    written->committed ? "The commit had already been issued, so this page holds neither its old contents nor its new ones."
                                       : "The commit was never issued, so this page still holds its old contents.");
                failedStep = "write";
                return false;
            }

            const auto readOnce = [&](u32* into, const char* step) {
                if (sentinelSurvives && !bracket(step)) {
                    return false;
                }
                if (!sentinelSurvives && ownInterval) {

                    pollSyncs++;
                    const auto sync = NtrCard::waitForMappedCardPoll();
                    if (!sync || sync->status != NtrCard::CardPollStatus::Observed) {
                        return false;
                    }
                }
                const auto read = NtrCard::readMappedNandSaveRange(
                    nandSave.rwStart, window, firstSector, NtrCard::nandPageSectors, 1, romControl, into, NtrCard::nandPageWords);
                return read && read->status == NtrCard::SectorReadStatus::Ok && read->stage == NtrCard::NandReadStage::Complete;
            };
            if (!readOnce(readBack.get(), "the read-back") || !readOnce(readBackVerify.get(), "the read-back re-read")) {
                failedStep = "read-back";
                return false;
            }
            if (std::memcmp(readBack.get(), readBackVerify.get(), NtrCard::nandPageSize) != 0) {
                readMismatches++;
                failedStep = "read-back disagreed with itself";
                return false;
            }
            if (std::memcmp(readBack.get(), source, NtrCard::nandPageSize) != 0) {
                std::size_t byte     = 0;
                const auto* expected = reinterpret_cast<const u8*>(source);
                const auto* actual   = reinterpret_cast<const u8*>(readBack.get());
                while (byte < NtrCard::nandPageSize && expected[byte] == actual[byte]) {
                    byte++;
                }
                Logging::error("NTRCARD NAND restore read-back mismatch at page 0x{:08X} +0x{:03X}: wrote 0x{:02X}, read 0x{:02X}. The write "
                               "reported success, so this is the failure the read-back exists to catch.",
                    pageOffset, byte, expected[byte], actual[byte]);
                failedStep = "read-back comparison";
                return false;
            }
            outcome.pagesWritten++;
            return true;
        };

        const auto sourcePage = [&](u32 window, u32 page) {
            return image.get() + static_cast<std::size_t>(window) * NtrCard::nandWindowWords +
                   static_cast<std::size_t>(page) * NtrCard::nandPageWords;
        };

        const u64 started = osGetTime();

        constexpr u32 deferredPage = 0;
        bool deferredPageNeeded    = false;

        for (u32 window = 0; window < windows && complete; window++) {
            if (!readWindow(window)) {
                failedWindow = window;
                complete     = false;
                break;
            }

            std::array<u32, pagesPerWindow> pending{};
            u32 pendingCount = 0;
            for (std::size_t page = 0; page < pagesPerWindow; page++) {
                const std::size_t wordOffset = page * NtrCard::nandPageWords;
                const u32* const wanted      = image.get() + static_cast<std::size_t>(window) * NtrCard::nandWindowWords + wordOffset;
                const u32* const held        = current.get() + wordOffset;
                if (std::memcmp(wanted, held, NtrCard::nandPageSize) == 0) {
                    outcome.pagesSkipped++;
                    continue;
                }
                if (window == 0 && page == deferredPage) {
                    deferredPageNeeded = true;
                    continue;
                }
                pending[pendingCount++] = (u32)page;
            }

            for (u32 index = 0; index < pendingCount && complete;) {
                pollSyncs++;
                const auto sync = NtrCard::waitForMappedCardPoll();
                if (!sync || sync->status != NtrCard::CardPollStatus::Observed) {
                    failedStep   = "poll synchronisation before a write batch";
                    failedWindow = window;
                    failedPage   = pending[index];
                    complete     = false;
                    break;
                }

                const u64 batchStarted = svcGetSystemTick();
                u32 batched            = 0;
                while (index < pendingCount) {

                    const u64 elapsed = svcGetSystemTick() - batchStarted;
                    const u64 reserve = slowestBatchedPageTicks > minimumReserveTicks ? slowestBatchedPageTicks : minimumReserveTicks;
                    if (batched != 0 && elapsed + reserve > writeBudgetTicks) {
                        break;
                    }

                    const u64 pageStarted = svcGetSystemTick();
                    if (!writePage(window, pending[index], sourcePage(window, pending[index]), true, false)) {
                        failedWindow = window;
                        failedPage   = pending[index];
                        complete     = false;
                        break;
                    }
                    const u64 pageTook      = svcGetSystemTick() - pageStarted;
                    slowestBatchedPageTicks = pageTook > slowestBatchedPageTicks ? pageTook : slowestBatchedPageTicks;
                    index++;
                    batched++;
                }
                if (batched > largestBatch) {
                    largestBatch = batched;
                }
                batches += batched != 0 ? 1 : 0;
            }

            if (!complete) {
                break;
            }
            outcome.windowsDone++;
            sink.advanceBytes(outcome.windowsDone * (u32)NtrCard::nandWindowSize);

            svcSleepThread(5'000'000);
            if (sink.cancelled()) {
                cancelled = true;
                complete  = false;
                break;
            }
        }

        bool deferredPageWritten = false;
        if (complete && deferredPageNeeded) {
            if (!writePage(0, deferredPage, image.get(), false)) {
                failedPage = deferredPage;
                complete   = false;
            }
            else {
                deferredPageWritten = true;
            }
        }

        const u32 cardElapsed = (u32)(osGetTime() - started);

        if (sentinelFailures != 0) {
            Logging::error("NTRCARD NAND restore: a sentinel read failed at window {}, page {}; the cartridge stopped answering as it did at "
                           "Gate 3.",
                failedWindow, failedPage);
        }
        if (sentinelMismatches != 0) {
            Logging::error("NTRCARD NAND restore: the first save sector stopped matching the trusted Gate-3 copy at window {}, page {}. Nothing "
                           "in this restore should have changed it before the very last write.",
                failedWindow, failedPage);
        }
        if (cartridgeStateLost) {
            Logging::error("NTRCARD NAND restore left the cartridge in an unknown mode; power the console off before another attempt.");
        }

        Logging::info("NTRCARD NAND restore: status={}, file={}, windows={}/{}, pages-written={}, pages-skipped={}, first-page-written={}, "
                      "commits={}, status-polls total={} max={}, poll-syncs={}, sentinel-checks={}, sentinel-failures={}, "
                      "sentinel-mismatches={}, verified-windows={}, compare-mismatches={}, resets={}, batches={}, largest-batch={}, "
                      "card-elapsed={} ms, source sha256={}.",
            cancelled ? "cancelled" : (complete ? "ok" : "failed"), name, outcome.windowsDone, windows, outcome.pagesWritten, outcome.pagesSkipped,
            deferredPageWritten ? "yes" : (deferredPageNeeded ? "no" : "not-needed"), commits, statusPollsTotal, statusPollsMax, pollSyncs,
            sentinelChecks, sentinelFailures, sentinelMismatches, verifiedWindows, readMismatches, resets, batches, largestBatch, cardElapsed,
            outcome.sourceSha256);

        if (!complete && !cancelled) {
            Logging::error("NTRCARD NAND restore stopped at window {}, page {} ({}). {}", failedWindow, failedPage,
                failedStep != nullptr ? failedStep : "unknown step",
                outcome.modified ? "Pages were already committed, so the cartridge now holds a mixture of its old save and the source image. "
                                   "Run this restore again, or restore a known-good backup, before using the cartridge."
                                 : "No page was ever committed, so the cartridge still holds exactly what it held before.");
        }

        outcome.requiresPowerCycle = cartridgeStateLost;
        if (cancelled) {
            outcome.status = DSCardNand::RestoreStatus::Cancelled;
            if (outcome.modified) {
                Logging::error("NTRCARD NAND restore was cancelled after {} page(s) had been committed, so the cartridge holds a mixture of its "
                               "old save and the source image. Run the restore again, or restore a known-good backup, before using it.",
                    outcome.pagesWritten);
            }
        }
        else if (complete) {
            outcome.status = DSCardNand::RestoreStatus::Ok;
            Logging::info(
                "NTRCARD NAND restore of {} completed: {} page(s) written, {} already matched.", name, outcome.pagesWritten, outcome.pagesSkipped);
        }
        else if (failedStep != nullptr && std::strcmp(failedStep, "write") == 0) {
            outcome.status = DSCardNand::RestoreStatus::WriteFailed;
        }
        else {
            outcome.status = DSCardNand::RestoreStatus::ReadFailed;
        }

        Result verifyResult      = 0;
        const bool process9Works = process9RecoveryOk(media, nandSave, verifyResult);
        Logging::info(
            "NTRCARD NAND restore Process9 recovery probe: result=0x{:08X}, game-code-match={}.", (u32)verifyResult, process9Works ? "yes" : "no");

        sink.finishFile();
        sink.end();

        return outcome;
    }
}

DSCardNand::RestoreOutcome DSCardNand::restoreFromFile(
    FS_MediaType media, const DSCard::NandSave& nandSave, const Session& session, const std::u16string& path, ProgressSink& sink)
{
    RestoreOutcome outcome;
    if (!available()) {
        outcome.status = RestoreStatus::NotAvailable;
        return outcome;
    }
    if (!session.ready) {
        Logging::error("NTRCARD NAND restore refused: the gate chain that produces this launch's trusted sentinel did not pass.");
        outcome.status = RestoreStatus::NotReady;
        return outcome;
    }

    const std::size_t leaf    = path.find_last_of(u'/');
    const std::u16string tail = leaf == std::u16string::npos ? path : path.substr(leaf + 1);
    return runRestore(media, nandSave, session.sentinel, path, StringUtils::UTF16toUTF8(tail), sink);
}
