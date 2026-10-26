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

#include "ntrcard.hpp"

#ifdef __3DS__
#include <3ds.h>
#endif

namespace {
    constexpr std::uint16_t mCardEnable          = 1u << 15;
    constexpr std::uint16_t mCardTransferIrq     = 1u << 14;
    constexpr std::uint32_t romStart             = 1u << 31;
    constexpr std::uint32_t romWrite             = 1u << 30;
    constexpr std::uint32_t romNoReset           = 1u << 29;
    constexpr std::uint32_t romBlockSizeMask     = 7u << 24;
    constexpr std::uint32_t romSectorBlockSize   = 1u << 24;
    constexpr std::uint32_t romDataReady         = 1u << 23;
    constexpr std::uint32_t romSeedApply         = 1u << 15;
    constexpr std::uint32_t romVolatileStatus    = romStart | romDataReady;
    constexpr std::uint32_t romTransferFixedBits = romWrite | romBlockSizeMask | romSeedApply | romVolatileStatus;
    constexpr std::uint8_t mainReadCommand       = 0xB7;
    constexpr std::uint8_t nandRomModeCommand    = 0x8B;
    constexpr std::uint8_t nandRwModeCommand     = 0xB2;

    constexpr std::uint8_t nandWriteEnableCommand   = 0x85;
    constexpr std::uint8_t nandWriteBufferCommand   = 0x81;
    constexpr std::uint8_t nandCommitBufferCommand  = 0x82;
    constexpr std::uint8_t nandDiscardBufferCommand = 0x84;
    constexpr std::uint8_t nandReadStatusCommand    = 0xD6;

    constexpr std::uint32_t nandStatusReady = 1u << 5;
    constexpr std::uint32_t mainDataMinimum = 0x8000;

    constexpr std::size_t maxDrainWords = (0x100u << 6) / sizeof(std::uint32_t);

    constexpr std::uint64_t cleanupTimeoutMultiplier = 20;
    constexpr unsigned cleanupAttempts               = 4;

    constexpr std::uint32_t blockSizeField(std::size_t sectors)
    {
        std::uint32_t field = 1;
        for (std::size_t size = 1; size < sectors; size *= 2) {
            field++;
        }
        return field << 24;
    }

    constexpr bool validBlockSectors(std::size_t sectors)
    {
        return sectors >= 1 && sectors <= NtrCard::maxNandBlockSectors && (sectors & (sectors - 1)) == 0;
    }

    struct CommandResult {
        NtrCard::SectorReadStatus status  = NtrCard::SectorReadStatus::InvalidArgument;
        std::size_t wordsRead             = 0;
        std::uint32_t transferControl     = 0;
        std::uint32_t completedControl    = 0;
        bool transferStarted              = false;
        bool commandReadbackAvailable     = false;
        bool commandIdentityChanged       = false;
        std::uint32_t expectedCommandHigh = 0;
        std::uint32_t expectedCommandLow  = 0;

        std::uint16_t observedMCardControl = 0;
        std::uint32_t observedRomControl   = 0;
        std::uint32_t observedCommandHigh  = 0;
        std::uint32_t observedCommandLow   = 0;

        std::size_t drainedWords = 0;
    };

    std::size_t drainAbandonedTransfer(NtrCard::CardBus& bus, std::uint32_t transferControl, std::uint64_t timeoutTicks)
    {
        constexpr std::uint32_t ownership = ~(romVolatileStatus | romNoReset);
        const std::uint64_t started       = bus.ticks();
        std::size_t drained               = 0;
        for (;;) {
            const std::uint32_t control = bus.read32(NtrCard::RegisterOffset::RomControl);
            if ((control & romStart) == 0) {
                break;
            }

            if ((control & ownership) != (transferControl & ownership)) {
                break;
            }
            if ((control & romDataReady) != 0) {
                (void)bus.read32(NtrCard::RegisterOffset::Fifo);
                drained++;
            }
            if (drained >= maxDrainWords || bus.ticks() - started >= timeoutTicks) {
                break;
            }
        }
        return drained;
    }

    class MappedCardBus final : public NtrCard::CardBus {
    public:
        std::uint16_t read16(NtrCard::RegisterOffset offset) const override
        {
            const auto address = NtrCard::processRegisterBase + static_cast<std::uintptr_t>(offset);
            return *reinterpret_cast<volatile const std::uint16_t*>(address);
        }

        std::uint32_t read32(NtrCard::RegisterOffset offset) const override
        {
            const auto address = NtrCard::processRegisterBase + static_cast<std::uintptr_t>(offset);
            return *reinterpret_cast<volatile const std::uint32_t*>(address);
        }

        std::array<std::uint8_t, 8> readCommand(void) const override
        {
            std::array<std::uint8_t, 8> command{};
            auto* const input = reinterpret_cast<volatile const std::uint8_t*>(
                NtrCard::processRegisterBase + static_cast<std::uintptr_t>(NtrCard::RegisterOffset::CommandHigh));
            for (std::size_t i = 0; i < command.size(); i++) {
                command[i] = input[i];
            }
            return command;
        }

        void write16(NtrCard::RegisterOffset offset, std::uint16_t value) override
        {
            const auto address                                  = NtrCard::processRegisterBase + static_cast<std::uintptr_t>(offset);
            *reinterpret_cast<volatile std::uint16_t*>(address) = value;
        }

        void write32(NtrCard::RegisterOffset offset, std::uint32_t value) override
        {
            const auto address                                  = NtrCard::processRegisterBase + static_cast<std::uintptr_t>(offset);
            *reinterpret_cast<volatile std::uint32_t*>(address) = value;
        }

        void writeCommand(const std::array<std::uint8_t, 8>& command) override
        {
            auto* const output = reinterpret_cast<volatile std::uint8_t*>(
                NtrCard::processRegisterBase + static_cast<std::uintptr_t>(NtrCard::RegisterOffset::CommandHigh));
            for (std::size_t i = 0; i < command.size(); i++) {
                output[i] = command[i];
            }
        }

        std::uint64_t ticks(void) const override
        {
#ifdef __3DS__
            return svcGetSystemTick();
#else
            return 0;
#endif
        }
    };

    class ControllerStateRestorer {
    public:
        ControllerStateRestorer(
            NtrCard::CardBus& bus, std::uint16_t mCardControl, std::uint32_t romControl, std::uint32_t commandHigh, std::uint32_t commandLow)
            : mBus(bus), mMCardControl(mCardControl), mRomControl(romControl & ~romVolatileStatus), mCommandHigh(commandHigh), mCommandLow(commandLow)
        {
        }

        void noteMCardControlWritten(std::uint16_t written)
        {
            mWrittenMCardControl = written;
            mMCardControlWritten = true;
        }

        ~ControllerStateRestorer()
        {
            if (mRestoreRegisters) {

                mBus.write32(NtrCard::RegisterOffset::RomControl, mRomControl);
                mBus.write32(NtrCard::RegisterOffset::CommandHigh, mCommandHigh);
                mBus.write32(NtrCard::RegisterOffset::CommandLow, mCommandLow);
            }

            if (mMCardControlWritten && mBus.read16(NtrCard::RegisterOffset::MCardControl) == mWrittenMCardControl) {
                mBus.write16(NtrCard::RegisterOffset::MCardControl, mMCardControl);
            }
        }

        void abandon(void) { mRestoreRegisters = false; }

    private:
        NtrCard::CardBus& mBus;
        std::uint16_t mMCardControl;
        std::uint32_t mRomControl;
        std::uint32_t mCommandHigh;
        std::uint32_t mCommandLow;
        std::uint16_t mWrittenMCardControl = 0;
        bool mMCardControlWritten          = false;
        bool mRestoreRegisters             = true;
    };

    NtrCard::SectorReadStatus preflightController(
        NtrCard::CardBus& bus, std::uint32_t normalRomControl, std::uint16_t& mCardControl, std::uint32_t& romControl, bool& idleConfigDiffers)
    {
        mCardControl = bus.read16(NtrCard::RegisterOffset::MCardControl);
        romControl   = bus.read32(NtrCard::RegisterOffset::RomControl);
        if ((mCardControl & mCardEnable) == 0) {
            return NtrCard::SectorReadStatus::ControllerDisabled;
        }
        if ((romControl & romStart) != 0) {
            return NtrCard::SectorReadStatus::ControllerBusy;
        }
        if ((romControl & romNoReset) == 0) {
            return NtrCard::SectorReadStatus::ControllerReset;
        }

        const std::uint32_t expectedIdleControl = (normalRomControl & ~romTransferFixedBits) | romNoReset;
        idleConfigDiffers                       = (romControl & ~romTransferFixedBits) != expectedIdleControl;
        return NtrCard::SectorReadStatus::Ok;
    }

    std::array<std::uint8_t, 8> commandWithParameter(std::uint8_t command, std::uint32_t parameter)
    {
        return {
            command,
            static_cast<std::uint8_t>(parameter >> 24),
            static_cast<std::uint8_t>(parameter >> 16),
            static_cast<std::uint8_t>(parameter >> 8),
            static_cast<std::uint8_t>(parameter),
            0,
            0,
            0,
        };
    }

    std::uint32_t packedCommandWord(const std::array<std::uint8_t, 8>& command, std::size_t offset)
    {
        return static_cast<std::uint32_t>(command[offset]) | (static_cast<std::uint32_t>(command[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(command[offset + 2]) << 16) | (static_cast<std::uint32_t>(command[offset + 3]) << 24);
    }

    struct NandRangeResult {
        NtrCard::SectorReadStatus status            = NtrCard::SectorReadStatus::InvalidArgument;
        NtrCard::NandReadStage stage                = NtrCard::NandReadStage::Preflight;
        NtrCard::SectorReadStatus returnToRomStatus = NtrCard::SectorReadStatus::InvalidArgument;
        bool returnToRomAttempted                   = false;
        bool recoveredToRomMode                     = false;
        bool cartridgeWasReset                      = false;
        std::uint32_t windowBase                    = 0;
        std::uint32_t firstSector                   = 0;
        std::uint16_t observedMCardControl          = 0;
        std::uint32_t observedRomControl            = 0;
        std::uint32_t observedCommandHigh           = 0;
        std::uint32_t observedCommandLow            = 0;
        bool commandReadbackAvailable               = false;
        bool commandIdentityChanged                 = false;
        std::uint32_t expectedCommandHigh           = 0;
        std::uint32_t expectedCommandLow            = 0;
        std::size_t sectorsRead                     = 0;
        std::size_t wordsRead                       = 0;
        std::size_t blockSectors                    = 1;
        std::size_t drainedWords                    = 0;
        bool idleConfigDiffers                      = false;
        std::uint16_t initialMCardControl           = 0;
        std::uint32_t initialRomControl             = 0;
        std::uint32_t selectTransferRomControl      = 0;
        std::uint32_t selectCompletedRomControl     = 0;
        std::uint32_t readTransferRomControl        = 0;
        std::uint32_t readCompletedRomControl       = 0;
        std::uint32_t returnTransferRomControl      = 0;
        std::uint32_t returnCompletedRomControl     = 0;
    };

    CommandResult runPolledCommand(NtrCard::CardBus& bus, std::uint8_t command, std::uint32_t parameter, std::uint32_t normalRomControl,
        std::uint32_t blockSize, std::uint16_t expectedMCardControl, std::uint32_t* output, std::size_t expectedWords, std::uint64_t timeoutTicks)
    {
        CommandResult result;
        const auto commandBytes     = commandWithParameter(command, parameter);
        result.expectedCommandHigh  = packedCommandWord(commandBytes, 0);
        result.expectedCommandLow   = packedCommandWord(commandBytes, 4);
        result.observedRomControl   = bus.read32(NtrCard::RegisterOffset::RomControl);
        result.observedMCardControl = bus.read16(NtrCard::RegisterOffset::MCardControl);
        if ((result.observedRomControl & romStart) != 0 || result.observedMCardControl != expectedMCardControl) {
            result.status = NtrCard::SectorReadStatus::ControllerStateChanged;
            return result;
        }

        bus.writeCommand(commandBytes);

        const auto commandReadback = bus.readCommand();
        result.observedCommandHigh = packedCommandWord(commandReadback, 0);
        result.observedCommandLow  = packedCommandWord(commandReadback, 4);
        bool commandReadbackIsZero = true;
        for (const std::uint8_t byte : commandReadback) {
            commandReadbackIsZero = commandReadbackIsZero && byte == 0;
        }
        result.commandReadbackAvailable = !commandReadbackIsZero;
        if (result.commandReadbackAvailable &&
            (result.observedCommandHigh != result.expectedCommandHigh || result.observedCommandLow != result.expectedCommandLow)) {
            result.commandIdentityChanged = true;
            result.status                 = NtrCard::SectorReadStatus::ControllerStateChanged;
            return result;
        }

        result.transferControl = (normalRomControl & ~romTransferFixedBits) | romNoReset | blockSize | romStart;
        bus.write32(NtrCard::RegisterOffset::RomControl, result.transferControl);
        result.transferStarted = true;

        const std::uint64_t started = bus.ticks();
        for (;;) {
            const std::uint16_t mCardControl = bus.read16(NtrCard::RegisterOffset::MCardControl);
            if (mCardControl != expectedMCardControl) {

                result.observedMCardControl = mCardControl;
                result.observedRomControl   = bus.read32(NtrCard::RegisterOffset::RomControl);
                result.status               = NtrCard::SectorReadStatus::ControllerStateChanged;
                result.drainedWords         = drainAbandonedTransfer(bus, result.transferControl, timeoutTicks);
                return result;
            }

            const std::uint32_t control = bus.read32(NtrCard::RegisterOffset::RomControl);
            result.completedControl     = control;
            if ((control & ~romVolatileStatus) != (result.transferControl & ~romVolatileStatus)) {
                result.observedMCardControl = mCardControl;
                result.observedRomControl   = control;

                result.status =
                    (control & romNoReset) == 0 ? NtrCard::SectorReadStatus::ControllerReset : NtrCard::SectorReadStatus::ControllerStateChanged;
                result.drainedWords = drainAbandonedTransfer(bus, result.transferControl, timeoutTicks);
                return result;
            }

            if (result.commandReadbackAvailable && ((control & romStart) == 0 || ((control & romDataReady) != 0 && (result.wordsRead % 16) == 0))) {
                const auto currentCommand       = bus.readCommand();
                const std::uint32_t commandHigh = packedCommandWord(currentCommand, 0);
                const std::uint32_t commandLow  = packedCommandWord(currentCommand, 4);
                if (commandHigh != result.expectedCommandHigh || commandLow != result.expectedCommandLow) {
                    result.observedMCardControl   = mCardControl;
                    result.observedRomControl     = control;
                    result.observedCommandHigh    = commandHigh;
                    result.observedCommandLow     = commandLow;
                    result.commandIdentityChanged = true;
                    result.status                 = NtrCard::SectorReadStatus::ControllerStateChanged;

                    return result;
                }
            }

            if ((control & romDataReady) != 0) {
                const std::uint32_t word = bus.read32(NtrCard::RegisterOffset::Fifo);
                if (output != nullptr && result.wordsRead < expectedWords) {
                    output[result.wordsRead] = word;
                }
                result.wordsRead++;
            }

            if ((control & romStart) == 0) {
                break;
            }
            if (bus.ticks() - started >= timeoutTicks) {
                result.observedMCardControl = mCardControl;
                result.observedRomControl   = control;
                result.status               = NtrCard::SectorReadStatus::Timeout;

                result.drainedWords = drainAbandonedTransfer(bus, result.transferControl, timeoutTicks);
                return result;
            }
        }

        result.status = result.wordsRead == expectedWords ? NtrCard::SectorReadStatus::Ok : NtrCard::SectorReadStatus::WordCountMismatch;
        return result;
    }

    std::size_t flushAbandonedWrite(NtrCard::CardBus& bus, std::uint32_t transferControl, std::uint64_t timeoutTicks)
    {
        constexpr std::uint32_t ownership = ~(romVolatileStatus | romNoReset);
        const std::uint64_t started       = bus.ticks();
        std::size_t flushed               = 0;
        for (;;) {
            const std::uint32_t control = bus.read32(NtrCard::RegisterOffset::RomControl);
            if ((control & romStart) == 0) {
                break;
            }
            if ((control & ownership) != (transferControl & ownership)) {
                break;
            }
            if ((control & romDataReady) != 0) {
                bus.write32(NtrCard::RegisterOffset::Fifo, 0);
                flushed++;
            }
            if (flushed >= maxDrainWords || bus.ticks() - started >= timeoutTicks) {
                break;
            }
        }
        return flushed;
    }

    CommandResult runPolledWriteCommand(NtrCard::CardBus& bus, std::uint8_t command, std::uint32_t parameter, std::uint32_t normalRomControl,
        std::uint32_t blockSize, std::uint16_t expectedMCardControl, const std::uint32_t* input, std::size_t inputWords, std::uint64_t timeoutTicks)
    {
        CommandResult result;
        const auto commandBytes     = commandWithParameter(command, parameter);
        result.expectedCommandHigh  = packedCommandWord(commandBytes, 0);
        result.expectedCommandLow   = packedCommandWord(commandBytes, 4);
        result.observedRomControl   = bus.read32(NtrCard::RegisterOffset::RomControl);
        result.observedMCardControl = bus.read16(NtrCard::RegisterOffset::MCardControl);
        if ((result.observedRomControl & romStart) != 0 || result.observedMCardControl != expectedMCardControl) {
            result.status = NtrCard::SectorReadStatus::ControllerStateChanged;
            return result;
        }

        bus.writeCommand(commandBytes);

        const auto commandReadback = bus.readCommand();
        result.observedCommandHigh = packedCommandWord(commandReadback, 0);
        result.observedCommandLow  = packedCommandWord(commandReadback, 4);
        bool commandReadbackIsZero = true;
        for (const std::uint8_t byte : commandReadback) {
            commandReadbackIsZero = commandReadbackIsZero && byte == 0;
        }
        result.commandReadbackAvailable = !commandReadbackIsZero;
        if (result.commandReadbackAvailable &&
            (result.observedCommandHigh != result.expectedCommandHigh || result.observedCommandLow != result.expectedCommandLow)) {
            result.commandIdentityChanged = true;
            result.status                 = NtrCard::SectorReadStatus::ControllerStateChanged;
            return result;
        }

        result.transferControl = (normalRomControl & ~romTransferFixedBits) | romNoReset | romWrite | blockSize | romStart;
        bus.write32(NtrCard::RegisterOffset::RomControl, result.transferControl);
        result.transferStarted = true;

        const std::uint64_t started = bus.ticks();
        for (;;) {
            const std::uint16_t mCardControl = bus.read16(NtrCard::RegisterOffset::MCardControl);
            if (mCardControl != expectedMCardControl) {
                result.observedMCardControl = mCardControl;
                result.observedRomControl   = bus.read32(NtrCard::RegisterOffset::RomControl);
                result.status               = NtrCard::SectorReadStatus::ControllerStateChanged;
                result.drainedWords         = flushAbandonedWrite(bus, result.transferControl, timeoutTicks);
                return result;
            }

            const std::uint32_t control = bus.read32(NtrCard::RegisterOffset::RomControl);
            result.completedControl     = control;
            if ((control & ~romVolatileStatus) != (result.transferControl & ~romVolatileStatus)) {
                result.observedMCardControl = mCardControl;
                result.observedRomControl   = control;
                result.status =
                    (control & romNoReset) == 0 ? NtrCard::SectorReadStatus::ControllerReset : NtrCard::SectorReadStatus::ControllerStateChanged;
                result.drainedWords = flushAbandonedWrite(bus, result.transferControl, timeoutTicks);
                return result;
            }

            if (result.commandReadbackAvailable && ((control & romStart) == 0 || ((control & romDataReady) != 0 && (result.wordsRead % 16) == 0))) {
                const auto currentCommand       = bus.readCommand();
                const std::uint32_t commandHigh = packedCommandWord(currentCommand, 0);
                const std::uint32_t commandLow  = packedCommandWord(currentCommand, 4);
                if (commandHigh != result.expectedCommandHigh || commandLow != result.expectedCommandLow) {
                    result.observedMCardControl   = mCardControl;
                    result.observedRomControl     = control;
                    result.observedCommandHigh    = commandHigh;
                    result.observedCommandLow     = commandLow;
                    result.commandIdentityChanged = true;
                    result.status                 = NtrCard::SectorReadStatus::ControllerStateChanged;
                    return result;
                }
            }

            if ((control & romDataReady) != 0) {

                const std::uint32_t word = result.wordsRead < inputWords ? input[result.wordsRead] : 0;
                bus.write32(NtrCard::RegisterOffset::Fifo, word);
                result.wordsRead++;
            }

            if ((control & romStart) == 0) {
                break;
            }
            if (bus.ticks() - started >= timeoutTicks) {
                result.observedMCardControl = mCardControl;
                result.observedRomControl   = control;
                result.status               = NtrCard::SectorReadStatus::Timeout;
                result.drainedWords         = flushAbandonedWrite(bus, result.transferControl, timeoutTicks);
                return result;
            }
        }

        result.status = result.wordsRead == inputWords ? NtrCard::SectorReadStatus::Ok : NtrCard::SectorReadStatus::WordCountMismatch;
        return result;
    }

    NandRangeResult readNandRange(NtrCard::CardBus& bus, std::uint32_t rwStart, std::uint32_t windowIndex, std::uint32_t firstSector,
        std::size_t sectorCount, std::size_t blockSectors, std::uint32_t normalRomControl, std::uint64_t timeoutTicks, std::uint32_t* output)
    {
        using NtrCard::NandReadStage;
        using NtrCard::RegisterOffset;
        using NtrCard::SectorReadStatus;

        NandRangeResult result;
        if (rwStart < mainDataMinimum || (rwStart % NtrCard::nandWindowSize) != 0 || timeoutTicks == 0 || output == nullptr || sectorCount == 0 ||
            firstSector >= NtrCard::nandWindowSectors || sectorCount > NtrCard::nandWindowSectors - firstSector ||
            windowIndex > (0xFFFFFFFFu - rwStart) / NtrCard::nandWindowSize) {
            return result;
        }

        if (!validBlockSectors(blockSectors) || (firstSector % blockSectors) != 0 || (sectorCount % blockSectors) != 0) {
            return result;
        }
        result.windowBase   = rwStart + windowIndex * static_cast<std::uint32_t>(NtrCard::nandWindowSize);
        result.firstSector  = firstSector;
        result.blockSectors = blockSectors;

        result.status = preflightController(bus, normalRomControl, result.initialMCardControl, result.initialRomControl, result.idleConfigDiffers);
        if (result.status != SectorReadStatus::Ok) {

            result.observedMCardControl = result.initialMCardControl;
            result.observedRomControl   = result.initialRomControl;
            if (result.status == SectorReadStatus::ControllerReset) {
                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
            }
            return result;
        }

        const std::uint32_t initialCommandHigh = bus.read32(RegisterOffset::CommandHigh);
        const std::uint32_t initialCommandLow  = bus.read32(RegisterOffset::CommandLow);
        ControllerStateRestorer restore(bus, result.initialMCardControl, result.initialRomControl, initialCommandHigh, initialCommandLow);

        std::uint16_t polledMCardControl = result.initialMCardControl;
        const auto returnToRom           = [&]() {
            result.returnToRomAttempted = true;

            const std::uint16_t currentMCardControl = bus.read16(RegisterOffset::MCardControl);
            const std::uint32_t currentRomControl   = bus.read32(RegisterOffset::RomControl);
            if ((currentMCardControl & mCardEnable) == 0 || (currentRomControl & romStart) != 0) {
                result.returnToRomStatus =
                    (currentMCardControl & mCardEnable) == 0 ? SectorReadStatus::ControllerDisabled : SectorReadStatus::ControllerBusy;
                restore.abandon();
                return;
            }

            const CommandResult command =
                runPolledCommand(bus, nandRomModeCommand, 0, normalRomControl, 0, currentMCardControl, nullptr, 0, timeoutTicks);
            result.returnToRomStatus        = command.status;
            result.commandReadbackAvailable = result.commandReadbackAvailable && command.commandReadbackAvailable;
            result.drainedWords += command.drainedWords;
            result.returnTransferRomControl  = command.transferControl;
            result.returnCompletedRomControl = command.completedControl;
            result.recoveredToRomMode        = command.status == SectorReadStatus::Ok;
            if (command.status == SectorReadStatus::ControllerReset) {
                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
                restore.abandon();
            }
            else if (command.status == SectorReadStatus::ControllerStateChanged) {
                restore.abandon();
            }
        };

        const auto recoverToRomMode = [&](SectorReadStatus status) {
            if (status == SectorReadStatus::ControllerReset) {

                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
                restore.abandon();
                return;
            }
            if (status == SectorReadStatus::WordCountMismatch) {
                if ((bus.read32(RegisterOffset::RomControl) & romStart) == 0 && bus.read16(RegisterOffset::MCardControl) == polledMCardControl) {
                    returnToRom();
                }
                return;
            }
            if (status != SectorReadStatus::ControllerStateChanged) {

                return;
            }

            const std::uint64_t cleanupTimeout = timeoutTicks * cleanupTimeoutMultiplier;
            const std::uint64_t started        = bus.ticks();
            for (unsigned attempt = 0; attempt < cleanupAttempts; attempt++) {
                bool reclaimed = false;
                while (bus.ticks() - started < cleanupTimeout) {
                    const std::uint16_t mCardControl = bus.read16(RegisterOffset::MCardControl);
                    const std::uint32_t romControl   = bus.read32(RegisterOffset::RomControl);
                    if ((romControl & romNoReset) == 0) {

                        result.cartridgeWasReset  = true;
                        result.recoveredToRomMode = true;
                        restore.abandon();
                        return;
                    }
                    if ((mCardControl & mCardEnable) != 0 && (romControl & romStart) == 0) {
                        reclaimed = true;
                        break;
                    }
                }
                if (!reclaimed) {
                    return;
                }

                returnToRom();
                if (result.recoveredToRomMode) {
                    return;
                }
            }
        };

        result.stage = NandReadStage::SelectWindow;
        const CommandResult selected =
            runPolledCommand(bus, nandRwModeCommand, result.windowBase, normalRomControl, 0, polledMCardControl, nullptr, 0, timeoutTicks);
        result.status                   = selected.status;
        result.commandReadbackAvailable = selected.commandReadbackAvailable;
        result.drainedWords += selected.drainedWords;
        result.selectTransferRomControl  = selected.transferControl;
        result.selectCompletedRomControl = selected.completedControl;
        if (selected.status != SectorReadStatus::Ok) {
            result.observedMCardControl   = selected.observedMCardControl;
            result.observedRomControl     = selected.observedRomControl;
            result.observedCommandHigh    = selected.observedCommandHigh;
            result.observedCommandLow     = selected.observedCommandLow;
            result.commandIdentityChanged = selected.commandIdentityChanged;
            result.expectedCommandHigh    = selected.expectedCommandHigh;
            result.expectedCommandLow     = selected.expectedCommandLow;

            if (selected.status == SectorReadStatus::ControllerStateChanged && !selected.transferStarted) {
                result.stage = NandReadStage::Preflight;
                restore.abandon();
                return result;
            }
            recoverToRomMode(selected.status);
            if (selected.status == SectorReadStatus::ControllerStateChanged && !result.recoveredToRomMode) {
                restore.abandon();
            }
            return result;
        }

        result.stage                      = NandReadStage::ReadSector;
        const std::uint32_t readBlockSize = blockSizeField(blockSectors);
        const std::size_t blockWords      = blockSectors * NtrCard::sectorWords;
        for (std::size_t sector = 0; sector < sectorCount; sector += blockSectors) {
            const std::uint32_t offset      = result.windowBase + static_cast<std::uint32_t>((firstSector + sector) * NtrCard::sectorSize);
            const CommandResult read        = runPolledCommand(bus, mainReadCommand, offset, normalRomControl, readBlockSize, polledMCardControl,
                       output + sector * NtrCard::sectorWords, blockWords, timeoutTicks);
            result.status                   = read.status;
            result.commandReadbackAvailable = result.commandReadbackAvailable && read.commandReadbackAvailable;
            result.wordsRead                = read.wordsRead;
            result.readTransferRomControl   = read.transferControl;
            result.readCompletedRomControl  = read.completedControl;
            result.drainedWords += read.drainedWords;
            if (read.status != SectorReadStatus::Ok) {
                result.observedMCardControl   = read.observedMCardControl;
                result.observedRomControl     = read.observedRomControl;
                result.observedCommandHigh    = read.observedCommandHigh;
                result.observedCommandLow     = read.observedCommandLow;
                result.commandIdentityChanged = read.commandIdentityChanged;
                result.expectedCommandHigh    = read.expectedCommandHigh;
                result.expectedCommandLow     = read.expectedCommandLow;
                recoverToRomMode(read.status);
                if (read.status == SectorReadStatus::ControllerStateChanged && !result.recoveredToRomMode) {
                    restore.abandon();
                }
                return result;
            }
            result.sectorsRead = sector + blockSectors;
        }

        result.stage = NandReadStage::ReturnToRom;
        returnToRom();
        if (result.returnToRomStatus != SectorReadStatus::Ok) {
            result.status = result.returnToRomStatus;
            return result;
        }

        result.status = SectorReadStatus::Ok;
        result.stage  = NandReadStage::Complete;
        return result;
    }

    NtrCard::NandPageWriteResult writeNandPage(NtrCard::CardBus& bus, std::uint32_t rwStart, std::uint32_t pageOffset, std::uint32_t normalRomControl,
        std::uint64_t timeoutTicks, const std::uint32_t* page, std::size_t pageWords)
    {
        using NtrCard::NandWriteStage;
        using NtrCard::RegisterOffset;
        using NtrCard::SectorReadStatus;

        NtrCard::NandPageWriteResult result;
        result.pageOffset = pageOffset;
        if (rwStart < mainDataMinimum || (rwStart % NtrCard::nandWindowSize) != 0 || timeoutTicks == 0 || page == nullptr ||
            pageWords != NtrCard::nandPageWords || pageOffset < rwStart || (pageOffset % NtrCard::nandPageSize) != 0) {
            return result;
        }

        const std::uint32_t windowBase = rwStart + ((pageOffset - rwStart) / NtrCard::nandWindowSize) * (std::uint32_t)NtrCard::nandWindowSize;

        bool idleConfigDiffers = false;
        result.status          = preflightController(bus, normalRomControl, result.initialMCardControl, result.initialRomControl, idleConfigDiffers);
        if (result.status != SectorReadStatus::Ok) {
            if (result.status == SectorReadStatus::ControllerReset) {
                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
            }
            return result;
        }

        const std::uint32_t initialCommandHigh = bus.read32(RegisterOffset::CommandHigh);
        const std::uint32_t initialCommandLow  = bus.read32(RegisterOffset::CommandLow);
        ControllerStateRestorer restore(bus, result.initialMCardControl, result.initialRomControl, initialCommandHigh, initialCommandLow);

        std::uint16_t polledMCardControl = result.initialMCardControl;

        const auto controllerIdleAndOurs = [&]() {
            const std::uint16_t mCardControl = bus.read16(RegisterOffset::MCardControl);
            const std::uint32_t romControl   = bus.read32(RegisterOffset::RomControl);
            return (mCardControl & mCardEnable) != 0 && (romControl & romStart) == 0 && (romControl & romNoReset) != 0;
        };

        const auto discardBuffer = [&]() {
            if (result.discardAttempted || !controllerIdleAndOurs()) {
                return;
            }
            result.discardAttempted                 = true;
            const std::uint16_t currentMCardControl = bus.read16(RegisterOffset::MCardControl);
            (void)runPolledCommand(bus, nandDiscardBufferCommand, 0, normalRomControl, 0, currentMCardControl, nullptr, 0, timeoutTicks);
        };

        const auto returnToRom = [&]() {
            result.returnToRomAttempted = true;
            if (!controllerIdleAndOurs()) {
                const std::uint16_t mCardControl = bus.read16(RegisterOffset::MCardControl);
                result.returnToRomStatus =
                    (mCardControl & mCardEnable) == 0 ? SectorReadStatus::ControllerDisabled : SectorReadStatus::ControllerBusy;
                restore.abandon();
                return;
            }
            const std::uint16_t currentMCardControl = bus.read16(RegisterOffset::MCardControl);
            const CommandResult command =
                runPolledCommand(bus, nandRomModeCommand, 0, normalRomControl, 0, currentMCardControl, nullptr, 0, timeoutTicks);
            result.returnToRomStatus  = command.status;
            result.recoveredToRomMode = command.status == SectorReadStatus::Ok;
            if (command.status == SectorReadStatus::ControllerReset) {
                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
                restore.abandon();
            }
            else if (command.status == SectorReadStatus::ControllerStateChanged) {
                restore.abandon();
            }
        };

        const auto cleanup = [&](SectorReadStatus status) {
            if (status == SectorReadStatus::ControllerReset) {
                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
                restore.abandon();
                return;
            }
            discardBuffer();
            returnToRom();
        };

        const auto fail = [&](NandWriteStage stage, const CommandResult& command) {
            result.stage                  = stage;
            result.status                 = command.status;
            result.commandIdentityChanged = command.commandIdentityChanged;
            result.expectedCommandHigh    = command.expectedCommandHigh;
            result.expectedCommandLow     = command.expectedCommandLow;
            result.observedCommandHigh    = command.observedCommandHigh;
            result.observedCommandLow     = command.observedCommandLow;
            cleanup(command.status);
            return result;
        };

        result.stage = NandWriteStage::SelectWindow;
        const CommandResult selected =
            runPolledCommand(bus, nandRwModeCommand, windowBase, normalRomControl, 0, polledMCardControl, nullptr, 0, timeoutTicks);
        result.commandReadbackAvailable = selected.commandReadbackAvailable;
        if (selected.status != SectorReadStatus::Ok) {

            result.stage                  = NandWriteStage::SelectWindow;
            result.status                 = selected.status;
            result.commandIdentityChanged = selected.commandIdentityChanged;
            if (selected.status == SectorReadStatus::ControllerReset) {
                result.cartridgeWasReset  = true;
                result.recoveredToRomMode = true;
                restore.abandon();
            }
            return result;
        }

        const CommandResult enabled =
            runPolledCommand(bus, nandWriteEnableCommand, 0, normalRomControl, 0, polledMCardControl, nullptr, 0, timeoutTicks);
        if (enabled.status != SectorReadStatus::Ok) {
            return fail(NandWriteStage::WriteEnable, enabled);
        }

        result.stage = NandWriteStage::WriteBuffer;
        for (std::size_t chunk = 0; chunk < NtrCard::nandPageSectors; chunk++) {
            const CommandResult written = runPolledWriteCommand(bus, nandWriteBufferCommand, pageOffset, normalRomControl, blockSizeField(1),
                polledMCardControl, page + chunk * NtrCard::sectorWords, NtrCard::sectorWords, timeoutTicks);
            result.wordsWritten += written.wordsRead;
            result.flushedWords += written.drainedWords;
            if (written.status != SectorReadStatus::Ok) {
                return fail(NandWriteStage::WriteBuffer, written);
            }
        }

        result.stage = NandWriteStage::CommitBuffer;

        result.committed = true;
        const CommandResult committed =
            runPolledCommand(bus, nandCommitBufferCommand, 0, normalRomControl, 0, polledMCardControl, nullptr, 0, timeoutTicks);
        if (committed.status != SectorReadStatus::Ok) {
            return fail(NandWriteStage::CommitBuffer, committed);
        }

        result.stage                     = NandWriteStage::AwaitReady;
        const std::uint64_t readyStarted = bus.ticks();
        const std::uint64_t readyTimeout = timeoutTicks * cleanupTimeoutMultiplier;
        for (;;) {
            std::uint32_t status = 0;
            const CommandResult polled =
                runPolledCommand(bus, nandReadStatusCommand, 0, normalRomControl, romBlockSizeMask, polledMCardControl, &status, 1, timeoutTicks);
            result.statusPolls++;
            if (polled.status != SectorReadStatus::Ok) {
                return fail(NandWriteStage::AwaitReady, polled);
            }
            result.statusRegister = status;
            if ((status & nandStatusReady) != 0) {
                break;
            }
            if (bus.ticks() - readyStarted >= readyTimeout) {
                result.stage  = NandWriteStage::AwaitReady;
                result.status = SectorReadStatus::Timeout;
                cleanup(SectorReadStatus::Timeout);
                return result;
            }
        }

        result.stage = NandWriteStage::DiscardBuffer;
        discardBuffer();

        result.stage = NandWriteStage::ReturnToRom;
        returnToRom();
        if (!result.recoveredToRomMode) {
            result.status = result.returnToRomStatus;
            return result;
        }

        result.status = SectorReadStatus::Ok;
        result.stage  = NandWriteStage::Complete;
        return result;
    }

}

NtrCard::RegisterSnapshot NtrCard::capture(const RegisterReader& reader)
{

    return {
        .mCardControl = reader.read16(RegisterOffset::MCardControl),
        .romControl   = reader.read32(RegisterOffset::RomControl),
        .commandHigh  = reader.read32(RegisterOffset::CommandHigh),
        .commandLow   = reader.read32(RegisterOffset::CommandLow),
        .seedXLow     = reader.read32(RegisterOffset::SeedXLow),
        .seedYLow     = reader.read32(RegisterOffset::SeedYLow),
        .seedXHigh    = reader.read16(RegisterOffset::SeedXHigh),
        .seedYHigh    = reader.read16(RegisterOffset::SeedYHigh),
    };
}

bool NtrCard::process9ControllerReady(const RegisterSnapshot& snapshot)
{
    constexpr std::uint16_t requiredMCardControl = mCardEnable | mCardTransferIrq;
    return (snapshot.mCardControl & requiredMCardControl) == requiredMCardControl && (snapshot.romControl & romStart) == 0 &&
           (snapshot.romControl & romNoReset) != 0;
}

bool NtrCard::waitForBusQuiet(CardBus& bus, std::uint64_t quietTicks, std::uint64_t timeoutTicks)
{
    const std::uint64_t started = bus.ticks();
    std::uint64_t quietSince    = started;
    for (;;) {
        const std::uint32_t control = bus.read32(RegisterOffset::RomControl);
        const std::uint64_t now     = bus.ticks();
        if ((control & romStart) != 0) {
            quietSince = now;
        }
        else if (now - quietSince >= quietTicks) {
            return true;
        }
        if (now - started >= timeoutTicks) {
            return false;
        }
    }
}

NtrCard::CardPollResult NtrCard::waitForCardPoll(CardBus& bus, std::uint64_t quietTicks, std::uint64_t timeoutTicks)
{
    CardPollResult result;
    const std::uint64_t started = bus.ticks();
    bool pollSeen               = false;
    std::uint64_t quietSince    = started;
    for (;;) {
        const std::uint16_t mCardControl = bus.read16(RegisterOffset::MCardControl);
        const std::uint32_t romControl   = bus.read32(RegisterOffset::RomControl);
        const std::uint64_t now          = bus.ticks();
        result.observedRomControl        = romControl;
        result.elapsedTicks              = now - started;

        if ((mCardControl & mCardEnable) == 0) {
            result.status = CardPollStatus::ControllerDisabled;
            return result;
        }
        if ((romControl & romNoReset) == 0) {
            result.status = CardPollStatus::ControllerReset;
            return result;
        }

        if (!pollSeen) {
            pollSeen   = (romControl & romStart) != 0 && (romControl & romBlockSizeMask) == romBlockSizeMask;
            quietSince = now;
        }
        else if ((romControl & romStart) != 0) {
            quietSince = now;
        }
        else if (now - quietSince >= quietTicks) {
            result.status = CardPollStatus::Observed;
            return result;
        }

        if (now - started >= timeoutTicks) {
            result.status = CardPollStatus::Timeout;
            return result;
        }
    }
}

NtrCard::NandPageWriteResult NtrCard::writeNandSavePage(CardBus& bus, std::uint32_t rwStart, std::uint32_t pageOffset, std::uint32_t normalRomControl,
    std::uint64_t timeoutTicks, const std::uint32_t* page, std::size_t pageWords)
{
    return writeNandPage(bus, rwStart, pageOffset, normalRomControl, timeoutTicks, page, pageWords);
}

NtrCard::StalledControllerRecovery NtrCard::recoverStalledController(CardBus& bus, std::uint64_t timeoutTicks)
{
    StalledControllerRecovery recovery;
    recovery.mCardControlBefore = bus.read16(RegisterOffset::MCardControl);
    recovery.romControlBefore   = bus.read32(RegisterOffset::RomControl);
    recovery.mCardControlAfter  = recovery.mCardControlBefore;
    recovery.romControlAfter    = recovery.romControlBefore;
    if ((recovery.romControlBefore & romStart) == 0 || timeoutTicks == 0) {

        return recovery;
    }
    recovery.attempted = true;

    if ((recovery.mCardControlBefore & mCardEnable) != 0 && (recovery.mCardControlBefore & mCardTransferIrq) == 0) {
        bus.write16(RegisterOffset::MCardControl, static_cast<std::uint16_t>(recovery.mCardControlBefore | mCardTransferIrq));
        recovery.restoredTransferIrq = true;

        const std::uint64_t started = bus.ticks();
        while (bus.ticks() - started < timeoutTicks / 2) {
            if ((bus.read32(RegisterOffset::RomControl) & romStart) == 0) {
                break;
            }
        }
    }

    if ((bus.read32(RegisterOffset::RomControl) & romStart) != 0) {
        const std::uint64_t started = bus.ticks();
        while (recovery.drainedWords < maxDrainWords && bus.ticks() - started < timeoutTicks) {
            const std::uint32_t control = bus.read32(RegisterOffset::RomControl);
            if ((control & romStart) == 0) {
                break;
            }
            if ((control & romDataReady) != 0) {
                (void)bus.read32(RegisterOffset::Fifo);
                recovery.drainedWords++;
            }
        }
    }

    recovery.mCardControlAfter = bus.read16(RegisterOffset::MCardControl);
    recovery.romControlAfter   = bus.read32(RegisterOffset::RomControl);
    recovery.cleared           = (recovery.romControlAfter & romStart) == 0;
    return recovery;
}

NtrCard::SectorReadResult NtrCard::readMainModeSector(CardBus& bus, std::uint32_t offset, std::uint32_t normalRomControl, std::uint64_t timeoutTicks)
{
    SectorReadResult result;
    if (offset < mainDataMinimum || (offset % sectorSize) != 0 || timeoutTicks == 0) {
        return result;
    }

    bool idleConfigDiffers = false;
    result.status          = preflightController(bus, normalRomControl, result.initialMCardControl, result.initialRomControl, idleConfigDiffers);
    if (result.status != SectorReadStatus::Ok) {
        return result;
    }

    const std::uint32_t initialCommandHigh = bus.read32(RegisterOffset::CommandHigh);
    const std::uint32_t initialCommandLow  = bus.read32(RegisterOffset::CommandLow);
    ControllerStateRestorer restore(bus, result.initialMCardControl, result.initialRomControl, initialCommandHigh, initialCommandLow);

    std::uint16_t polledMCardControl = result.initialMCardControl;
    const CommandResult transfer     = runPolledCommand(
        bus, mainReadCommand, offset, normalRomControl, romSectorBlockSize, polledMCardControl, result.data.data(), result.data.size(), timeoutTicks);
    result.status                   = transfer.status;
    result.wordsRead                = transfer.wordsRead;
    result.drainedWords             = transfer.drainedWords;
    result.observedMCardControl     = transfer.observedMCardControl;
    result.observedRomControl       = transfer.observedRomControl;
    result.commandReadbackAvailable = transfer.commandReadbackAvailable;
    result.commandIdentityChanged   = transfer.commandIdentityChanged;
    result.expectedCommandHigh      = transfer.expectedCommandHigh;
    result.expectedCommandLow       = transfer.expectedCommandLow;
    result.observedCommandHigh      = transfer.observedCommandHigh;
    result.observedCommandLow       = transfer.observedCommandLow;
    result.transferRomControl       = transfer.transferControl;
    result.completedRomControl      = transfer.completedControl;
    if (transfer.status == SectorReadStatus::ControllerStateChanged || transfer.status == SectorReadStatus::ControllerReset) {
        restore.abandon();
    }
    return result;
}

NtrCard::NandSectorReadResult NtrCard::readFirstNandSaveSector(
    CardBus& bus, std::uint32_t rwStart, std::uint32_t normalRomControl, std::uint64_t timeoutTicks)
{
    NandSectorReadResult result;
    const NandRangeResult range = readNandRange(bus, rwStart, 0, 0, 1, 1, normalRomControl, timeoutTicks, result.data.data());

    result.status                    = range.status;
    result.stage                     = range.stage;
    result.returnToRomStatus         = range.returnToRomStatus;
    result.returnToRomAttempted      = range.returnToRomAttempted;
    result.wordsRead                 = range.wordsRead;
    result.drainedWords              = range.drainedWords;
    result.initialMCardControl       = range.initialMCardControl;
    result.initialRomControl         = range.initialRomControl;
    result.commandReadbackAvailable  = range.commandReadbackAvailable;
    result.commandIdentityChanged    = range.commandIdentityChanged;
    result.expectedCommandHigh       = range.expectedCommandHigh;
    result.expectedCommandLow        = range.expectedCommandLow;
    result.observedCommandHigh       = range.observedCommandHigh;
    result.observedCommandLow        = range.observedCommandLow;
    result.selectTransferRomControl  = range.selectTransferRomControl;
    result.selectCompletedRomControl = range.selectCompletedRomControl;
    result.readTransferRomControl    = range.readTransferRomControl;
    result.readCompletedRomControl   = range.readCompletedRomControl;
    result.returnTransferRomControl  = range.returnTransferRomControl;
    result.returnCompletedRomControl = range.returnCompletedRomControl;
    return result;
}

NtrCard::NandRangeReadResult NtrCard::readNandSaveRange(CardBus& bus, std::uint32_t rwStart, std::uint32_t windowIndex, std::uint32_t firstSector,
    std::size_t sectorCount, std::size_t blockSectors, std::uint32_t normalRomControl, std::uint64_t timeoutTicks, std::uint32_t* output,
    std::size_t outputWords)
{
    NandRangeReadResult result;
    if (sectorCount == 0 || outputWords < sectorCount * sectorWords) {
        return result;
    }

    const NandRangeResult range =
        readNandRange(bus, rwStart, windowIndex, firstSector, sectorCount, blockSectors, normalRomControl, timeoutTicks, output);

    result.status                    = range.status;
    result.stage                     = range.stage;
    result.returnToRomStatus         = range.returnToRomStatus;
    result.returnToRomAttempted      = range.returnToRomAttempted;
    result.recoveredToRomMode        = range.recoveredToRomMode;
    result.cartridgeWasReset         = range.cartridgeWasReset;
    result.windowBase                = range.windowBase;
    result.firstSector               = range.firstSector;
    result.sectorsRead               = range.sectorsRead;
    result.wordsRead                 = range.wordsRead;
    result.blockSectors              = range.blockSectors;
    result.drainedWords              = range.drainedWords;
    result.idleConfigDiffers         = range.idleConfigDiffers;
    result.initialMCardControl       = range.initialMCardControl;
    result.initialRomControl         = range.initialRomControl;
    result.observedMCardControl      = range.observedMCardControl;
    result.observedRomControl        = range.observedRomControl;
    result.commandReadbackAvailable  = range.commandReadbackAvailable;
    result.commandIdentityChanged    = range.commandIdentityChanged;
    result.expectedCommandHigh       = range.expectedCommandHigh;
    result.expectedCommandLow        = range.expectedCommandLow;
    result.observedCommandHigh       = range.observedCommandHigh;
    result.observedCommandLow        = range.observedCommandLow;
    result.selectTransferRomControl  = range.selectTransferRomControl;
    result.selectCompletedRomControl = range.selectCompletedRomControl;
    result.readTransferRomControl    = range.readTransferRomControl;
    result.readCompletedRomControl   = range.readCompletedRomControl;
    result.returnTransferRomControl  = range.returnTransferRomControl;
    result.returnCompletedRomControl = range.returnCompletedRomControl;
    return result;
}

std::uint32_t NtrCard::tunedRomControl(std::uint32_t headerRomControl)
{

    return headerRomControl;
}

bool NtrCard::mappedCardAccessAvailable(void)
{
#if defined(__3DS__)
    return true;
#else
    return false;
#endif
}

std::optional<NtrCard::RegisterSnapshot> NtrCard::captureMapped(void)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    const MappedCardBus reader;
    return capture(reader);
#else
    return std::nullopt;
#endif
}

std::optional<NtrCard::SectorReadResult> NtrCard::readMappedMainModeSector(std::uint32_t offset, std::uint32_t normalRomControl)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    MappedCardBus bus;
    return readMainModeSector(bus, offset, normalRomControl, SYSCLOCK_ARM11 / 10);
#else
    (void)offset;
    (void)normalRomControl;
    return std::nullopt;
#endif
}

std::optional<NtrCard::NandSectorReadResult> NtrCard::readMappedFirstNandSaveSector(std::uint32_t rwStart, std::uint32_t normalRomControl)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    MappedCardBus bus;
    return readFirstNandSaveSector(bus, rwStart, normalRomControl, SYSCLOCK_ARM11 / 10);
#else
    (void)rwStart;
    (void)normalRomControl;
    return std::nullopt;
#endif
}

std::optional<NtrCard::NandRangeReadResult> NtrCard::readMappedNandSaveRange(std::uint32_t rwStart, std::uint32_t windowIndex,
    std::uint32_t firstSector, std::size_t sectorCount, std::size_t blockSectors, std::uint32_t normalRomControl, std::uint32_t* output,
    std::size_t outputWords)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    MappedCardBus bus;
    return readNandSaveRange(
        bus, rwStart, windowIndex, firstSector, sectorCount, blockSectors, normalRomControl, SYSCLOCK_ARM11 / 10, output, outputWords);
#else
    (void)rwStart;
    (void)windowIndex;
    (void)firstSector;
    (void)sectorCount;
    (void)blockSectors;
    (void)normalRomControl;
    (void)output;
    (void)outputWords;
    return std::nullopt;
#endif
}

bool NtrCard::waitForMappedBusQuiet(void)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return false;
    }

    MappedCardBus bus;

    return waitForBusQuiet(bus, SYSCLOCK_ARM11 / 50, SYSCLOCK_ARM11);
#else
    return false;
#endif
}

std::optional<NtrCard::CardPollResult> NtrCard::waitForMappedCardPoll(void)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    MappedCardBus bus;
    return waitForCardPoll(bus, SYSCLOCK_ARM11 / 200, SYSCLOCK_ARM11 * 3 / 2);
#else
    return std::nullopt;
#endif
}

std::optional<NtrCard::NandPageWriteResult> NtrCard::writeMappedNandSavePage(
    std::uint32_t rwStart, std::uint32_t pageOffset, std::uint32_t normalRomControl, const std::uint32_t* page, std::size_t pageWords)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    MappedCardBus bus;

    return writeNandSavePage(bus, rwStart, pageOffset, normalRomControl, SYSCLOCK_ARM11 / 10, page, pageWords);
#else
    (void)rwStart;
    (void)pageOffset;
    (void)normalRomControl;
    (void)page;
    (void)pageWords;
    return std::nullopt;
#endif
}

std::optional<NtrCard::StalledControllerRecovery> NtrCard::recoverMappedStalledController(void)
{
#if defined(__3DS__)
    if (envIsHomebrew()) {
        return std::nullopt;
    }

    MappedCardBus bus;
    return recoverStalledController(bus, SYSCLOCK_ARM11 / 10);
#else
    return std::nullopt;
#endif
}
