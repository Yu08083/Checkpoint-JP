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

#include "titlecache.hpp"
#include "smdh.hpp"
#include <algorithm>
#include <cstring>

namespace {

    constexpr size_t OFF_ID         = 0;
    constexpr size_t OFF_PRODUCT    = 8;
    constexpr size_t OFF_ACCESS     = 24;
    constexpr size_t OFF_ACCESS_EXT = 25;
    constexpr size_t OFF_SHORT      = 26;
    constexpr size_t OFF_LONG       = 90;
    constexpr size_t OFF_SAVE_PATH  = 218;
    constexpr size_t OFF_EXT_PATH   = 474;
    constexpr size_t OFF_MEDIA      = 730;
    constexpr size_t OFF_FS_CARD    = 731;
    constexpr size_t OFF_CARD       = 732;
    constexpr size_t OFF_ICON       = 733;

    constexpr size_t SHORT_LEN     = 0x40;
    constexpr size_t LONG_LEN      = 0x80;
    constexpr size_t PATH_LEN      = 256;
    constexpr size_t ICON_BYTES    = 0x900 * 2;
    constexpr size_t DS_ICON_BYTES = 0x400 * 2;

    void copyClamped(u8* dst, size_t offset, const std::string& src, size_t fieldLen)
    {
        size_t n = std::min(src.length(), fieldLen - 1);

        while (n > 0 && (static_cast<u8>(src[n]) & 0xC0) == 0x80) {
            n--;
        }
        std::memcpy(dst + offset, src.data(), n);
    }
}

void TitleCache::encode(u8* dst, Title& title, IconStore& icons)
{
    std::memset(dst, 0, ENTRY_SIZE);

    u64 id = title.id();

    u8 accessibleSaveRaw         = title.accessibleSave() ? 1 : (title.isGBAVC() ? 2 : 0);
    bool accessibleExtdata       = title.accessibleExtdata();
    std::string shortDescription = title.shortDescription();
    std::string longDescription  = title.longDescription();
    std::string savePath         = StringUtils::UTF16toUTF8(title.savePath());
    std::string extdataPath      = StringUtils::UTF16toUTF8(title.extdataPath());
    FS_MediaType media           = title.mediaType();
    FS_CardType cardType         = title.cardType();
    CardType card                = title.SPICardType();

    if (cardType == CARD_CTR) {

        u16 iconPixels[ICON_BYTES / sizeof(u16)];
        if (icons.copyCtrPixels(id, iconPixels)) {
            std::memcpy(dst + OFF_ICON, iconPixels, ICON_BYTES);
        }
        else {
            smdh_s* smdh = loadSMDH(title.lowId(), title.highId(), media);
            if (smdh != NULL) {
                std::memcpy(dst + OFF_ICON, smdh->bigIconData, ICON_BYTES);
            }
            delete smdh;
        }
    }
    else if (media == MEDIATYPE_NAND) {

        u16 iconPixels[DS_ICON_BYTES / sizeof(u16)];
        if (icons.copyDsPixels(id, iconPixels)) {
            std::memcpy(dst + OFF_ICON, iconPixels, DS_ICON_BYTES);
        }
    }

    std::memcpy(dst + OFF_ID, &id, sizeof(u64));
    std::memcpy(dst + OFF_PRODUCT, title.productCode, 16);
    std::memcpy(dst + OFF_ACCESS, &accessibleSaveRaw, sizeof(u8));
    std::memcpy(dst + OFF_ACCESS_EXT, &accessibleExtdata, sizeof(u8));
    copyClamped(dst, OFF_SHORT, shortDescription, SHORT_LEN);
    copyClamped(dst, OFF_LONG, longDescription, LONG_LEN);
    copyClamped(dst, OFF_SAVE_PATH, savePath, PATH_LEN);
    copyClamped(dst, OFF_EXT_PATH, extdataPath, PATH_LEN);
    std::memcpy(dst + OFF_MEDIA, &media, sizeof(u8));
    std::memcpy(dst + OFF_FS_CARD, &cardType, sizeof(u8));
    std::memcpy(dst + OFF_CARD, &card, sizeof(u8));
}

Title TitleCache::decode(const u8* src, IconStore& icons)
{
    u64 id;
    u8 productCode[16];
    u8 accessibleSaveRaw;
    bool accessibleExtdata;
    char shortDescription[SHORT_LEN];
    char longDescription[LONG_LEN];
    char savePath[PATH_LEN];
    char extdataPath[PATH_LEN];
    FS_MediaType media;
    FS_CardType cardType;
    CardType card;

    std::memcpy(&id, src + OFF_ID, sizeof(u64));
    std::memcpy(productCode, src + OFF_PRODUCT, 16);
    std::memcpy(&accessibleSaveRaw, src + OFF_ACCESS, sizeof(u8));
    std::memcpy(&accessibleExtdata, src + OFF_ACCESS_EXT, sizeof(u8));
    std::memcpy(shortDescription, src + OFF_SHORT, SHORT_LEN);
    std::memcpy(longDescription, src + OFF_LONG, LONG_LEN);
    std::memcpy(savePath, src + OFF_SAVE_PATH, PATH_LEN);
    std::memcpy(extdataPath, src + OFF_EXT_PATH, PATH_LEN);
    std::memcpy(&media, src + OFF_MEDIA, sizeof(u8));
    std::memcpy(&cardType, src + OFF_FS_CARD, sizeof(u8));
    std::memcpy(&card, src + OFF_CARD, sizeof(u8));

    shortDescription[SHORT_LEN - 1] = 0;
    longDescription[LONG_LEN - 1]   = 0;
    savePath[PATH_LEN - 1]          = 0;
    extdataPath[PATH_LEN - 1]       = 0;

    bool accessibleSave = accessibleSaveRaw & 1;
    bool saveIsGBA      = accessibleSaveRaw & 2;

    Title title;
    title.load(id, productCode, accessibleSave, saveIsGBA, accessibleExtdata, shortDescription, longDescription, StringUtils::UTF8toUTF16(savePath),
        StringUtils::UTF8toUTF16(extdataPath), media, cardType, card);

    if (cardType == CARD_CTR) {

        u16 bigIconData[0x900];
        std::memcpy(bigIconData, src + OFF_ICON, ICON_BYTES);
        icons.storeCtrIcon(id, bigIconData);
    }
    else if (media == MEDIATYPE_NAND) {
        u16 dsIconData[0x400];
        std::memcpy(dsIconData, src + OFF_ICON, DS_ICON_BYTES);
        icons.storeDsPixels(id, dsIconData);
    }

    return title;
}

u64 TitleCache::readId(const u8* src)
{
    u64 id;
    std::memcpy(&id, src + OFF_ID, sizeof(u64));
    return id;
}
