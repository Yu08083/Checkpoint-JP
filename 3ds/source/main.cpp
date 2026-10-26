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

#include "main.hpp"
#include "ChoiceOverlay.hpp"
#include "MainScreen.hpp"
#include "ScriptScreen.hpp"
#include "UpdateOverlay.hpp"
#include "autoupdater.hpp"
#include "backupsize.hpp"
#include "colors.hpp"
#include "configuration.hpp"
#include "ftpserver.hpp"
#include "i18n.hpp"
#include "loader.hpp"
#include "scriptlogview.hpp"
#include "scriptrunner.hpp"
#include "server.hpp"
#include "textpool.hpp"
#include "thread.hpp"
#include "transfer.hpp"
#include "transferjob.hpp"
#include "util.hpp"
#include <atomic>
#include <chrono>
#include <optional>

int main(int argc, char* argv[])
{
    auto start = std::chrono::high_resolution_clock::now();

    Result res;
    try {
        res = servicesInit();
    }
    catch (const std::exception& e) {
        res = consoleDisplayError(std::string("Error during services init. ") + e.what(), -1);
        exit(res);
    }
    catch (...) {
        res = consoleDisplayError("Unknown error during startup", -2);
        exit(res);
    }

    if (R_FAILED(res)) {

        exit(res);
    }

    const std::string executablePath = argc > 0 && argv[0] ? argv[0] : "";
    std::optional<AutoUpdater::Update> availableUpdate;
    std::atomic<bool> updateCheckFinished{!Configuration::getInstance().autoUpdate()};
    bool updatePrompted = false;
    bool shouldExit     = false;

    try {

        Transfer::sweepTempFiles();

        Colors::apply(Configuration::getInstance().theme());

        ScriptScreen::configureLogWidth();

        g_screen       = std::make_unique<MainScreen>();
        auto uiIsReady = std::chrono::high_resolution_clock::now();
        Logging::info("Loading took {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(uiIsReady - start).count());

        if (Configuration::getInstance().autoUpdate()) {

            AutoUpdater::init();
            if (!Threads::create(Threads::WORKER_STACK, [&availableUpdate, &updateCheckFinished, executablePath]() {
                    availableUpdate = AutoUpdater::check(executablePath);
                    updateCheckFinished.store(true, std::memory_order_release);
                })) {
                Logging::warning("Could not start auto-update check thread.");
                updateCheckFinished.store(true, std::memory_order_release);
            }
        }

        while (aptMainLoop() && !shouldExit) {
            touchPosition touch;
            hidScanInput();
            hidTouchRead(&touch);

            if (hidKeysDown() & KEY_START) {
                if (g_screen->allowsExit() && !TitleCatalog::get().progress().active && !TransferJob::get().active() &&
                    !ScriptRunner::get().active() && !AutoUpdater::busy()) {
                    break;
                }
            }

            static int scriptCancelHold = 0;
            if (ScriptRunner::get().active() && (hidKeysHeld() & KEY_B)) {
                if ((hidKeysDown() & KEY_B) && g_screen->hasOverlay()) {

                    scriptCancelHold = 0;
                }
                else if (++scriptCancelHold >= 45 && !ScriptRunner::get().cancelRequested()) {
                    ScriptRunner::get().requestCancel();
                }
            }
            else {
                scriptCancelHold = 0;
            }

            static int scrollHold = 0;
            if (ScriptScreen::showing()) {

                const u32 kDown = hidKeysDown();
                if ((kDown & (KEY_DLEFT | KEY_DRIGHT)) && !g_screen->hasOverlay()) {
                    ScriptLogView::get().scrollPages((kDown & KEY_DLEFT) ? -1 : 1);
                }

                u32 kHeld = hidKeysHeld() & (KEY_L | KEY_R);
                if (!g_screen->hasOverlay()) {
                    kHeld |= hidKeysHeld() & (KEY_DUP | KEY_DDOWN);
                }
                if (kHeld) {

                    constexpr int DELAY = 20, RATE = 2;
                    if (scrollHold == 0 || (scrollHold >= DELAY && (scrollHold - DELAY) % RATE == 0)) {
                        ScriptLogView::get().scrollLines((kHeld & (KEY_DUP | KEY_L)) ? 1 : -1);
                    }
                    scrollHold++;
                }
                else {
                    scrollHold = 0;
                }
            }
            else {
                scrollHold = 0;
            }

            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            g_screen->doDrawTop();
            C2D_SceneBegin(g_bottom);
            g_screen->doDrawBottom();
            Gui::frameEnd();
            TextPool::get().frameTick();
            g_screen->doUpdate(InputState{touch});

            if (g_pendingScreen) {
                g_screen        = std::move(g_pendingScreen);
                g_pendingScreen = nullptr;
            }

            if (!updatePrompted && updateCheckFinished.load(std::memory_order_acquire) && availableUpdate && !g_screen->hasOverlay() &&
                !TitleCatalog::get().progress().active) {
                updatePrompted                  = true;
                const auto update               = *availableUpdate;
                auto screen                     = g_screen;
                std::shared_ptr<Overlay> prompt = std::make_shared<YesNoOverlay>(
                    *screen, i18n::t("updater.update_available", {update.version}),
                    [screen, update, executablePath, &shouldExit]() {

                        std::shared_ptr<Overlay> progress = std::make_shared<UpdateOverlay>(*screen, update, [executablePath, &shouldExit]() {
                            if (!AutoUpdater::requestRelaunch(executablePath)) {
                                Logging::warning("Update installed, but automatic relaunch is unavailable.");
                            }
                            shouldExit = true;
                        });
                        screen->setOverlay(progress);
                    },
                    [screen]() { screen->removeOverlay(); });
                g_screen->setOverlay(prompt);
            }
        }
    }
    catch (const std::exception& e) {
        consoleDisplayError(std::string("Error during main. ") + e.what(), -5);
    }
    catch (...) {
        res = consoleDisplayError("Unknown error during main", -6);
    }

    TitleCatalog::clearCartScanFlag();
    Server::requestStop();
    FTPServer::requestStop();
    BackupSizeCache::shutdownStatic();

    ScriptRunner::get().shutdown();
    Threads::exit();

    FTPServer::exit();

    exit(0);
}
