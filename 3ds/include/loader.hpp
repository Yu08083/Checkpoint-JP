/*
 *   This file is part of Checkpoint
 *   Copyright (C) 2017-2025 Bernardo Giordano, FlagBrew
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

#ifndef LOADER_HPP
#define LOADER_HPP

#include "iconstore.hpp"
#include "title.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

struct LoadProgress {
    bool active;
    int done;
    int total;
    int percent(void) const { return total == 0 ? 0 : (done * 100) / total; }
};

class TitleCatalog {
public:
    static TitleCatalog& get(void)
    {
        static TitleCatalog instance;
        return instance;
    }

    void getTitle(Title& dst, int i, BackupKind kind);
    bool getTitleById(Title& dst, u64 id);

    bool nameById(std::string& dst, u64 id);

    bool descriptionByIndex(std::string& dst, int i, BackupKind kind);

    int indexById(u64 id, BackupKind kind);
    bool getTitleByName(Title& dst, const std::string& name);
    int getTitleCount(BackupKind kind);
    C2D_Image icon(int i, BackupKind kind);
    bool favorite(int i, BackupKind kind);

    void refreshDirectories(u64 id);
    void refreshAllDirectories(void);

    LoadProgress progress(void);

    u32 generation(void) const { return mGeneration.load(); }

    static void loadTitlesThread(void);
    static void cartScan(void);
    static void cartScanFlagTestAndSet(void);
    static void clearCartScanFlag(void);

    class CartScanPause {
    public:
        CartScanPause(void);
        ~CartScanPause();
        CartScanPause(const CartScanPause&)            = delete;
        CartScanPause& operator=(const CartScanPause&) = delete;
    };

private:
    TitleCatalog(void)                           = default;
    ~TitleCatalog(void)                          = default;
    TitleCatalog(const TitleCatalog&)            = delete;
    TitleCatalog& operator=(const TitleCatalog&) = delete;

    bool validId(u64 id);
    bool scanCard(void);

    void loadTitles(bool forceRefreshParam);
    bool isCacheFresh(void);
    void loadFromCache(std::vector<Title>& saves, std::vector<Title>& extdatas, IconStore& icons);
    void scanInstalledTitles(std::vector<Title>& saves, std::vector<Title>& extdatas, IconStore& icons);
    void appendCartTitle(std::vector<Title>& saves, std::vector<Title>& extdatas, IconStore& icons);
    void sortLists(std::vector<Title>& saves, std::vector<Title>& extdatas);
    void exportCaches(std::vector<Title>& saves, std::vector<Title>& extdatas, IconStore& icons);

    void exportTitleListCache(std::vector<Title>& list, const std::u16string& path, IconStore& icons);
    void importTitleListCache(std::vector<Title>& saves, std::vector<Title>& extdatas, IconStore& icons);

    std::vector<Title> mSaves;
    std::vector<Title> mExtdatas;

    IconStore mIcons;
    std::mutex mMutex;

    bool mForceRefresh           = false;
    std::atomic_flag mDoCartScan = ATOMIC_FLAG_INIT;

    std::atomic<int> mCartScanPaused{0};
    std::atomic<bool> mCartScanQuiet{false};

    std::atomic<bool> mLoading{false};
    std::atomic<int> mCounter{0};
    std::atomic<int> mLimit{0};
    std::atomic<u32> mGeneration{1};
};

#endif
