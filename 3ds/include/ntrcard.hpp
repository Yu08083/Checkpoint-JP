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
 */

#ifndef NTRCARD_HPP
#define NTRCARD_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace NtrCard {

    inline constexpr std::uintptr_t physicalRegisterBase = 0x10164000;
    inline constexpr std::uintptr_t processRegisterBase  = 0x1EC64000;
    inline constexpr std::size_t sectorSize              = 0x200;
    inline constexpr std::size_t nandWindowSize          = 128 * 1024;
    inline constexpr std::size_t sectorWords             = sectorSize / sizeof(std::uint32_t);
    inline constexpr std::size_t nandWindowSectors       = nandWindowSize / sectorSize;
    inline constexpr std::size_t nandWindowWords         = nandWindowSize / sizeof(std::uint32_t);

    inline constexpr std::size_t nandPageSize    = 0x800;
    inline constexpr std::size_t nandPageWords   = nandPageSize / sizeof(std::uint32_t);
    inline constexpr std::size_t nandPageSectors = nandPageSize / sectorSize;

    inline constexpr std::size_t maxNandBlockSectors = 8;

    enum class RegisterOffset : std::uintptr_t {
        MCardControl = 0x00,
        RomControl   = 0x04,

        CommandHigh = 0x08,
        CommandLow  = 0x0C,
        SeedXLow    = 0x10,
        SeedYLow    = 0x14,
        SeedXHigh   = 0x18,
        SeedYHigh   = 0x1A,
        Fifo        = 0x1C,
    };

    struct RegisterSnapshot {
        std::uint16_t mCardControl;
        std::uint32_t romControl;
        std::uint32_t commandHigh;
        std::uint32_t commandLow;
        std::uint32_t seedXLow;
        std::uint32_t seedYLow;
        std::uint16_t seedXHigh;
        std::uint16_t seedYHigh;
    };

    class RegisterReader {
    public:
        virtual ~RegisterReader()                                 = default;
        virtual std::uint16_t read16(RegisterOffset offset) const = 0;
        virtual std::uint32_t read32(RegisterOffset offset) const = 0;
    };

    class CardBus : public RegisterReader {
    public:
        virtual std::array<std::uint8_t, 8> readCommand(void) const           = 0;
        virtual void write16(RegisterOffset offset, std::uint16_t value)      = 0;
        virtual void write32(RegisterOffset offset, std::uint32_t value)      = 0;
        virtual void writeCommand(const std::array<std::uint8_t, 8>& command) = 0;
        virtual std::uint64_t ticks(void) const                               = 0;
    };

    enum class SectorReadStatus {
        Ok,
        InvalidArgument,
        ControllerDisabled,
        ControllerBusy,

        ControllerStateChanged,

        ControllerReset,
        Timeout,
        WordCountMismatch,
    };

    struct SectorReadResult {
        SectorReadStatus status = SectorReadStatus::InvalidArgument;
        std::array<std::uint32_t, sectorSize / sizeof(std::uint32_t)> data{};
        std::size_t wordsRead = 0;

        std::size_t drainedWords           = 0;
        std::uint16_t initialMCardControl  = 0;
        std::uint32_t initialRomControl    = 0;
        std::uint16_t observedMCardControl = 0;
        std::uint32_t observedRomControl   = 0;
        bool commandReadbackAvailable      = false;
        bool commandIdentityChanged        = false;
        std::uint32_t expectedCommandHigh  = 0;
        std::uint32_t expectedCommandLow   = 0;
        std::uint32_t observedCommandHigh  = 0;
        std::uint32_t observedCommandLow   = 0;
        std::uint32_t transferRomControl   = 0;
        std::uint32_t completedRomControl  = 0;
    };

    enum class NandReadStage {
        Preflight,
        SelectWindow,
        ReadSector,
        ReturnToRom,
        Complete,
    };

    struct NandSectorReadResult {
        SectorReadStatus status            = SectorReadStatus::InvalidArgument;
        NandReadStage stage                = NandReadStage::Preflight;
        SectorReadStatus returnToRomStatus = SectorReadStatus::InvalidArgument;
        bool returnToRomAttempted          = false;
        std::array<std::uint32_t, sectorSize / sizeof(std::uint32_t)> data{};
        std::size_t wordsRead                   = 0;
        std::size_t drainedWords                = 0;
        std::uint16_t initialMCardControl       = 0;
        std::uint32_t initialRomControl         = 0;
        bool commandReadbackAvailable           = false;
        bool commandIdentityChanged             = false;
        std::uint32_t expectedCommandHigh       = 0;
        std::uint32_t expectedCommandLow        = 0;
        std::uint32_t observedCommandHigh       = 0;
        std::uint32_t observedCommandLow        = 0;
        std::uint32_t selectTransferRomControl  = 0;
        std::uint32_t selectCompletedRomControl = 0;
        std::uint32_t readTransferRomControl    = 0;
        std::uint32_t readCompletedRomControl   = 0;
        std::uint32_t returnTransferRomControl  = 0;
        std::uint32_t returnCompletedRomControl = 0;
    };

    struct NandRangeReadResult {
        SectorReadStatus status            = SectorReadStatus::InvalidArgument;
        NandReadStage stage                = NandReadStage::Preflight;
        SectorReadStatus returnToRomStatus = SectorReadStatus::InvalidArgument;
        bool returnToRomAttempted          = false;

        bool recoveredToRomMode   = false;
        bool cartridgeWasReset    = false;
        std::uint32_t windowBase  = 0;
        std::uint32_t firstSector = 0;
        std::size_t sectorsRead   = 0;
        std::size_t wordsRead     = 0;

        std::size_t blockSectors = 1;
        std::size_t drainedWords = 0;

        bool idleConfigDiffers             = false;
        std::uint16_t initialMCardControl  = 0;
        std::uint32_t initialRomControl    = 0;
        std::uint16_t observedMCardControl = 0;
        std::uint32_t observedRomControl   = 0;

        bool commandReadbackAvailable           = false;
        bool commandIdentityChanged             = false;
        std::uint32_t expectedCommandHigh       = 0;
        std::uint32_t expectedCommandLow        = 0;
        std::uint32_t observedCommandHigh       = 0;
        std::uint32_t observedCommandLow        = 0;
        std::uint32_t selectTransferRomControl  = 0;
        std::uint32_t selectCompletedRomControl = 0;
        std::uint32_t readTransferRomControl    = 0;
        std::uint32_t readCompletedRomControl   = 0;
        std::uint32_t returnTransferRomControl  = 0;
        std::uint32_t returnCompletedRomControl = 0;
    };

    enum class NandWriteStage {
        Preflight,
        SelectWindow,
        WriteEnable,
        WriteBuffer,
        CommitBuffer,
        AwaitReady,
        DiscardBuffer,
        ReturnToRom,
        Complete,
    };

    struct NandPageWriteResult {
        SectorReadStatus status            = SectorReadStatus::InvalidArgument;
        NandWriteStage stage               = NandWriteStage::Preflight;
        SectorReadStatus returnToRomStatus = SectorReadStatus::InvalidArgument;
        bool returnToRomAttempted          = false;
        bool recoveredToRomMode            = false;
        bool cartridgeWasReset             = false;

        bool committed           = false;
        bool discardAttempted    = false;
        std::uint32_t pageOffset = 0;
        std::size_t wordsWritten = 0;

        std::size_t flushedWords          = 0;
        std::size_t statusPolls           = 0;
        std::uint32_t statusRegister      = 0;
        bool commandReadbackAvailable     = false;
        bool commandIdentityChanged       = false;
        std::uint32_t expectedCommandHigh = 0;
        std::uint32_t expectedCommandLow  = 0;
        std::uint32_t observedCommandHigh = 0;
        std::uint32_t observedCommandLow  = 0;
        std::uint16_t initialMCardControl = 0;
        std::uint32_t initialRomControl   = 0;
    };

    struct StalledControllerRecovery {
        bool attempted                   = false;
        bool cleared                     = false;
        bool restoredTransferIrq         = false;
        std::size_t drainedWords         = 0;
        std::uint16_t mCardControlBefore = 0;
        std::uint16_t mCardControlAfter  = 0;
        std::uint32_t romControlBefore   = 0;
        std::uint32_t romControlAfter    = 0;
    };

    enum class CardPollStatus {
        Observed,
        Timeout,
        ControllerDisabled,
        ControllerReset,
    };

    struct CardPollResult {
        CardPollStatus status            = CardPollStatus::Timeout;
        std::uint32_t observedRomControl = 0;
        std::uint64_t elapsedTicks       = 0;
    };

    RegisterSnapshot capture(const RegisterReader& reader);

    bool process9ControllerReady(const RegisterSnapshot& snapshot);

    SectorReadResult readMainModeSector(CardBus& bus, std::uint32_t offset, std::uint32_t normalRomControl, std::uint64_t timeoutTicks);

    NandSectorReadResult readFirstNandSaveSector(CardBus& bus, std::uint32_t rwStart, std::uint32_t normalRomControl, std::uint64_t timeoutTicks);

    NandRangeReadResult readNandSaveRange(CardBus& bus, std::uint32_t rwStart, std::uint32_t windowIndex, std::uint32_t firstSector,
        std::size_t sectorCount, std::size_t blockSectors, std::uint32_t normalRomControl, std::uint64_t timeoutTicks, std::uint32_t* output,
        std::size_t outputWords);

    StalledControllerRecovery recoverStalledController(CardBus& bus, std::uint64_t timeoutTicks);

    NandPageWriteResult writeNandSavePage(CardBus& bus, std::uint32_t rwStart, std::uint32_t pageOffset, std::uint32_t normalRomControl,
        std::uint64_t timeoutTicks, const std::uint32_t* page, std::size_t pageWords);

    bool waitForBusQuiet(CardBus& bus, std::uint64_t quietTicks, std::uint64_t timeoutTicks);

    CardPollResult waitForCardPoll(CardBus& bus, std::uint64_t quietTicks, std::uint64_t timeoutTicks);

    bool mappedCardAccessAvailable(void);
    std::optional<RegisterSnapshot> captureMapped(void);
    std::optional<SectorReadResult> readMappedMainModeSector(std::uint32_t offset, std::uint32_t normalRomControl);
    std::optional<NandSectorReadResult> readMappedFirstNandSaveSector(std::uint32_t rwStart, std::uint32_t normalRomControl);
    std::optional<NandRangeReadResult> readMappedNandSaveRange(std::uint32_t rwStart, std::uint32_t windowIndex, std::uint32_t firstSector,
        std::size_t sectorCount, std::size_t blockSectors, std::uint32_t normalRomControl, std::uint32_t* output, std::size_t outputWords);
    std::optional<StalledControllerRecovery> recoverMappedStalledController(void);
    std::optional<NandPageWriteResult> writeMappedNandSavePage(
        std::uint32_t rwStart, std::uint32_t pageOffset, std::uint32_t normalRomControl, const std::uint32_t* page, std::size_t pageWords);

    bool waitForMappedBusQuiet(void);
    std::optional<CardPollResult> waitForMappedCardPoll(void);

    std::uint32_t tunedRomControl(std::uint32_t headerRomControl);
}

#endif
