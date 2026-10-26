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

#ifndef SETTINGSSCREEN_HPP
#define SETTINGSSCREEN_HPP

#include "Screen.hpp"
#include "hid.hpp"
#include <citro2d.h>
#include <memory>
#include <string>
#include <vector>

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(std::shared_ptr<Screen> parent);
    void drawTop(void) const override;
    void drawBottom(void) const override;
    void update(const InputState& input) override;
    bool allowsExit() const override { return false; }

private:

    enum RemoveKind { RM_FAVORITE = 0, RM_FILTER, RM_SAVE_FOLDER, RM_EXTDATA_FOLDER };
    struct Row {
        std::string primary;
        std::string secondary;
        u32 pip;
        int removeKind;
        u64 id;
        size_t idx;
    };

    bool sectionInteractive(size_t section) const;
    size_t contentRowCount(size_t section) const;

    void rebuildRows();

    void toggleGeneral(int idx);

    void editReceivePin();

    void editServerPort();

    void drawGeneral(void) const;
    void drawLibrary(void) const;
    void drawFolders(void) const;
    void drawNetwork(void) const;
    void drawAbout(void) const;

    void drawToggleRow(int y, const std::string& name, const std::string& sub, bool on, bool focused) const;

    void drawValueRow(int y, const std::string& name, const std::string& sub, const std::string& value, bool focused) const;
    void drawListRow(int y, const std::string& primary, const std::string& secondary, u32 pipColor, bool focused, bool removable) const;
    void drawEmptyState(const std::string& title, const std::string& body) const;
    void drawHints(int screenW, int y, const std::string& text) const;

    void drawScrollbar(int totalRows) const;

    std::shared_ptr<Screen> mParent;
    C2D_ImageTint flagTint;

    std::vector<Row> mLibraryRows;
    std::vector<Row> mFolderRows;

    Hid<HidDirection::VERTICAL, HidDirection::VERTICAL> navHid;
    bool contentFocus                 = false;
    int contentCursor                 = 0;
    int contentOffset                 = 0;
    int savedTimer                    = 0;
    static constexpr int VISIBLE_ROWS = 5;
};

#endif
