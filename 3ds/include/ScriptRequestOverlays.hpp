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

#ifndef SCRIPTREQUESTOVERLAYS_HPP
#define SCRIPTREQUESTOVERLAYS_HPP

#include "ListCursor.hpp"
#include "Overlay.hpp"
#include "ScriptTile.hpp"
#include "clickable.hpp"
#include "scriptuibridge.hpp"
#include <memory>
#include <string>
#include <vector>

class ScriptTileOverlay : public Overlay {
public:
    void drawTop(void) const override {}
    void drawBottom(void) const override;

protected:

    ScriptTileOverlay(Screen& screen, std::string title);

    virtual void drawBody(int bodyY) const = 0;
    virtual std::string hints(void) const  = 0;

    virtual std::string headerNote(void) const { return ""; }

    void answer(UiResponse resp);

    void drawWrappedBody(const std::string& text, int bodyY) const;

    std::string mTitle;
};

class ScriptMessageOverlay : public ScriptTileOverlay {
public:
    ScriptMessageOverlay(Screen& screen, std::string text);
    void update(const InputState& input) override;

private:
    void drawBody(int bodyY) const override;
    std::string hints(void) const override;

    std::string mText;
    std::unique_ptr<Clickable> mButton;
};

class ScriptConfirmOverlay : public ScriptTileOverlay {
public:
    ScriptConfirmOverlay(Screen& screen, std::string text);
    void update(const InputState& input) override;

private:
    void drawBody(int bodyY) const override;
    std::string hints(void) const override;

    std::string mText;
    std::unique_ptr<Clickable> mConfirm, mCancel;
    bool mConfirmSelected = true;
};

class ScriptListOverlay : public ScriptTileOverlay {
public:

    static constexpr size_t VISIBLE = 6;

protected:
    ScriptListOverlay(Screen& screen, std::string prompt, size_t count);

    void drawBody(int bodyY) const override;
    std::string headerNote(void) const override;

    virtual void drawRow(size_t k, int rowY, bool selected) const = 0;

    static constexpr int ROW_H = 26;

    size_t mCount;

    ListCursor mCursor;
};

class ScriptPickOneOverlay : public ScriptListOverlay {
public:
    ScriptPickOneOverlay(Screen& screen, const std::string& prompt, std::vector<std::string> items);
    void update(const InputState& input) override;

private:
    void drawRow(size_t k, int rowY, bool selected) const override;
    std::string hints(void) const override;

    std::vector<std::string> mItems;
};

class ScriptPickManyOverlay : public ScriptListOverlay {
public:
    ScriptPickManyOverlay(Screen& screen, const std::string& prompt, std::vector<std::string> items, std::vector<bool> preselected);
    void update(const InputState& input) override;

private:
    void drawRow(size_t k, int rowY, bool selected) const override;
    std::string hints(void) const override;

    std::vector<std::string> mItems;
    std::vector<bool> mSelected;
};

#endif
