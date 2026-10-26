#include "ntrcard.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace {
    class RecordingRegisterReader final : public NtrCard::RegisterReader {
    public:
        std::uint16_t read16(NtrCard::RegisterOffset offset) const override
        {
            reads.emplace_back(16, offset);
            return static_cast<std::uint16_t>(0xA000u + static_cast<std::uintptr_t>(offset));
        }

        std::uint32_t read32(NtrCard::RegisterOffset offset) const override
        {
            reads.emplace_back(32, offset);
            return 0xB0000000u + static_cast<std::uint32_t>(offset);
        }

        mutable std::vector<std::pair<unsigned, NtrCard::RegisterOffset>> reads;
    };

    class ScriptedCardBus final : public NtrCard::CardBus {
    public:
        enum class Behavior {
            Success,
            Timeout,
            Short,

            Stolen,

            AcquireStolen,

            Reset,

            IrqCleared,

            BusyThenIdle,

            SameControlCommandStolen,

            CommandStolenBeforeStart,

            CommandReadbackUnavailable,
        };

        explicit ScriptedCardBus(Behavior behavior) : behavior(behavior) {}

        std::uint16_t read16(NtrCard::RegisterOffset offset) const override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);

            if (behavior == Behavior::Stolen && started && currentCommand == 0xB7 && b7Index == failSectorIndex && !stolenOnce) {
                stolenOnce   = true;
                mCardControl = 0xC000;
            }
            if (behavior == Behavior::IrqCleared && started && currentCommand == 0xB7 && b7Index == failSectorIndex) {
                mCardControl = 0x8000;
            }
            if (behavior == Behavior::AcquireStolen && acquireArmed && !acquireStolenOnce) {
                acquireStolenOnce = true;
                mCardControl      = 0xC000;
            }
            return mCardControl;
        }

        std::uint32_t read32(NtrCard::RegisterOffset offset) const override
        {
            switch (offset) {
                case NtrCard::RegisterOffset::RomControl:
                    if (!started) {
                        return romControl;
                    }

                    if (behavior == Behavior::Stolen && currentCommand == 0xB7 && b7Index == failSectorIndex && !stolenRomReported) {
                        stolenRomReported = true;
                        return foreignRomControl | busy;
                    }

                    if (behavior == Behavior::BusyThenIdle && currentCommand == 0xB7 && b7Index == failSectorIndex) {
                        if (busyReads != 0) {
                            busyReads--;
                            return foreignRomControl | busy;
                        }
                        return foreignRomControl;
                    }

                    if (behavior == Behavior::Stolen && stolenOnce) {
                        return transferControl & ~busy;
                    }

                    if (behavior == Behavior::Reset && currentCommand == 0xB7 && b7Index == failSectorIndex) {
                        romControl = transferControl & ~nReset;
                        romControl = fifoWordsRead < wordsToProduce() ? (romControl | dataReady) : (romControl & ~busy);
                        return romControl;
                    }

                    if (currentCommand != 0xB7) {
                        return transferControl & ~busy;
                    }
                    if (behavior == Behavior::Timeout && b7Index == failSectorIndex) {
                        return transferControl;
                    }
                    if (fifoWordsRead < wordsToProduce()) {
                        return transferControl | dataReady;
                    }
                    return transferControl & ~busy;
                case NtrCard::RegisterOffset::CommandHigh:
                    stealCommandAtSameControl();
                    return commandHigh;
                case NtrCard::RegisterOffset::CommandLow:
                    stealCommandAtSameControl();
                    return commandLow;
                case NtrCard::RegisterOffset::Fifo:

                    return 0xC0000000u + (static_cast<std::uint32_t>(b7Index) << 16) + static_cast<std::uint32_t>(fifoWordsRead++);
                default:
                    assert(false);
                    return 0;
            }
        }

        std::array<std::uint8_t, 8> readCommand(void) const override
        {
            stealCommandAtSameControl();
            if (behavior == Behavior::CommandReadbackUnavailable) {
                return {};
            }

            std::array<std::uint8_t, 8> value{};
            for (std::size_t i = 0; i < 4; i++) {
                value[i]     = static_cast<std::uint8_t>(commandHigh >> (i * 8));
                value[i + 4] = static_cast<std::uint8_t>(commandLow >> (i * 8));
            }
            return value;
        }

        void write16(NtrCard::RegisterOffset offset, std::uint16_t value) override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);
            writes.emplace_back(16, offset, value);
            mCardControl = value;
            if (behavior == Behavior::AcquireStolen && value == 0x8000) {
                acquireArmed = true;
            }
        }

        void write32(NtrCard::RegisterOffset offset, std::uint32_t value) override
        {
            writes.emplace_back(32, offset, value);
            switch (offset) {
                case NtrCard::RegisterOffset::RomControl:
                    romControl = value;
                    if ((value & busy) != 0) {
                        transferControl = value;
                        started         = true;
                    }
                    else {
                        started = false;
                    }
                    break;
                case NtrCard::RegisterOffset::CommandHigh:
                    commandHigh = value;
                    break;
                case NtrCard::RegisterOffset::CommandLow:
                    commandLow = value;
                    break;
                default:
                    assert(false);
                    break;
            }
        }

        void writeCommand(const std::array<std::uint8_t, 8>& value) override
        {
            commandWrites.push_back(value);
            if (value[0] == 0xB7) {
                b7Index = b7Seen++;
            }
            currentCommand = value[0];
            fifoWordsRead  = 0;
            started        = false;
            commandHigh    = packLittle(value, 0);
            commandLow     = packLittle(value, 4);
        }

        std::uint64_t ticks(void) const override { return tick++; }

        static constexpr std::uint32_t busy      = 1u << 31;
        static constexpr std::uint32_t dataReady = 1u << 23;
        static constexpr std::uint32_t nReset    = 1u << 29;

        Behavior behavior;
        mutable std::uint16_t mCardControl    = 0xC000;
        mutable std::uint32_t romControl      = 0x27416657;
        mutable std::uint32_t commandHigh     = 0x11223344;
        mutable std::uint32_t commandLow      = 0x55667788;
        mutable std::uint32_t transferControl = 0;
        mutable std::size_t fifoWordsRead     = 0;
        mutable std::uint64_t tick            = 0;
        mutable bool started                  = false;
        mutable std::uint8_t currentCommand   = 0;
        mutable bool stolenOnce               = false;
        mutable bool stolenRomReported        = false;
        mutable std::size_t busyReads         = 0;
        std::uint32_t foreignRomControl       = 0x27000100;
        mutable bool acquireArmed             = false;
        mutable bool acquireStolenOnce        = false;
        mutable bool commandStolenOnce        = false;
        mutable std::size_t b7Seen            = 0;
        mutable std::size_t b7Index           = 0;
        std::size_t failSectorIndex           = 0;
        std::vector<std::tuple<unsigned, NtrCard::RegisterOffset, std::uint32_t>> writes;
        std::vector<std::array<std::uint8_t, 8>> commandWrites;

    private:
        void stealCommandAtSameControl(void) const
        {
            if (behavior == Behavior::CommandStolenBeforeStart && !started && currentCommand == 0xB2 && !commandStolenOnce) {
                commandStolenOnce = true;
                commandHigh       = 0xDEADBEEFu;
                commandLow        = 0x0BADF00Du;
            }
            if (behavior == Behavior::SameControlCommandStolen && started && currentCommand == 0xB7 && b7Index == failSectorIndex &&
                fifoWordsRead >= 100 && !commandStolenOnce) {
                commandStolenOnce = true;
                commandHigh       = 0xDEADBEEFu;
                commandLow        = 0x0BADF00Du;
                romControl        = transferControl & ~busy;
                started           = false;
            }
        }

        std::size_t wordsToProduce(void) const
        {
            const std::uint32_t field = (transferControl >> 24) & 7u;
            const std::size_t bytes   = field == 0 ? 0 : (field == 7 ? 4u : (0x100u << field));
            const std::size_t words   = bytes / sizeof(std::uint32_t);
            const bool shortSector    = behavior == Behavior::Short && b7Index == failSectorIndex;
            return shortSector ? words - 1 : words;
        }

        static std::uint32_t packLittle(const std::array<std::uint8_t, 8>& value, std::size_t offset)
        {
            return static_cast<std::uint32_t>(value[offset]) | (static_cast<std::uint32_t>(value[offset + 1]) << 8) |
                   (static_cast<std::uint32_t>(value[offset + 2]) << 16) | (static_cast<std::uint32_t>(value[offset + 3]) << 24);
        }
    };

    std::optional<std::uint32_t> lastWrite(const ScriptedCardBus& bus, NtrCard::RegisterOffset offset)
    {
        std::optional<std::uint32_t> value;
        for (const auto& [width, written, data] : bus.writes) {
            (void)width;
            if (written == offset) {
                value = data;
            }
        }
        return value;
    }

    class StalledCardBus final : public NtrCard::CardBus {
    public:
        std::uint16_t read16(NtrCard::RegisterOffset offset) const override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);
            return mCardControl;
        }

        std::uint32_t read32(NtrCard::RegisterOffset offset) const override
        {
            if (offset == NtrCard::RegisterOffset::Fifo) {
                if (remaining != 0) {
                    remaining--;
                }
                return 0xF0000000u + static_cast<std::uint32_t>(remaining);
            }
            assert(offset == NtrCard::RegisterOffset::RomControl);
            return remaining == 0 ? (control & ~busy) : (control | busy | dataReady);
        }

        void write16(NtrCard::RegisterOffset offset, std::uint16_t value) override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);
            writes++;
            mCardControl = value;
            if (clearsWhenIrqEnabled && (value & transferIrq) != 0) {
                remaining = 0;
            }
        }

        void write32(NtrCard::RegisterOffset, std::uint32_t) override { assert(false); }
        std::array<std::uint8_t, 8> readCommand(void) const override
        {
            assert(false);
            return {};
        }
        void writeCommand(const std::array<std::uint8_t, 8>&) override { assert(false); }
        std::uint64_t ticks(void) const override { return tick++; }

        static constexpr std::uint32_t busy        = 1u << 31;
        static constexpr std::uint32_t dataReady   = 1u << 23;
        static constexpr std::uint16_t transferIrq = 1u << 14;

        std::uint16_t mCardControl    = 0x8000;
        std::uint32_t control         = 0x27416657;
        mutable std::size_t remaining = 0;
        mutable std::uint64_t tick    = 0;
        unsigned writes               = 0;
        bool clearsWhenIrqEnabled     = false;
    };

    class PollCardBus final : public NtrCard::CardBus {
    public:
        enum class Behavior {
            Poll,
            WrongBlock,
            Reset,
        };

        explicit PollCardBus(Behavior behavior) : behavior(behavior) {}

        std::uint16_t read16(NtrCard::RegisterOffset offset) const override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);
            return mCardControl;
        }

        std::uint32_t read32(NtrCard::RegisterOffset offset) const override
        {
            assert(offset == NtrCard::RegisterOffset::RomControl);
            samples++;
            if (behavior == Behavior::Reset && samples >= 4) {
                return idleControl & ~nReset;
            }
            if (samples >= 4 && samples < 8) {
                const std::uint32_t block = behavior == Behavior::Poll ? pollBlock : wrongBlock;
                return idleControl | busy | block;
            }
            return idleControl;
        }

        void write16(NtrCard::RegisterOffset, std::uint16_t) override { assert(false); }
        void write32(NtrCard::RegisterOffset, std::uint32_t) override { assert(false); }
        std::array<std::uint8_t, 8> readCommand(void) const override
        {
            assert(false);
            return {};
        }
        void writeCommand(const std::array<std::uint8_t, 8>&) override { assert(false); }
        std::uint64_t ticks(void) const override { return tick++; }

        static constexpr std::uint32_t busy       = 1u << 31;
        static constexpr std::uint32_t nReset     = 1u << 29;
        static constexpr std::uint32_t pollBlock  = 7u << 24;
        static constexpr std::uint32_t wrongBlock = 5u << 24;

        Behavior behavior;
        std::uint16_t mCardControl  = 0xC000;
        std::uint32_t idleControl   = 0x20416657;
        mutable std::size_t samples = 0;
        mutable std::uint64_t tick  = 0;
    };

    void assertRestored(const ScriptedCardBus& bus)
    {
        assert(!bus.writes.empty());

        for (const auto& [width, offset, data] : bus.writes) {
            (void)width;
            (void)data;
            assert(offset != NtrCard::RegisterOffset::MCardControl);
        }

        assert(bus.mCardControl == 0xC000);
        assert(bus.romControl == 0x27416657);
        assert(bus.commandHigh == 0x11223344);
        assert(bus.commandLow == 0x55667788);
        assert(!bus.started);
    }

    std::uint32_t packCommandWord(const std::array<std::uint8_t, 8>& value, std::size_t offset)
    {
        return static_cast<std::uint32_t>(value[offset]) | (static_cast<std::uint32_t>(value[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(value[offset + 2]) << 16) | (static_cast<std::uint32_t>(value[offset + 3]) << 24);
    }

    class ScriptedWriteBus final : public NtrCard::CardBus {
    public:
        enum class Behavior {
            Success,

            StolenDuringBuffer,

            StolenAtCommit,

            StatusNeverReady,

            StatusReadyLate,
        };

        explicit ScriptedWriteBus(Behavior behavior) : behavior(behavior) {}

        std::uint16_t read16(NtrCard::RegisterOffset offset) const override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);

            if (behavior == Behavior::StolenDuringBuffer && currentCommand == 0x81 && bufferTransfers == 2) {
                return static_cast<std::uint16_t>(mCardControl ^ 0x4000u);
            }
            if (behavior == Behavior::StolenAtCommit && currentCommand == 0x82) {
                return static_cast<std::uint16_t>(mCardControl ^ 0x4000u);
            }
            return mCardControl;
        }

        std::uint32_t read32(NtrCard::RegisterOffset offset) const override
        {
            switch (offset) {
                case NtrCard::RegisterOffset::RomControl: {
                    if (!started) {
                        return romControl;
                    }
                    const std::size_t expected = wordsForCommand();
                    if (expected == 0) {
                        return transferControl & ~busy;
                    }
                    if (behavior == Behavior::StatusNeverReady && currentCommand == 0xD6) {

                        return wordsMoved < expected ? (transferControl | dataReady) : (transferControl & ~busy);
                    }
                    return wordsMoved < expected ? (transferControl | dataReady) : (transferControl & ~busy);
                }
                case NtrCard::RegisterOffset::CommandHigh:
                    return commandHigh;
                case NtrCard::RegisterOffset::CommandLow:
                    return commandLow;
                case NtrCard::RegisterOffset::Fifo: {

                    assert(currentCommand == 0xD6);
                    wordsMoved++;
                    if (behavior == Behavior::StatusNeverReady) {
                        return 0;
                    }
                    if (behavior == Behavior::StatusReadyLate && statusPolls < 2) {
                        statusPolls++;
                        return 0;
                    }
                    return 1u << 5;
                }
                default:
                    assert(false);
                    return 0;
            }
        }

        std::array<std::uint8_t, 8> readCommand(void) const override
        {
            std::array<std::uint8_t, 8> value{};
            for (std::size_t i = 0; i < 4; i++) {
                value[i]     = static_cast<std::uint8_t>(commandHigh >> (i * 8));
                value[i + 4] = static_cast<std::uint8_t>(commandLow >> (i * 8));
            }
            return value;
        }

        void write16(NtrCard::RegisterOffset offset, std::uint16_t value) override
        {
            assert(offset == NtrCard::RegisterOffset::MCardControl);
            mCardControl = value;
        }

        void write32(NtrCard::RegisterOffset offset, std::uint32_t value) override
        {
            switch (offset) {
                case NtrCard::RegisterOffset::RomControl:
                    romControl = value;
                    if ((value & busy) != 0) {
                        transferControl = value;
                        started         = true;

                        if (currentCommand == 0x81) {
                            assert((value & romWrite) != 0);
                        }
                        else {
                            assert((value & romWrite) == 0);
                        }
                    }
                    else {
                        started = false;
                    }
                    break;
                case NtrCard::RegisterOffset::Fifo:
                    assert(currentCommand == 0x81);
                    written.push_back(value);
                    wordsMoved++;
                    break;
                case NtrCard::RegisterOffset::CommandHigh:
                    commandHigh = value;
                    break;
                case NtrCard::RegisterOffset::CommandLow:
                    commandLow = value;
                    break;
                default:
                    assert(false);
                    break;
            }
        }

        void writeCommand(const std::array<std::uint8_t, 8>& value) override
        {
            commands.push_back(value[0]);
            if (value[0] == 0x81) {
                bufferTransfers++;
            }
            currentCommand = value[0];
            wordsMoved     = 0;
            started        = false;
            commandHigh    = packCommandWord(value, 0);
            commandLow     = packCommandWord(value, 4);
        }

        std::uint64_t ticks(void) const override { return tick++; }

        std::size_t wordsForCommand(void) const
        {
            if (currentCommand == 0x81) {
                return NtrCard::sectorWords;
            }
            if (currentCommand == 0xD6) {
                return 1;
            }
            return 0;
        }

        static constexpr std::uint32_t busy      = 1u << 31;
        static constexpr std::uint32_t dataReady = 1u << 23;
        static constexpr std::uint32_t romWrite  = 1u << 30;

        Behavior behavior;
        std::vector<std::uint8_t> commands;
        std::vector<std::uint32_t> written;
        mutable std::uint16_t mCardControl   = 0xC000;
        mutable std::uint32_t romControl     = 0x27416657;
        std::uint32_t transferControl        = 0;
        std::uint32_t commandHigh            = 0;
        std::uint32_t commandLow             = 0;
        std::uint8_t currentCommand          = 0;
        mutable std::size_t wordsMoved       = 0;
        mutable std::size_t statusPolls      = 0;
        std::size_t bufferTransfers          = 0;
        bool started                         = false;
        mutable std::uint64_t tick           = 0;
    };

    constexpr std::uint32_t writeRwStart   = 0x06A00000;
    constexpr std::uint32_t writeRomControl = 0x00416657;

    std::array<std::uint32_t, NtrCard::nandPageWords> samplePage(void)
    {
        std::array<std::uint32_t, NtrCard::nandPageWords> page{};
        for (std::size_t i = 0; i < page.size(); i++) {
            page[i] = 0xA5000000u + static_cast<std::uint32_t>(i);
        }
        return page;
    }

    void cartridgeWriteTests(void)
    {
        const auto page = samplePage();

        {
            const std::uint32_t badOffsets[] = {
                writeRwStart + 1,
                writeRwStart - NtrCard::nandPageSize,
            };
            for (const std::uint32_t offset : badOffsets) {
                ScriptedWriteBus bus(ScriptedWriteBus::Behavior::Success);
                const auto result =
                    NtrCard::writeNandSavePage(bus, writeRwStart, offset, writeRomControl, 1000, page.data(), page.size());
                assert(result.status == NtrCard::SectorReadStatus::InvalidArgument);
                assert(bus.commands.empty());
            }

            ScriptedWriteBus nullBus(ScriptedWriteBus::Behavior::Success);
            assert(NtrCard::writeNandSavePage(nullBus, writeRwStart, writeRwStart, writeRomControl, 1000, nullptr, NtrCard::nandPageWords).status ==
                   NtrCard::SectorReadStatus::InvalidArgument);
            assert(nullBus.commands.empty());

            ScriptedWriteBus shortBus(ScriptedWriteBus::Behavior::Success);
            assert(NtrCard::writeNandSavePage(shortBus, writeRwStart, writeRwStart, writeRomControl, 1000, page.data(), page.size() - 1).status ==
                   NtrCard::SectorReadStatus::InvalidArgument);
            assert(shortBus.commands.empty());
        }

        {
            ScriptedWriteBus bus(ScriptedWriteBus::Behavior::Success);
            const auto result =
                NtrCard::writeNandSavePage(bus, writeRwStart, writeRwStart, writeRomControl, 1000, page.data(), page.size());
            assert(result.status == NtrCard::SectorReadStatus::Ok);
            assert(result.stage == NtrCard::NandWriteStage::Complete);
            assert(result.committed);
            assert(result.recoveredToRomMode);
            assert(result.wordsWritten == NtrCard::nandPageWords);

            const std::vector<std::uint8_t> expected = {0xB2, 0x85, 0x81, 0x81, 0x81, 0x81, 0x82, 0xD6, 0x84, 0x8B};
            assert(bus.commands == expected);

            assert(bus.written.size() == NtrCard::nandPageWords);
            for (std::size_t i = 0; i < bus.written.size(); i++) {
                assert(bus.written[i] == page[i]);
            }
        }

        {
            ScriptedWriteBus bus(ScriptedWriteBus::Behavior::Success);
            const std::uint32_t offset = writeRwStart + 3 * NtrCard::nandWindowSize + 2 * NtrCard::nandPageSize;
            const auto result = NtrCard::writeNandSavePage(bus, writeRwStart, offset, writeRomControl, 1000, page.data(), page.size());
            assert(result.status == NtrCard::SectorReadStatus::Ok);
            assert(result.pageOffset == offset);
        }

        {
            ScriptedWriteBus bus(ScriptedWriteBus::Behavior::StolenDuringBuffer);
            const auto result =
                NtrCard::writeNandSavePage(bus, writeRwStart, writeRwStart, writeRomControl, 1000, page.data(), page.size());
            assert(result.status != NtrCard::SectorReadStatus::Ok);
            assert(result.stage == NtrCard::NandWriteStage::WriteBuffer);
            assert(!result.committed);
            assert(bus.commands.back() == 0x8B);
            const bool discarded = std::find(bus.commands.begin(), bus.commands.end(), 0x84) != bus.commands.end();
            assert(discarded);

            assert(std::find(bus.commands.begin(), bus.commands.end(), 0x82) == bus.commands.end());
        }

        {
            ScriptedWriteBus bus(ScriptedWriteBus::Behavior::StolenAtCommit);
            const auto result =
                NtrCard::writeNandSavePage(bus, writeRwStart, writeRwStart, writeRomControl, 1000, page.data(), page.size());
            assert(result.status != NtrCard::SectorReadStatus::Ok);
            assert(result.stage == NtrCard::NandWriteStage::CommitBuffer);
            assert(result.committed);
        }

        {
            ScriptedWriteBus bus(ScriptedWriteBus::Behavior::StatusNeverReady);
            const auto result =
                NtrCard::writeNandSavePage(bus, writeRwStart, writeRwStart, writeRomControl, 1000, page.data(), page.size());
            assert(result.status == NtrCard::SectorReadStatus::Timeout);
            assert(result.committed);
            assert(result.statusPolls > 1);
            assert(bus.commands.back() == 0x8B);
        }

        {
            ScriptedWriteBus bus(ScriptedWriteBus::Behavior::StatusReadyLate);
            const auto result =
                NtrCard::writeNandSavePage(bus, writeRwStart, writeRwStart, writeRomControl, 1000, page.data(), page.size());
            assert(result.status == NtrCard::SectorReadStatus::Ok);
            assert(result.statusPolls == 3);
            assert(result.committed);
        }
    }

}

int main()
{
    cartridgeWriteTests();

    RecordingRegisterReader reader;
    const NtrCard::RegisterSnapshot snapshot = NtrCard::capture(reader);

    assert(snapshot.mCardControl == 0xA000);
    assert(snapshot.romControl == 0xB0000004);
    assert(snapshot.commandHigh == 0xB0000008);
    assert(snapshot.commandLow == 0xB000000C);
    assert(snapshot.seedXLow == 0xB0000010);
    assert(snapshot.seedYLow == 0xB0000014);
    assert(snapshot.seedXHigh == 0xA018);
    assert(snapshot.seedYHigh == 0xA01A);

    const std::vector<std::pair<unsigned, NtrCard::RegisterOffset>> expected = {
        {16, NtrCard::RegisterOffset::MCardControl},
        {32, NtrCard::RegisterOffset::RomControl},
        {32, NtrCard::RegisterOffset::CommandHigh},
        {32, NtrCard::RegisterOffset::CommandLow},
        {32, NtrCard::RegisterOffset::SeedXLow},
        {32, NtrCard::RegisterOffset::SeedYLow},
        {16, NtrCard::RegisterOffset::SeedXHigh},
        {16, NtrCard::RegisterOffset::SeedYHigh},
    };
    assert(reader.reads == expected);

    for (const auto& [width, offset] : reader.reads) {
        (void)width;
        assert(offset != NtrCard::RegisterOffset::Fifo);
    }

    NtrCard::RegisterSnapshot process9Ready{};
    process9Ready.mCardControl = 0xC000;
    process9Ready.romControl   = 0x27416657;
    assert(NtrCard::process9ControllerReady(process9Ready));
    process9Ready.mCardControl = 0x8000;
    assert(!NtrCard::process9ControllerReady(process9Ready));
    process9Ready.mCardControl = 0xC000;
    process9Ready.romControl   = 0xA7416657;
    assert(!NtrCard::process9ControllerReady(process9Ready));
    process9Ready.romControl = 0x07416657;
    assert(!NtrCard::process9ControllerReady(process9Ready));

    constexpr std::uint32_t offset           = 0x01234400;
    constexpr std::uint32_t normalRomControl = 0x00416657;

    ScriptedCardBus success(ScriptedCardBus::Behavior::Success);
    const auto successResult = NtrCard::readMainModeSector(success, offset, normalRomControl, 1000);
    assert(successResult.status == NtrCard::SectorReadStatus::Ok);
    assert(successResult.wordsRead == NtrCard::sectorSize / sizeof(std::uint32_t));
    assert(successResult.data.front() == 0xC0000000);
    assert(successResult.data.back() == 0xC000007F);
    assert(successResult.commandReadbackAvailable);
    assert(!successResult.commandIdentityChanged);
    assert(successResult.expectedCommandHigh == 0x442301B7u);
    assert(successResult.expectedCommandLow == 0);
    assert(success.commandWrites.size() == 1);
    assert((success.commandWrites.front() == std::array<std::uint8_t, 8>{0xB7, 0x01, 0x23, 0x44, 0x00, 0, 0, 0}));
    assert((successResult.transferRomControl & ScriptedCardBus::busy) != 0);
    assert((successResult.transferRomControl & (7u << 24)) == (1u << 24));
    assert((successResult.transferRomControl & (1u << 30)) == 0);
    assertRestored(success);

    ScriptedCardBus unreadableCommand(ScriptedCardBus::Behavior::CommandReadbackUnavailable);
    const auto unreadableCommandResult = NtrCard::readMainModeSector(unreadableCommand, offset, normalRomControl, 1000);
    assert(unreadableCommandResult.status == NtrCard::SectorReadStatus::Ok);
    assert(unreadableCommandResult.wordsRead == NtrCard::sectorWords);
    assert(!unreadableCommandResult.commandReadbackAvailable);
    assert(!unreadableCommandResult.commandIdentityChanged);
    assert(unreadableCommandResult.observedCommandHigh == 0);
    assert(unreadableCommandResult.observedCommandLow == 0);
    assertRestored(unreadableCommand);

    ScriptedCardBus initiallyBusy(ScriptedCardBus::Behavior::Success);
    initiallyBusy.romControl |= ScriptedCardBus::busy;
    const auto busyResult = NtrCard::readMainModeSector(initiallyBusy, offset, normalRomControl, 1000);
    assert(busyResult.status == NtrCard::SectorReadStatus::ControllerBusy);
    assert(initiallyBusy.writes.empty());
    assert(initiallyBusy.commandWrites.empty());

    ScriptedCardBus disabled(ScriptedCardBus::Behavior::Success);
    disabled.mCardControl     = 0x4000;
    const auto disabledResult = NtrCard::readMainModeSector(disabled, offset, normalRomControl, 1000);
    assert(disabledResult.status == NtrCard::SectorReadStatus::ControllerDisabled);
    assert(disabled.writes.empty());

    ScriptedCardBus foreignIdle(ScriptedCardBus::Behavior::Success);
    foreignIdle.romControl       = 0x27000100;
    const auto foreignIdleResult = NtrCard::readMainModeSector(foreignIdle, offset, normalRomControl, 1000);
    assert(foreignIdleResult.status == NtrCard::SectorReadStatus::Ok);
    assert(foreignIdleResult.wordsRead == NtrCard::sectorWords);

    assert((foreignIdleResult.transferRomControl & 0x1FFFu) == (normalRomControl & 0x1FFFu));
    assert((foreignIdleResult.transferRomControl & (1u << 22)) == (normalRomControl & (1u << 22)));

    assert(foreignIdle.romControl == 0x27000100);

    ScriptedCardBus invalidOffset(ScriptedCardBus::Behavior::Success);
    const auto invalidResult = NtrCard::readMainModeSector(invalidOffset, 0, normalRomControl, 1000);
    assert(invalidResult.status == NtrCard::SectorReadStatus::InvalidArgument);
    assert(invalidOffset.writes.empty());

    ScriptedCardBus timeout(ScriptedCardBus::Behavior::Timeout);
    const auto timeoutResult = NtrCard::readMainModeSector(timeout, offset, normalRomControl, 3);
    assert(timeoutResult.status == NtrCard::SectorReadStatus::Timeout);
    assert(timeoutResult.wordsRead == 0);
    assertRestored(timeout);

    ScriptedCardBus shortRead(ScriptedCardBus::Behavior::Short);
    const auto shortResult = NtrCard::readMainModeSector(shortRead, offset, normalRomControl, 1000);
    assert(shortResult.status == NtrCard::SectorReadStatus::WordCountMismatch);
    assert(shortResult.wordsRead == (NtrCard::sectorSize / sizeof(std::uint32_t)) - 1);
    assertRestored(shortRead);

    constexpr std::uint32_t rwStart = 0x06A00000;
    ScriptedCardBus invalidNandStart(ScriptedCardBus::Behavior::Success);
    const auto invalidNandResult = NtrCard::readFirstNandSaveSector(invalidNandStart, rwStart + NtrCard::sectorSize, normalRomControl, 1000);
    assert(invalidNandResult.status == NtrCard::SectorReadStatus::InvalidArgument);
    assert(invalidNandStart.writes.empty());
    assert(invalidNandStart.commandWrites.empty());

    ScriptedCardBus nandSuccess(ScriptedCardBus::Behavior::Success);
    const auto nandResult = NtrCard::readFirstNandSaveSector(nandSuccess, rwStart, normalRomControl, 1000);
    assert(nandResult.status == NtrCard::SectorReadStatus::Ok);
    assert(nandResult.stage == NtrCard::NandReadStage::Complete);
    assert(nandResult.returnToRomAttempted);
    assert(nandResult.returnToRomStatus == NtrCard::SectorReadStatus::Ok);
    assert(nandResult.wordsRead == NtrCard::sectorSize / sizeof(std::uint32_t));
    assert(nandResult.data.front() == 0xC0000000);
    assert(nandResult.data.back() == 0xC000007F);
    assert(nandResult.commandReadbackAvailable);
    assert(nandSuccess.commandWrites.size() == 3);
    assert((nandSuccess.commandWrites[0] == std::array<std::uint8_t, 8>{0xB2, 0x06, 0xA0, 0, 0, 0, 0, 0}));
    assert((nandSuccess.commandWrites[1] == std::array<std::uint8_t, 8>{0xB7, 0x06, 0xA0, 0, 0, 0, 0, 0}));
    assert((nandSuccess.commandWrites[2] == std::array<std::uint8_t, 8>{0x8B, 0, 0, 0, 0, 0, 0, 0}));
    assert((nandResult.selectTransferRomControl & (7u << 24)) == 0);
    assert((nandResult.readTransferRomControl & (7u << 24)) == (1u << 24));
    assert((nandResult.returnTransferRomControl & (7u << 24)) == 0);
    for (const auto& command : nandSuccess.commandWrites) {
        assert(command[0] != 0x81);
        assert(command[0] != 0x82);
        assert(command[0] != 0x84);
        assert(command[0] != 0x85);
        assert(command[0] != 0xD6);
    }
    assertRestored(nandSuccess);

    ScriptedCardBus nandUnreadableCommand(ScriptedCardBus::Behavior::CommandReadbackUnavailable);
    const auto nandUnreadableCommandResult = NtrCard::readFirstNandSaveSector(nandUnreadableCommand, rwStart, normalRomControl, 1000);
    assert(nandUnreadableCommandResult.status == NtrCard::SectorReadStatus::Ok);
    assert(nandUnreadableCommandResult.stage == NtrCard::NandReadStage::Complete);
    assert(!nandUnreadableCommandResult.commandReadbackAvailable);
    assert(!nandUnreadableCommandResult.commandIdentityChanged);
    assert(nandUnreadableCommand.commandWrites.size() == 3);
    assertRestored(nandUnreadableCommand);

    ScriptedCardBus nandTimeout(ScriptedCardBus::Behavior::Timeout);
    const auto nandTimeoutResult = NtrCard::readFirstNandSaveSector(nandTimeout, rwStart, normalRomControl, 3);
    assert(nandTimeoutResult.status == NtrCard::SectorReadStatus::Timeout);
    assert(nandTimeoutResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(!nandTimeoutResult.returnToRomAttempted);
    assert(nandTimeout.commandWrites.size() == 2);
    assert(nandTimeout.commandWrites[0][0] == 0xB2);
    assert(nandTimeout.commandWrites[1][0] == 0xB7);
    assertRestored(nandTimeout);

    ScriptedCardBus nandShort(ScriptedCardBus::Behavior::Short);
    const auto nandShortResult = NtrCard::readFirstNandSaveSector(nandShort, rwStart, normalRomControl, 1000);
    assert(nandShortResult.status == NtrCard::SectorReadStatus::WordCountMismatch);
    assert(nandShortResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(nandShortResult.returnToRomAttempted);
    assert(nandShortResult.returnToRomStatus == NtrCard::SectorReadStatus::Ok);
    assert(nandShort.commandWrites.size() == 3);
    assert(nandShort.commandWrites[2][0] == 0x8B);
    assertRestored(nandShort);

    constexpr std::uint32_t windowIndex = 3;
    constexpr std::uint32_t windowBase  = rwStart + windowIndex * static_cast<std::uint32_t>(NtrCard::nandWindowSize);
    constexpr std::size_t burstSectors  = 32;
    constexpr std::uint32_t firstSector = 64;
    std::vector<std::uint32_t> burst(burstSectors * NtrCard::sectorWords, 0);

    ScriptedCardBus rangeSuccess(ScriptedCardBus::Behavior::Success);
    const auto rangeResult = NtrCard::readNandSaveRange(
        rangeSuccess, rwStart, windowIndex, firstSector, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(rangeResult.status == NtrCard::SectorReadStatus::Ok);
    assert(rangeResult.stage == NtrCard::NandReadStage::Complete);
    assert(rangeResult.windowBase == windowBase);
    assert(rangeResult.firstSector == firstSector);
    assert(rangeResult.sectorsRead == burstSectors);
    assert(rangeResult.returnToRomAttempted);
    assert(rangeResult.recoveredToRomMode);
    assert(rangeSuccess.commandWrites.size() == burstSectors + 2);

    assert((rangeSuccess.commandWrites.front() == std::array<std::uint8_t, 8>{0xB2, 0x06, 0xA6, 0x00, 0x00, 0, 0, 0}));
    assert((rangeSuccess.commandWrites.back() == std::array<std::uint8_t, 8>{0x8B, 0, 0, 0, 0, 0, 0, 0}));
    for (std::size_t sector = 0; sector < burstSectors; sector++) {
        const std::uint32_t address                  = windowBase + static_cast<std::uint32_t>((firstSector + sector) * NtrCard::sectorSize);
        const std::array<std::uint8_t, 8> expectedB7 = {0xB7, static_cast<std::uint8_t>(address >> 24), static_cast<std::uint8_t>(address >> 16),
            static_cast<std::uint8_t>(address >> 8), static_cast<std::uint8_t>(address), 0, 0, 0};
        assert(rangeSuccess.commandWrites[sector + 1] == expectedB7);

        assert(burst[sector * NtrCard::sectorWords] == 0xC0000000u + (static_cast<std::uint32_t>(sector) << 16));
        assert(burst[sector * NtrCard::sectorWords + NtrCard::sectorWords - 1] ==
               0xC0000000u + (static_cast<std::uint32_t>(sector) << 16) + NtrCard::sectorWords - 1);
    }
    for (const auto& command : rangeSuccess.commandWrites) {
        assert(command[0] == 0xB2 || command[0] == 0xB7 || command[0] == 0x8B);
    }
    assertRestored(rangeSuccess);

    ScriptedCardBus rangeOverruns(ScriptedCardBus::Behavior::Success);
    const auto overrunResult = NtrCard::readNandSaveRange(
        rangeOverruns, rwStart, 0, NtrCard::nandWindowSectors - 4, 8, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(overrunResult.status == NtrCard::SectorReadStatus::InvalidArgument);
    assert(rangeOverruns.writes.empty());
    assert(rangeOverruns.commandWrites.empty());

    ScriptedCardBus rangeTooSmall(ScriptedCardBus::Behavior::Success);
    const auto tooSmallResult = NtrCard::readNandSaveRange(
        rangeTooSmall, rwStart, 0, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burstSectors * NtrCard::sectorWords - 1);
    assert(tooSmallResult.status == NtrCard::SectorReadStatus::InvalidArgument);
    assert(rangeTooSmall.writes.empty());

    ScriptedCardBus rangeOverflow(ScriptedCardBus::Behavior::Success);
    const auto overflowResult =
        NtrCard::readNandSaveRange(rangeOverflow, rwStart, 0xFFFFu, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(overflowResult.status == NtrCard::SectorReadStatus::InvalidArgument);
    assert(rangeOverflow.writes.empty());

    ScriptedCardBus rangeTimeout(ScriptedCardBus::Behavior::Timeout);
    rangeTimeout.failSectorIndex = 5;
    const auto rangeTimeoutResult =
        NtrCard::readNandSaveRange(rangeTimeout, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 1000, burst.data(), burst.size());
    assert(rangeTimeoutResult.status == NtrCard::SectorReadStatus::Timeout);
    assert(rangeTimeoutResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(rangeTimeoutResult.sectorsRead == 5);
    assert(!rangeTimeoutResult.returnToRomAttempted);
    assert(!rangeTimeoutResult.recoveredToRomMode);
    assert(rangeTimeout.commandWrites.size() == 7);
    assert(rangeTimeout.commandWrites.back()[0] == 0xB7);
    assertRestored(rangeTimeout);

    ScriptedCardBus rangeShort(ScriptedCardBus::Behavior::Short);
    rangeShort.failSectorIndex = 5;
    const auto rangeShortResult =
        NtrCard::readNandSaveRange(rangeShort, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(rangeShortResult.status == NtrCard::SectorReadStatus::WordCountMismatch);
    assert(rangeShortResult.sectorsRead == 5);
    assert(rangeShortResult.returnToRomAttempted);
    assert(rangeShortResult.recoveredToRomMode);
    assert(rangeShort.commandWrites.size() == 8);
    assert(rangeShort.commandWrites.back()[0] == 0x8B);
    assertRestored(rangeShort);

    ScriptedCardBus rangeStolen(ScriptedCardBus::Behavior::Stolen);
    rangeStolen.failSectorIndex = 5;
    const auto rangeStolenResult =
        NtrCard::readNandSaveRange(rangeStolen, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(rangeStolenResult.status == NtrCard::SectorReadStatus::ControllerStateChanged);
    assert(rangeStolenResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(rangeStolenResult.sectorsRead == 5);

    assert(rangeStolenResult.observedMCardControl == 0xC000);
    assert(rangeStolenResult.initialMCardControl == 0xC000);
    assert(rangeStolenResult.returnToRomAttempted);
    assert(rangeStolenResult.recoveredToRomMode);
    assert(rangeStolenResult.returnToRomStatus == NtrCard::SectorReadStatus::Ok);
    assert(rangeStolen.commandWrites.back()[0] == 0x8B);
    for (const auto& command : rangeStolen.commandWrites) {
        assert(command[0] == 0xB2 || command[0] == 0xB7 || command[0] == 0x8B);
    }
    assertRestored(rangeStolen);

    ScriptedCardBus sameControlStolen(ScriptedCardBus::Behavior::SameControlCommandStolen);
    sameControlStolen.failSectorIndex = 5;
    const auto sameControlStolenResult =
        NtrCard::readNandSaveRange(sameControlStolen, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(sameControlStolenResult.status == NtrCard::SectorReadStatus::ControllerStateChanged);
    assert(sameControlStolenResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(sameControlStolenResult.sectorsRead == 5);
    assert(sameControlStolenResult.wordsRead >= 100);
    assert(sameControlStolenResult.observedMCardControl == 0xC000);
    assert((sameControlStolenResult.observedRomControl & ~ScriptedCardBus::dataReady) == sameControlStolenResult.readTransferRomControl);
    assert(sameControlStolenResult.commandIdentityChanged);
    assert(sameControlStolenResult.expectedCommandHigh != sameControlStolenResult.observedCommandHigh);
    assert(sameControlStolenResult.observedCommandHigh == 0xDEADBEEFu);
    assert(sameControlStolenResult.observedCommandLow == 0x0BADF00Du);
    assert(sameControlStolenResult.returnToRomAttempted);
    assert(sameControlStolenResult.recoveredToRomMode);
    assert(sameControlStolen.commandWrites.back()[0] == 0x8B);
    assertRestored(sameControlStolen);

    ScriptedCardBus commandStolenBeforeStart(ScriptedCardBus::Behavior::CommandStolenBeforeStart);
    const auto commandStolenBeforeStartResult = NtrCard::readNandSaveRange(
        commandStolenBeforeStart, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(commandStolenBeforeStartResult.status == NtrCard::SectorReadStatus::ControllerStateChanged);
    assert(commandStolenBeforeStartResult.stage == NtrCard::NandReadStage::Preflight);
    assert(commandStolenBeforeStartResult.commandIdentityChanged);
    assert(commandStolenBeforeStartResult.expectedCommandHigh != commandStolenBeforeStartResult.observedCommandHigh);
    assert(commandStolenBeforeStartResult.observedCommandHigh == 0xDEADBEEFu);
    assert(commandStolenBeforeStartResult.observedCommandLow == 0x0BADF00Du);
    assert(!commandStolenBeforeStartResult.returnToRomAttempted);
    assert(!commandStolenBeforeStartResult.recoveredToRomMode);
    assert(commandStolenBeforeStart.commandWrites.size() == 1);
    assert(commandStolenBeforeStart.commandWrites.front()[0] == 0xB2);
    assert(commandStolenBeforeStart.commandHigh == 0xDEADBEEFu);
    assert(commandStolenBeforeStart.commandLow == 0x0BADF00Du);

    ScriptedCardBus rangeReset(ScriptedCardBus::Behavior::Reset);
    rangeReset.failSectorIndex = 5;
    const auto rangeResetResult =
        NtrCard::readNandSaveRange(rangeReset, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(rangeResetResult.status == NtrCard::SectorReadStatus::ControllerReset);
    assert(rangeResetResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(rangeResetResult.sectorsRead == 5);
    assert(rangeResetResult.cartridgeWasReset);
    assert(rangeResetResult.recoveredToRomMode);
    assert(!rangeResetResult.returnToRomAttempted);
    assert((rangeResetResult.observedRomControl & (1u << 29)) == 0);
    assert(rangeReset.commandWrites.back()[0] == 0xB7);

    assert(lastWrite(rangeReset, NtrCard::RegisterOffset::RomControl) == rangeResetResult.readTransferRomControl);
    assert((rangeReset.romControl & ScriptedCardBus::nReset) == 0);

    assert(rangeReset.mCardControl == 0xC000);

    assert(rangeResetResult.drainedWords == NtrCard::sectorWords);
    assert((rangeReset.romControl & ScriptedCardBus::busy) == 0);

    ScriptedCardBus resetAtStart(ScriptedCardBus::Behavior::Success);
    resetAtStart.romControl = 0x07000100;
    const auto resetAtStartResult =
        NtrCard::readNandSaveRange(resetAtStart, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(resetAtStartResult.status == NtrCard::SectorReadStatus::ControllerReset);
    assert(resetAtStartResult.stage == NtrCard::NandReadStage::Preflight);
    assert(resetAtStartResult.cartridgeWasReset);
    assert(resetAtStartResult.recoveredToRomMode);
    assert(resetAtStartResult.observedRomControl == 0x07000100);
    assert(resetAtStart.writes.empty());
    assert(resetAtStart.commandWrites.empty());

    ScriptedCardBus rangeContended(ScriptedCardBus::Behavior::Success);
    rangeContended.romControl |= ScriptedCardBus::busy;
    const auto contendedResult =
        NtrCard::readNandSaveRange(rangeContended, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(contendedResult.status == NtrCard::SectorReadStatus::ControllerBusy);
    assert(contendedResult.stage == NtrCard::NandReadStage::Preflight);
    assert(contendedResult.observedRomControl == (0x27416657u | ScriptedCardBus::busy));
    assert(contendedResult.observedMCardControl == 0xC000);
    assert(rangeContended.writes.empty());
    assert(rangeContended.commandWrites.empty());

    ScriptedCardBus rangeDisabled(ScriptedCardBus::Behavior::Success);
    rangeDisabled.mCardControl = 0x4000;
    const auto disabledRangeResult =
        NtrCard::readNandSaveRange(rangeDisabled, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(disabledRangeResult.status == NtrCard::SectorReadStatus::ControllerDisabled);
    assert(disabledRangeResult.stage == NtrCard::NandReadStage::Preflight);
    assert(disabledRangeResult.observedMCardControl == 0x4000);
    assert(rangeDisabled.writes.empty());

    ScriptedCardBus abandoned(ScriptedCardBus::Behavior::Success);
    const auto abandonedResult =
        NtrCard::readNandSaveRange(abandoned, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100, burst.data(), burst.size());
    assert(abandonedResult.status == NtrCard::SectorReadStatus::Timeout);
    assert(abandonedResult.drainedWords > 0);
    assert(abandonedResult.wordsRead + abandonedResult.drainedWords == NtrCard::sectorWords);
    assert((abandoned.romControl & ScriptedCardBus::busy) == 0);
    assertRestored(abandoned);

    constexpr std::size_t blockSectors = 4;
    std::vector<std::uint32_t> blocked(2 * blockSectors * NtrCard::sectorWords, 0);
    ScriptedCardBus blockedBus(ScriptedCardBus::Behavior::Success);
    const auto blockedResult = NtrCard::readNandSaveRange(
        blockedBus, rwStart, windowIndex, firstSector, 2 * blockSectors, blockSectors, normalRomControl, 100000, blocked.data(), blocked.size());
    assert(blockedResult.status == NtrCard::SectorReadStatus::Ok);
    assert(blockedResult.stage == NtrCard::NandReadStage::Complete);
    assert(blockedResult.sectorsRead == 2 * blockSectors);
    assert(blockedResult.blockSectors == blockSectors);

    assert(blockedBus.commandWrites.size() == 4);
    assert((blockedResult.readTransferRomControl & (7u << 24)) == (3u << 24));
    for (std::size_t block = 0; block < 2; block++) {
        const std::uint32_t address = windowBase + static_cast<std::uint32_t>((firstSector + block * blockSectors) * NtrCard::sectorSize);
        const std::array<std::uint8_t, 8> expectedB7 = {0xB7, static_cast<std::uint8_t>(address >> 24), static_cast<std::uint8_t>(address >> 16),
            static_cast<std::uint8_t>(address >> 8), static_cast<std::uint8_t>(address), 0, 0, 0};
        assert(blockedBus.commandWrites[block + 1] == expectedB7);
        assert(blocked[block * blockSectors * NtrCard::sectorWords] == 0xC0000000u + (static_cast<std::uint32_t>(block) << 16));
        assert(blocked[(block + 1) * blockSectors * NtrCard::sectorWords - 1] ==
               0xC0000000u + (static_cast<std::uint32_t>(block) << 16) + blockSectors * NtrCard::sectorWords - 1);
    }
    assertRestored(blockedBus);

    const auto refusedBlock = [&](std::uint32_t first, std::size_t count, std::size_t sectors) {
        ScriptedCardBus refused(ScriptedCardBus::Behavior::Success);
        const auto result = NtrCard::readNandSaveRange(
            refused, rwStart, windowIndex, first, count, sectors, normalRomControl, 100000, blocked.data(), blocked.size());
        assert(result.status == NtrCard::SectorReadStatus::InvalidArgument);
        assert(refused.writes.empty());
        assert(refused.commandWrites.empty());
    };
    refusedBlock(2, 8, 4);
    refusedBlock(0, 6, 4);
    refusedBlock(0, 8, 3);
    refusedBlock(0, 8, NtrCard::maxNandBlockSectors * 2);
    refusedBlock(0, 8, 0);

    StalledCardBus idle;
    const auto idleRecovery = NtrCard::recoverStalledController(idle, 1000);
    assert(!idleRecovery.attempted);
    assert(!idleRecovery.cleared);
    assert(idleRecovery.drainedWords == 0);
    assert(idle.writes == 0);

    StalledCardBus ownerFinishes;
    ownerFinishes.remaining            = 40;
    ownerFinishes.clearsWhenIrqEnabled = true;
    const auto ownerRecovery           = NtrCard::recoverStalledController(ownerFinishes, 1000);
    assert(ownerRecovery.attempted);
    assert(ownerRecovery.restoredTransferIrq);
    assert(ownerRecovery.cleared);
    assert(ownerRecovery.drainedWords == 0);
    assert(ownerFinishes.mCardControl == 0xC000);

    StalledCardBus wedged;
    wedged.remaining          = 40;
    const auto wedgedRecovery = NtrCard::recoverStalledController(wedged, 1000);
    assert(wedgedRecovery.attempted);
    assert(wedgedRecovery.restoredTransferIrq);
    assert(wedgedRecovery.cleared);
    assert(wedgedRecovery.drainedWords == 40);
    assert((wedgedRecovery.romControlBefore & StalledCardBus::busy) != 0);
    assert((wedgedRecovery.romControlAfter & StalledCardBus::busy) == 0);

    assert(NtrCard::tunedRomControl(normalRomControl) == normalRomControl);

    ScriptedCardBus irqCleared(ScriptedCardBus::Behavior::IrqCleared);
    irqCleared.failSectorIndex = 5;
    const auto irqClearedResult =
        NtrCard::readNandSaveRange(irqCleared, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(irqClearedResult.status == NtrCard::SectorReadStatus::ControllerStateChanged);
    assert(irqClearedResult.stage == NtrCard::NandReadStage::ReadSector);
    assert(irqClearedResult.observedMCardControl == 0x8000);

    assert(irqClearedResult.drainedWords == NtrCard::sectorWords);

    assert(irqClearedResult.returnToRomAttempted);
    assert(irqClearedResult.returnToRomStatus == NtrCard::SectorReadStatus::Ok);
    assert(irqClearedResult.recoveredToRomMode);
    assert(irqCleared.commandWrites.back()[0] == 0x8B);

    assert(irqCleared.mCardControl == 0x8000);
    for (const auto& command : irqCleared.commandWrites) {
        assert(command[0] == 0xB2 || command[0] == 0xB7 || command[0] == 0x8B);
    }

    assert(irqCleared.romControl == 0x27416657);
    assert(irqCleared.commandHigh == 0x11223344);
    assert(irqCleared.commandLow == 0x55667788);

    ScriptedCardBus disabledCleanup(ScriptedCardBus::Behavior::IrqCleared);
    disabledCleanup.failSectorIndex = 5;
    disabledCleanup.mCardControl    = 0x4000;
    const auto disabledCleanupResult =
        NtrCard::readNandSaveRange(disabledCleanup, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, burst.data(), burst.size());
    assert(disabledCleanupResult.status == NtrCard::SectorReadStatus::ControllerDisabled);
    assert(disabledCleanupResult.stage == NtrCard::NandReadStage::Preflight);
    assert(disabledCleanup.commandWrites.empty());

    std::vector<std::uint32_t> foreignBurst(burstSectors * NtrCard::sectorWords, 0);
    ScriptedCardBus foreignIdleRange(ScriptedCardBus::Behavior::Success);
    foreignIdleRange.romControl       = 0x20180000;
    const auto foreignIdleRangeResult = NtrCard::readNandSaveRange(
        foreignIdleRange, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, foreignBurst.data(), foreignBurst.size());
    assert(foreignIdleRangeResult.status == NtrCard::SectorReadStatus::Ok);
    assert(foreignIdleRangeResult.stage == NtrCard::NandReadStage::Complete);
    assert(foreignIdleRangeResult.sectorsRead == burstSectors);

    assert(foreignIdleRangeResult.idleConfigDiffers);
    assert(foreignIdleRange.romControl == 0x20180000);

    ScriptedCardBus nativeIdleRange(ScriptedCardBus::Behavior::Success);
    const auto nativeIdleRangeResult = NtrCard::readNandSaveRange(
        nativeIdleRange, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, 100000, foreignBurst.data(), foreignBurst.size());
    assert(nativeIdleRangeResult.status == NtrCard::SectorReadStatus::Ok);
    assert(!nativeIdleRangeResult.idleConfigDiffers);
    assertRestored(nativeIdleRange);

    constexpr std::uint64_t commandTimeout = 200;
    {
        ScriptedCardBus longTakeover(ScriptedCardBus::Behavior::BusyThenIdle);
        longTakeover.failSectorIndex   = 5;
        longTakeover.foreignRomControl = 0x293F1FFF & ~ScriptedCardBus::busy;

        longTakeover.busyReads        = 400;
        const auto longTakeoverResult = NtrCard::readNandSaveRange(
            longTakeover, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, commandTimeout, burst.data(), burst.size());
        assert(longTakeoverResult.status == NtrCard::SectorReadStatus::ControllerStateChanged);
        assert(longTakeoverResult.returnToRomAttempted);
        assert(longTakeoverResult.returnToRomStatus == NtrCard::SectorReadStatus::Ok);
        assert(longTakeoverResult.recoveredToRomMode);
        assert(longTakeover.commandWrites.back()[0] == 0x8B);
    }

    {
        ScriptedCardBus neverIdle(ScriptedCardBus::Behavior::BusyThenIdle);
        neverIdle.failSectorIndex   = 5;
        neverIdle.foreignRomControl = 0x293F1FFF & ~ScriptedCardBus::busy;
        neverIdle.busyReads         = static_cast<std::size_t>(-1);
        const auto neverIdleResult  = NtrCard::readNandSaveRange(
            neverIdle, rwStart, windowIndex, 0, burstSectors, 1, normalRomControl, commandTimeout, burst.data(), burst.size());
        assert(neverIdleResult.status == NtrCard::SectorReadStatus::ControllerStateChanged);
        assert(!neverIdleResult.returnToRomAttempted);
        assert(!neverIdleResult.recoveredToRomMode);
        assert(neverIdle.commandWrites.back()[0] == 0xB7);
    }

    {
        ScriptedCardBus quiet(ScriptedCardBus::Behavior::Success);
        assert(NtrCard::waitForBusQuiet(quiet, 10, 1000));

        ScriptedCardBus busyBus(ScriptedCardBus::Behavior::Success);
        busyBus.romControl |= ScriptedCardBus::busy;
        assert(!NtrCard::waitForBusQuiet(busyBus, 10, 200));
    }

    {
        PollCardBus poll(PollCardBus::Behavior::Poll);
        const auto observed = NtrCard::waitForCardPoll(poll, 5, 1000);
        assert(observed.status == NtrCard::CardPollStatus::Observed);
        assert((observed.observedRomControl & PollCardBus::busy) == 0);

        PollCardBus wrongBlock(PollCardBus::Behavior::WrongBlock);
        const auto missed = NtrCard::waitForCardPoll(wrongBlock, 5, 100);
        assert(missed.status == NtrCard::CardPollStatus::Timeout);

        PollCardBus reset(PollCardBus::Behavior::Reset);
        const auto resetResult = NtrCard::waitForCardPoll(reset, 5, 1000);
        assert(resetResult.status == NtrCard::CardPollStatus::ControllerReset);

        PollCardBus disabled(PollCardBus::Behavior::Poll);
        disabled.mCardControl     = 0x4000;
        const auto disabledResult = NtrCard::waitForCardPoll(disabled, 5, 1000);
        assert(disabledResult.status == NtrCard::CardPollStatus::ControllerDisabled);
    }
}
