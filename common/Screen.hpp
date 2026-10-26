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

#ifndef SCREEN_HPP
#define SCREEN_HPP

#include "InputState.hpp"
#include <memory>

class Overlay;

class Screen {
    friend class Overlay;

public:
    virtual ~Screen() = default;

#if defined(__3DS__)
    void doDrawTop(void) const;
    void doDrawBottom(void) const;
    virtual void drawTop(void) const    = 0;
    virtual void drawBottom(void) const = 0;
#elif defined(__SWITCH__)
    void doDraw() const;
    virtual void draw() const = 0;
#endif

    void doUpdate(const InputState&);
    virtual void update(const InputState&) = 0;

    virtual bool allowsExit() const { return true; }
    void removeOverlay() { currentOverlay.reset(); }

    bool hasOverlay() const { return currentOverlay != nullptr; }
    void setOverlay(std::shared_ptr<Overlay>& overlay) { currentOverlay = overlay; }

protected:

    mutable std::shared_ptr<Overlay> currentOverlay;
};

#endif
