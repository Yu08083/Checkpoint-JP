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

#include "checkpoint_api.h"
#include "interpreter.h"

void CheckpointSetupFunc(Picoc* pc)
{
    (void)pc;
}

struct LibraryFunction CheckpointFunctions[] =
{

    { ckpt_titles_count,       "int titles_count(void);" },
    { ckpt_title_find,         "int title_find(char* idHex);" },
    { ckpt_title_id,           "char* title_id(int idx);" },
    { ckpt_title_name,         "char* title_name(int idx);" },
    { ckpt_title_product_code, "char* title_product_code(int idx);" },
    { ckpt_title_is_cart,      "int title_is_cart(int idx);" },
    { ckpt_title_has_save,     "int title_has_save(int idx);" },
    { ckpt_title_has_extdata,  "int title_has_extdata(int idx);" },
    { ckpt_title_backup_path,  "char* title_backup_path(int idx, int kind);" },

    { ckpt_sav_open,           "int sav_open(int titleIdx, int kind);" },

    { ckpt_sav_open_shared,    "int sav_open_shared(char* extdataIdHex);" },

    { ckpt_sav_open_extdata,   "int sav_open_extdata(int extdataId);" },

    { ckpt_sav_open_system,    "int sav_open_system(int saveId);" },
    { ckpt_sav_read,           "int sav_read(int h, char* path, char** out, int* outSize);" },
    { ckpt_sav_write,          "int sav_write(int h, char* path, char* data, int size);" },
    { ckpt_sav_delete,         "int sav_delete(int h, char* path);" },

    { ckpt_sav_mkdir,          "int sav_mkdir(int h, char* path);" },
    { ckpt_sav_rmdir,          "int sav_rmdir(int h, char* path);" },
    { ckpt_sav_rename,         "int sav_rename(int h, char* from, char* to);" },
    { ckpt_sav_list,           "struct directory* sav_list(int h, char* path);" },
    { ckpt_sav_commit,         "int sav_commit(int h);" },
    { ckpt_sav_close,          "void sav_close(int h);" },

    { ckpt_extdata_default_id, "int extdata_default_id(char* idHex);" },
    { ckpt_extdata_create,     "int extdata_create(char* idHex, int extdataId, int maxDirs, int maxFiles);" },
    { ckpt_extdata_delete,     "int extdata_delete(int extdataId);" },

    { ckpt_net_ip,             "char* net_ip(void);" },
    { ckpt_web_get,            "int web_get(char** out, int* outSize, char* url);" },

    { ckpt_web_request,        "int web_request(char* method, char* url, char* headers, char* body, int bodySize, char** out, int* outSize, char** respHeaders);" },

    { ckpt_web_upload_file,    "int web_upload_file(char* method, char* url, char* headers, char* filePath, char** out, int* outSize, char** respHeaders);" },

    { ckpt_url_encode,         "char* url_encode(char* s);" },

    { ckpt_http_header_value,  "char* http_header_value(char* headers, char* key);" },

    { ckpt_zip_dir,            "int zip_dir(char* srcDir, char* outZipPath);" },
    { ckpt_unzip,              "int unzip(char* zipPath, char* outDir);" },

    { ckpt_device_seal,        "int device_seal(char* plain, int plainSize, char* passphrase, char** out, int* outSize);" },
    { ckpt_device_unseal,      "int device_unseal(char* blob, int blobSize, char* passphrase, char** out, int* outSize);" },

    { ckpt_seal_needs_passphrase, "int seal_needs_passphrase(char* blob, int blobSize);" },

    { ckpt_read_directory,     "struct directory* read_directory(char* dir);" },
    { ckpt_delete_directory,   "void delete_directory(struct directory* dir);" },
    { ckpt_sd_mkdirs,          "int sd_mkdirs(char* path);" },
    { ckpt_sd_exists,          "int sd_exists(char* path);" },

    { ckpt_gui_message,        "void gui_message(char* text);" },
    { ckpt_gui_confirm,        "int gui_confirm(char* text);" },
    { ckpt_gui_pick_one,       "int gui_pick_one(char* prompt, char** items, int count);" },
    { ckpt_gui_pick_many,      "int gui_pick_many(char* prompt, char** items, int count, int* selected);" },
    { ckpt_gui_keyboard,       "void gui_keyboard(char* out, char* hint, int maxChars);" },

    { ckpt_gui_numpad,         "int gui_numpad(char* prompt, int min, int max);" },
    { ckpt_gui_status,         "void gui_status(char* text);" },

    { ckpt_progress_begin,     "void progress_begin(int layer, char* label, int total);" },
    { ckpt_progress_set,       "void progress_set(int layer, int done);" },
    { ckpt_progress_label,     "void progress_label(int layer, char* label);" },
    { ckpt_progress_end,       "void progress_end(int layer);" },

    { ckpt_progress_note,      "void progress_note(char* text);" },
    { ckpt_progress_clear,     "void progress_clear(void);" },

    { ckpt_json_new,             "struct JSON* json_new(void);" },
    { ckpt_json_parse,           "void json_parse(struct JSON* obj, char* data);" },
    { ckpt_json_delete,          "void json_delete(struct JSON* obj);" },
    { ckpt_json_is_valid,        "int json_is_valid(struct JSON* obj);" },
    { ckpt_json_is_int,          "int json_is_int(struct JSON* obj);" },
    { ckpt_json_is_bool,         "int json_is_bool(struct JSON* obj);" },
    { ckpt_json_is_string,       "int json_is_string(struct JSON* obj);" },
    { ckpt_json_is_array,        "int json_is_array(struct JSON* obj);" },
    { ckpt_json_is_object,       "int json_is_object(struct JSON* obj);" },
    { ckpt_json_get_int,         "int json_get_int(struct JSON* obj);" },
    { ckpt_json_get_bool,        "int json_get_bool(struct JSON* obj);" },
    { ckpt_json_get_string,      "char* json_get_string(struct JSON* obj);" },
    { ckpt_json_array_size,      "int json_array_size(struct JSON* obj);" },
    { ckpt_json_array_element,   "struct JSON* json_array_element(struct JSON* obj, int index);" },
    { ckpt_json_object_contains, "int json_object_contains(struct JSON* obj, char* name);" },
    { ckpt_json_object_element,  "struct JSON* json_object_element(struct JSON* obj, char* name);" },
    { ckpt_json_object_key,      "char* json_object_key(struct JSON* obj, int index);" },

    { ckpt_script_log,         "void script_log(char* msg);" },
    { ckpt_selected_title,     "char* selected_title(void);" },

    { ckpt_app_root,           "char* app_root(void);" },
    { NULL, NULL }
};

void PlatformLibraryInit(Picoc* pc)
{
    IncludeRegister(pc, "checkpoint.h", &CheckpointSetupFunc, &CheckpointFunctions[0],
        "struct directory { int count; char** files; };"
        "struct JSON { void* dummy; };");
}
