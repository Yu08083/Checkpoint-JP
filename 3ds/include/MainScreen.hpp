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

#ifndef MAINSCREEN_HPP
#define MAINSCREEN_HPP

#include "BackupList.hpp"
#include "ChoiceOverlay.hpp"
#include "MessageOverlay.hpp"
#include "Screen.hpp"
#include "TransferOverlay.hpp"
#include "archive.hpp"
#include "clickable.hpp"
#include "gui.hpp"
#include "multiselection.hpp"
#include "thread.hpp"
#include "transferstatus.hpp"
#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

class Title;

class MainScreen : public Screen {
public:
    MainScreen(void);
    void drawTop(void) const override;
    void drawBottom(void) const override;
    void update(const InputState& input) override;

protected:
    int cellX(size_t i) const;
    int cellY(size_t i) const;
    void drawSelector(void) const;
    void drawTile(size_t k) const;
    void handleEvents(const InputState& input);

    void layoutActionButtons(void);
    void updateSelector(void);
    void updateButtons(void);
    void refreshTitlesFull(void);
    std::string nameFromCell(size_t index) const;
    void startTransferSend(void);

    void startTransferReceive(void);

    bool isReceiveRow(size_t row) const;
    size_t rowToCell(size_t row) const;
    size_t cellToRow(size_t cell) const;

    std::string backupConfirmMessage(const char* key) const;

    void refreshSelected(void);
    void doBackup(size_t fullIndex, size_t cellIndex);
    void doRestore(size_t fullIndex, size_t cellIndex);

    void startScriptPicker(void);

    void requestRestore(size_t cellIndex);

private:
    Hid<HidDirection::HORIZONTAL, HidDirection::VERTICAL> hid;
    std::unique_ptr<Clickable> buttonBackupAL, buttonRestoreAL;
    std::unique_ptr<Clickable> buttonSend;
    std::unique_ptr<Clickable> buttonBackupAll;
    std::unique_ptr<BackupList> directoryList;
    std::string ver;

    C2D_ImageTint flagTint;
    C2D_ImageTint checkboxTint;
    C2D_ImageTint starTint;
    int selectionTimer;
    bool transferEnabled;
    BackupKind backupKind = BackupKind::Save;

    struct SelectedTitle {
        bool valid       = false;
        size_t fullIndex = 0;
        BackupKind kind  = BackupKind::Save;
        u32 catalogGen   = 0;
        u32 sizeGen      = 0;
        u64 id           = 0;
        std::u16string rootPath;
        std::string name;
        std::string cartId;
        std::string mediaType;
        bool favorite      = false;
        bool transferRow   = false;
        size_t backupCount = 0;
        std::optional<u64> totalSize;
    };
    SelectedTitle selected;
    std::vector<u8> gridFavorites;

    TransferSnapshot mTransfer;

    int mCancelHoldFrames = 0;
};

#endif
