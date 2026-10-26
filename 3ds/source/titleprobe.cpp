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

#include "titleprobe.hpp"
#include "dscard.hpp"
#include "paths.hpp"
#include "titlequirks.hpp"
#include <3ds.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <format>
#include <memory>
#include <new>

namespace {

    std::u16string boundedU16String(const u16* data, size_t maxLen)
    {
        size_t len = 0;
        while (len < maxLen && data[len] != 0) {
            len++;
        }
        return std::u16string((const char16_t*)data, len);
    }

    bool probeCtr(u64 id, FS_MediaType media, u8* productCode, bool& accessibleSave, bool& gba, bool& accessibleExtdata,
        std::u16string& shortDescription, std::u16string& longDescription, std::u16string& savePath, std::u16string& extdataPath, IconStore& icons)
    {
        const u32 low  = (u32)id;
        const u32 high = (u32)(id >> 32);

        smdh_s* smdh = (id == TID_PKSM) ? loadSMDH("romfs:/PKSM.smdh") : loadSMDH(low, high, media);
        if (smdh == NULL) {
            Logging::error("Failed to load title {:X} due to smdh == NULL", id);
            return false;
        }

        shortDescription = StringUtils::removeForbiddenCharacters(boundedU16String(smdh->applicationTitles[1].shortDescription, 0x40));
        longDescription  = boundedU16String(smdh->applicationTitles[1].longDescription, 0x80);
        savePath         = Paths::ctrSavePath(id, shortDescription);
        extdataPath      = Paths::ctrExtdataPath(id, shortDescription);
        AM_GetTitleProductCode(media, id, (char*)productCode);

        accessibleSave    = SaveDataSource::ctrSave(media, low, high).accessible();
        gba               = (!accessibleSave) && SaveDataSource::rawGba(media, low, high).accessible();
        accessibleExtdata = SaveDataSource::extdata(TitleQuirks::extdataIdFor(id)).accessible();

        bool loadTitle = false;
        if (accessibleSave || gba) {
            loadTitle = true;
            if (!io::directoryExists(Archive::sdmc(), savePath)) {
                Result res = io::createDirectory(Archive::sdmc(), savePath);
                if (R_FAILED(res)) {
                    loadTitle = false;
                    Logging::error("Failed to create backup directory with result 0x{:08X}.", res);
                }
            }
        }

        if (accessibleExtdata) {
            loadTitle = true;
            if (!io::directoryExists(Archive::sdmc(), extdataPath)) {
                Result res = io::createDirectory(Archive::sdmc(), extdataPath);
                if (R_FAILED(res)) {
                    loadTitle = false;
                    Logging::error("Failed to create backup directory with result 0x{:08X}.", res);
                }
            }
        }

        if (loadTitle) {
            icons.storeCtrIcon(id, smdh->bigIconData);
        }

        delete smdh;
        return loadTitle;
    }

    constexpr u64 DS_CARD_ID_MARKER = 0x0000444E00000000ULL;
    u64 dsCardId(const char* gameCode)
    {
        u32 packed = 0;
        for (int i = 0; i < 4; i++) {
            packed = (packed << 8) | (u8)gameCode[i];
        }
        return DS_CARD_ID_MARKER | packed;
    }

    bool probeCard(u64& id, FS_MediaType media, u8* productCode, bool& accessibleSave, bool& gba, bool& accessibleExtdata,
        std::u16string& shortDescription, std::u16string& longDescription, std::u16string& savePath, std::u16string& extdataPath, CardType& spiCard,
        bool& cardNandSave, IconStore& icons)
    {
        u8* headerData = new u8[DSCard::headerSize];
        Result res     = FSUSER_GetLegacyRomHeader(media, 0LL, headerData);
        if (R_FAILED(res)) {
            delete[] headerData;
            Logging::error("Failed get legacy rom header with result 0x{:08X}.", res);
            return false;
        }

        char cardTitle[14] = {0};
        char gameCode[6]   = {0};

        std::copy(headerData, headerData + 12, cardTitle);
        std::copy(headerData + 12, headerData + 16, gameCode);
        cardTitle[13] = '\0';
        gameCode[5]   = '\0';

        const DSCard::NandSave nandSave = DSCard::parseNandSave(headerData);
        DSCard::logNandSave(nandSave);

        id = dsCardId(gameCode);

        delete[] headerData;
        headerData = new u8[0x23C0];
        res        = FSUSER_GetLegacyBannerData(media, 0LL, headerData);
        if (R_FAILED(res)) {
            Logging::error("Failed get legacy banner data with result 0x{:08X}.", (u32)res);
        }
        icons.storeDsIcon(id, headerData);
        delete[] headerData;

        if (nandSave.present) {
            cardNandSave = true;
            Logging::info("Skipping SPI save detection for {}: this cart stores its save in on-cart NAND.", nandSave.gameCode);
        }
        else {
            res = SPIGetCardType(&spiCard, (gameCode[0] == 'I') ? 1 : 0);
            if (R_FAILED(res)) {
                Logging::error("Failed get SPI Card Type with result 0x{:08X}.", res);
                return false;
            }
        }

        snprintf((char*)productCode, 16, "NTR-%s", gameCode);

        shortDescription = StringUtils::removeForbiddenCharacters(StringUtils::UTF8toUTF16(cardTitle));
        longDescription  = shortDescription;
        savePath         = StringUtils::UTF8toUTF16("/3ds/Checkpoint/saves/") + StringUtils::UTF8toUTF16(gameCode) + StringUtils::UTF8toUTF16(" ") +
                   shortDescription;
        extdataPath = savePath;

        accessibleSave    = true;
        accessibleExtdata = false;
        gba               = false;

        bool loadTitle = true;
        if (!io::directoryExists(Archive::sdmc(), savePath)) {
            res = io::createDirectory(Archive::sdmc(), savePath);
            if (R_FAILED(res)) {
                loadTitle = false;
                Logging::error("Failed to create backup directory with result 0x{:08X}.", res);
            }
        }
        return loadTitle;
    }

    bool probeTwl(u64 id, u8* productCode, bool& accessibleSave, bool& gba, bool& accessibleExtdata, std::u16string& shortDescription,
        std::u16string& longDescription, std::u16string& savePath, std::u16string& extdataPath, IconStore& icons)
    {
        u8* headerData = new u8[0x3B4];
        Result res     = FSUSER_GetLegacyRomHeader(MEDIATYPE_NAND, id, headerData);
        if (R_FAILED(res)) {
            delete[] headerData;
            Logging::error("Failed get legacy rom header for title 0x{:016X} with result 0x{:08X}.", id, res);
            return false;
        }

        char cardTitle[14] = {0};
        char gameCode[6]   = {0};

        std::copy(headerData, headerData + 12, cardTitle);
        std::copy(headerData + 12, headerData + 16, gameCode);
        cardTitle[13] = '\0';
        gameCode[5]   = '\0';

        delete[] headerData;
        headerData = new u8[0x23C0];
        FSUSER_GetLegacyBannerData(MEDIATYPE_NAND, id, headerData);
        icons.storeDsIcon(id, headerData);
        delete[] headerData;

        snprintf((char*)productCode, 16, "TWL-%s", gameCode);

        shortDescription = StringUtils::removeForbiddenCharacters(StringUtils::UTF8toUTF16(cardTitle));
        longDescription  = shortDescription;
        savePath         = StringUtils::UTF8toUTF16("/3ds/Checkpoint/saves/") + StringUtils::UTF8toUTF16(gameCode) + StringUtils::UTF8toUTF16(" ") +
                   shortDescription;
        extdataPath = savePath;

        accessibleSave    = SaveDataSource::twlSave((u32)id, (u32)(id >> 32)).accessible();
        accessibleExtdata = false;
        gba               = false;

        if (!accessibleSave) {
            return false;
        }

        bool loadTitle = true;
        if (!io::directoryExists(Archive::sdmc(), savePath)) {
            res = io::createDirectory(Archive::sdmc(), savePath);
            if (R_FAILED(res)) {
                loadTitle = false;
                Logging::error("Failed to create backup directory with result 0x{:08X}.", res);
            }
        }
        return loadTitle;
    }
}

bool TitleProbe::probe(Title& title, u64 id, FS_MediaType media, FS_CardType card, IconStore& icons)
{
    u8 productCode[16]  = {0};
    bool accessibleSave = false, gba = false, accessibleExtdata = false;
    std::u16string shortDescription, longDescription, savePath, extdataPath;
    CardType spiCard  = NO_CHIP;
    bool cardNandSave = false;

    bool loadTitle;
    if (card == CARD_CTR) {
        loadTitle =
            probeCtr(id, media, productCode, accessibleSave, gba, accessibleExtdata, shortDescription, longDescription, savePath, extdataPath, icons);
    }
    else if (media == MEDIATYPE_NAND) {
        loadTitle =
            probeTwl(id, productCode, accessibleSave, gba, accessibleExtdata, shortDescription, longDescription, savePath, extdataPath, icons);
    }
    else {
        loadTitle = probeCard(id, media, productCode, accessibleSave, gba, accessibleExtdata, shortDescription, longDescription, savePath,
            extdataPath, spiCard, cardNandSave, icons);
    }

    title.load(id, productCode, accessibleSave, gba, accessibleExtdata, StringUtils::UTF16toUTF8(shortDescription),
        StringUtils::UTF16toUTF8(longDescription), savePath, extdataPath, media, card, spiCard, cardNandSave);
    title.refreshDirectories();
    return loadTitle;
}
