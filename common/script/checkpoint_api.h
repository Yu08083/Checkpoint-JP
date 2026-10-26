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

#ifndef CHECKPOINT_API_H
#define CHECKPOINT_API_H

#include "picoc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CKPT_BINDING(name) void name(struct ParseState* Parser, struct Value* ReturnValue, struct Value** Param, int NumArgs)

CKPT_BINDING(ckpt_titles_count);
CKPT_BINDING(ckpt_title_find);
CKPT_BINDING(ckpt_title_id);
CKPT_BINDING(ckpt_title_name);
CKPT_BINDING(ckpt_title_product_code);
CKPT_BINDING(ckpt_title_is_cart);
CKPT_BINDING(ckpt_title_has_save);
CKPT_BINDING(ckpt_title_has_extdata);
CKPT_BINDING(ckpt_title_backup_path);

CKPT_BINDING(ckpt_read_directory);
CKPT_BINDING(ckpt_delete_directory);
CKPT_BINDING(ckpt_sd_mkdirs);
CKPT_BINDING(ckpt_sd_exists);

CKPT_BINDING(ckpt_sav_open);
CKPT_BINDING(ckpt_sav_open_shared);
CKPT_BINDING(ckpt_sav_open_extdata);
CKPT_BINDING(ckpt_sav_open_system);
CKPT_BINDING(ckpt_sav_read);
CKPT_BINDING(ckpt_sav_write);
CKPT_BINDING(ckpt_sav_delete);
CKPT_BINDING(ckpt_sav_mkdir);
CKPT_BINDING(ckpt_sav_rmdir);
CKPT_BINDING(ckpt_sav_rename);
CKPT_BINDING(ckpt_sav_list);
CKPT_BINDING(ckpt_sav_commit);
CKPT_BINDING(ckpt_sav_close);

CKPT_BINDING(ckpt_extdata_default_id);
CKPT_BINDING(ckpt_extdata_create);
CKPT_BINDING(ckpt_extdata_delete);

CKPT_BINDING(ckpt_net_ip);
CKPT_BINDING(ckpt_web_get);
CKPT_BINDING(ckpt_web_request);
CKPT_BINDING(ckpt_web_upload_file);
CKPT_BINDING(ckpt_url_encode);
CKPT_BINDING(ckpt_http_header_value);

CKPT_BINDING(ckpt_device_seal);
CKPT_BINDING(ckpt_device_unseal);
CKPT_BINDING(ckpt_seal_needs_passphrase);

CKPT_BINDING(ckpt_zip_dir);
CKPT_BINDING(ckpt_unzip);

CKPT_BINDING(ckpt_gui_message);
CKPT_BINDING(ckpt_gui_confirm);
CKPT_BINDING(ckpt_gui_pick_one);
CKPT_BINDING(ckpt_gui_pick_many);
CKPT_BINDING(ckpt_gui_keyboard);
CKPT_BINDING(ckpt_gui_numpad);
CKPT_BINDING(ckpt_gui_status);

CKPT_BINDING(ckpt_progress_begin);
CKPT_BINDING(ckpt_progress_set);
CKPT_BINDING(ckpt_progress_label);
CKPT_BINDING(ckpt_progress_end);
CKPT_BINDING(ckpt_progress_note);
CKPT_BINDING(ckpt_progress_clear);

CKPT_BINDING(ckpt_json_new);
CKPT_BINDING(ckpt_json_parse);
CKPT_BINDING(ckpt_json_delete);
CKPT_BINDING(ckpt_json_is_valid);
CKPT_BINDING(ckpt_json_is_int);
CKPT_BINDING(ckpt_json_is_bool);
CKPT_BINDING(ckpt_json_is_string);
CKPT_BINDING(ckpt_json_is_array);
CKPT_BINDING(ckpt_json_is_object);
CKPT_BINDING(ckpt_json_get_int);
CKPT_BINDING(ckpt_json_get_bool);
CKPT_BINDING(ckpt_json_get_string);
CKPT_BINDING(ckpt_json_array_size);
CKPT_BINDING(ckpt_json_array_element);
CKPT_BINDING(ckpt_json_object_contains);
CKPT_BINDING(ckpt_json_object_element);
CKPT_BINDING(ckpt_json_object_key);

CKPT_BINDING(ckpt_script_log);
CKPT_BINDING(ckpt_selected_title);
CKPT_BINDING(ckpt_app_root);

void ckpt_sav_close_all(void);

void ckpt_script_abort_request(void);
int ckpt_script_abort_requested(void);
void ckpt_script_abort_reset(void);

void ckpt_script_lower_priority(void);

#ifdef __cplusplus
}
#endif

#endif
