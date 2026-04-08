#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <flipper_format/flipper_format.h>

#include <lib/nfc/nfc.h>
#include <lib/nfc/protocols/mf_classic/mf_classic.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define FURNFC_FILETYPE "Flipper NFC device"

typedef enum {
    FurNfcViewMenu = 0,
    FurNfcViewTextBox,
    FurNfcViewPopup,
    FurNfcViewTextInput,
} FurNfcView;

typedef enum {
    FurNfcMenuReadTag = 0,
    FurNfcMenuChooseFile,
    FurNfcMenuSetKey,
    FurNfcMenuShowHelp,
} FurNfcMenu;

typedef enum {
    FurNfcCustomEventReadSuccess = 1,
} FurNfcCustomEvent;

typedef struct {
    Gui* gui;
    DialogsApp* dialogs;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    TextBox* text_box;
    Popup* popup;
    TextInput* text_input;
    Nfc* nfc;
    FuriThread* live_read_thread;
    FuriString* selected_path;
    FuriString* text;
    char key_input[13];
    bool custom_key_valid;
    MfClassicKey custom_key;
    volatile bool live_read_stop;
    FurNfcView current_view;
} FurNfcApp;

static const char* furnfc_ascii_type_name(MfClassicType type) {
    switch(type) {
    case MfClassicTypeMini:
        return "Mini";
    case MfClassicType1k:
        return "1K";
    case MfClassicType4k:
        return "4K";
    default:
        return "Unknown";
    }
}

static void furnfc_ascii_stop_live_read(FurNfcApp* app) {
    if(app->live_read_thread) {
        app->live_read_stop = true;
        furi_thread_join(app->live_read_thread);
        furi_thread_free(app->live_read_thread);
        app->live_read_thread = NULL;
    }
}

static bool furnfc_ascii_navigation_callback(void* context) {
    FurNfcApp* app = context;

    if(app->current_view == FurNfcViewPopup) {
        furnfc_ascii_stop_live_read(app);
        popup_reset(app->popup);
        app->current_view = FurNfcViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FurNfcViewMenu);
        return true;
    }

    if(app->current_view == FurNfcViewTextBox || app->current_view == FurNfcViewTextInput) {
        app->current_view = FurNfcViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, FurNfcViewMenu);
        return true;
    }

    return false;
}

static void furnfc_ascii_show_popup(FurNfcApp* app, const char* header, const char* text) {
    popup_reset(app->popup);
    popup_set_header(app->popup, header, 64, 5, AlignCenter, AlignTop);
    popup_set_text(app->popup, text, 64, 20, AlignCenter, AlignTop);
    app->current_view = FurNfcViewPopup;
    view_dispatcher_switch_to_view(app->view_dispatcher, FurNfcViewPopup);
}

static void furnfc_ascii_show_text(FurNfcApp* app) {
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text));
    app->current_view = FurNfcViewTextBox;
    view_dispatcher_switch_to_view(app->view_dispatcher, FurNfcViewTextBox);
}

static bool furnfc_ascii_custom_event_callback(void* context, uint32_t event) {
    FurNfcApp* app = context;

    if(event == FurNfcCustomEventReadSuccess) {
        furnfc_ascii_stop_live_read(app);
        popup_reset(app->popup);
        furnfc_ascii_show_text(app);
        return true;
    }

    return false;
}

static bool furnfc_ascii_decode_block(const MfClassicBlock* block, char* out, size_t out_size) {
    size_t out_len = 0;
    bool has_text = false;

    for(size_t i = 0; i < MF_CLASSIC_BLOCK_SIZE; i++) {
        uint8_t value = block->data[i];

        if(value == 0x00U || value == 0xFFU) continue;

        if(value >= 32U && value <= 126U) {
            if(out_len + 1U < out_size) {
                out[out_len++] = (char)value;
                has_text = true;
            }
        } else if(value == '\n' || value == '\r' || value == '\t') {
            if(out_len + 1U < out_size) {
                out[out_len++] = ' ';
            }
        }
    }

    while(out_len > 0U && out[out_len - 1U] == ' ') {
        out_len--;
    }

    out[out_len] = '\0';
    return has_text && (out_len > 0U);
}

static void furnfc_ascii_render_data(
    FurNfcApp* app,
    const MfClassicData* data,
    const char* source_kind,
    const char* source_name,
    const char* status_note) {
    furi_string_reset(app->text);

    furi_string_cat_printf(app->text, "Source: %s\n", source_kind);
    if(source_name && source_name[0]) {
        furi_string_cat_printf(app->text, "Nom: %s\n", source_name);
    }

    size_t uid_len = 0;
    const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
    if(uid && uid_len > 0) {
        furi_string_cat_str(app->text, "UID: ");
        for(size_t i = 0; i < uid_len; i++) {
            furi_string_cat_printf(app->text, "%02X", uid[i]);
            if(i + 1U < uid_len) furi_string_cat_str(app->text, " ");
        }
        furi_string_cat_str(app->text, "\n");
    }

    furi_string_cat_printf(app->text, "Type: %s\n", furnfc_ascii_type_name(data->type));
    if(status_note && status_note[0]) {
        furi_string_cat_printf(app->text, "%s\n", status_note);
    }

    furi_string_cat_str(app->text, "\nASCII decode:\n");

    bool found_text = false;
    uint16_t total_blocks = mf_classic_get_total_block_num(data->type);
    char ascii_line[MF_CLASSIC_BLOCK_SIZE + 1];

    for(uint16_t block = 1; block < total_blocks; block++) {
        if(mf_classic_is_sector_trailer(block) || !mf_classic_is_block_read(data, block)) {
            continue;
        }

        if(furnfc_ascii_decode_block(&data->block[block], ascii_line, sizeof(ascii_line))) {
            furi_string_cat_printf(app->text, "%s\n", ascii_line);
            found_text = true;
        }
    }

    if(!found_text) {
        furi_string_cat_str(app->text, "(Aucun texte ASCII lisible trouve)\n");
    }
}

static void furnfc_ascii_show_help(FurNfcApp* app) {
    furi_string_printf(
        app->text,
        "FurNFC ASCII\n"
        "\n"
        "Cette app peut:\n"
        "- lire un tag Mifare Classic\n"
        "  directement depuis le Flipper\n"
        "- decoder un fichier .nfc\n"
        "\n"
        "Le decodeur ignore les blocs\n"
        "trailer de secteur et les octets\n"
        "FF / 00 inutiles.\n"
        "\n"
        "Lecture directe:\n"
        "le mode live tente les cles par\n"
        "defaut FF FF FF FF FF FF sur tous\n"
        "les secteurs, puis ta cle perso\n"
        "si tu en as configure une.\n"
        "\n"
        "Fichier:\n"
        "choisis simplement un dump .nfc\n");
    furnfc_ascii_show_text(app);
}

static bool furnfc_ascii_decode_file(FurNfcApp* app, FuriString* path) {
    bool ok = false;
    FlipperFormat* ff = flipper_format_buffered_file_alloc(app->storage);
    FuriString* header = furi_string_alloc();
    FuriString* value = furi_string_alloc();
    FuriString* basename = furi_string_alloc();
    MfClassicData* data = mf_classic_alloc();

    furi_string_reset(app->text);

    do {
        uint32_t version = 0;

        if(!flipper_format_buffered_file_open_existing(ff, furi_string_get_cstr(path))) {
            furi_string_printf(
                app->text,
                "Impossible d'ouvrir:\n%s\n\n"
                "Place le fichier sur la carte SD\n"
                "puis relance la lecture.",
                furi_string_get_cstr(path));
            break;
        }

        if(!flipper_format_read_header(ff, header, &version) ||
           !furi_string_equal_str(header, FURNFC_FILETYPE)) {
            furi_string_set_str(app->text, "Fichier NFC invalide.");
            break;
        }

        if(!mf_classic_load(data, ff, version)) {
            furi_string_set_str(app->text, "Impossible de decoder ce dump Mifare Classic.");
            break;
        }

        if(!flipper_format_rewind(ff) || !flipper_format_read_string(ff, "Device type", value) ||
           !furi_string_equal_str(value, "Mifare Classic")) {
            furi_string_set_str(app->text, "Seuls les dumps Mifare Classic sont supportes.");
            break;
        }

        path_extract_filename(path, basename, false);
        furnfc_ascii_render_data(
            app, data, "Fichier .nfc", furi_string_get_cstr(basename), NULL);
        ok = true;
    } while(false);

    flipper_format_buffered_file_close(ff);
    mf_classic_free(data);
    flipper_format_free(ff);
    furi_string_free(header);
    furi_string_free(value);
    furi_string_free(basename);

    return ok;
}

static bool furnfc_ascii_try_read_with_key(
    FurNfcApp* app,
    const MfClassicKey* key,
    MfClassicType type,
    MfClassicData* data,
    MfClassicError* out_error) {
    MfClassicDeviceKeys keys = {};
    uint8_t sector_count = mf_classic_get_total_sectors_num(type);
    for(uint8_t sector = 0; sector < sector_count; sector++) {
        memcpy(keys.key_a[sector].data, key->data, sizeof(MfClassicKey));
        memcpy(keys.key_b[sector].data, key->data, sizeof(MfClassicKey));
        keys.key_a_mask |= (1ULL << sector);
        keys.key_b_mask |= (1ULL << sector);
    }

    *out_error = mf_classic_poller_sync_read(app->nfc, &keys, data);
    return (*out_error == MfClassicErrorNone) || (*out_error == MfClassicErrorPartialRead);
}

static int32_t furnfc_ascii_live_read_thread(void* context) {
    FurNfcApp* app = context;
    MfClassicData* data = mf_classic_alloc();
    const MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

    while(!app->live_read_stop) {
        MfClassicType type = MfClassicType1k;
        MfClassicError error = mf_classic_poller_sync_detect_type(app->nfc, &type);
        if(error != MfClassicErrorNone) {
            furi_delay_ms(250);
            continue;
        }

        data->type = type;
        bool read_ok = furnfc_ascii_try_read_with_key(app, &default_key, type, data, &error);
        const char* note = (error == MfClassicErrorPartialRead) ? "Note: lecture partielle" : NULL;

        if((!read_ok) && app->custom_key_valid) {
            mf_classic_reset(data);
            data->type = type;
            read_ok = furnfc_ascii_try_read_with_key(app, &app->custom_key, type, data, &error);
            note = (error == MfClassicErrorPartialRead) ? "Note: lecture partielle" : NULL;
        }

        if(read_ok) {
            furnfc_ascii_render_data(app, data, "Lecture directe", NULL, note);
            view_dispatcher_send_custom_event(app->view_dispatcher, FurNfcCustomEventReadSuccess);
            break;
        }

        furi_delay_ms(250);
    }

    mf_classic_free(data);
    return 0;
}

static void furnfc_ascii_start_live_read(FurNfcApp* app) {
    furnfc_ascii_stop_live_read(app);
    furnfc_ascii_show_popup(app, "Lecture NFC...", "Approche le tag\na l'arriere\n\nBack pour quitter");
    app->live_read_stop = false;
    app->live_read_thread =
        furi_thread_alloc_ex("FurNfcRead", 4096, furnfc_ascii_live_read_thread, app);
    furi_thread_start(app->live_read_thread);
}

static bool furnfc_ascii_key_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);

    if(strlen(text) != 12) {
        furi_string_set_str(error, "12 hex requis");
        return false;
    }

    for(size_t i = 0; i < 12; i++) {
        if(!isxdigit((unsigned char)text[i])) {
            furi_string_set_str(error, "Hex invalide");
            return false;
        }
    }

    return true;
}

static void furnfc_ascii_key_input_done(void* context) {
    FurNfcApp* app = context;

    for(size_t i = 0; i < sizeof(MfClassicKey); i++) {
        unsigned int value = 0;
        sscanf(&app->key_input[i * 2], "%2x", &value);
        app->custom_key.data[i] = (uint8_t)value;
    }
    app->custom_key_valid = true;

    furi_string_printf(
        app->text,
        "Cle Mifare enregistree.\n\n"
        "Valeur: %.12s\n\n"
        "La lecture live essayera d'abord\n"
        "FF FF FF FF FF FF puis cette cle.",
        app->key_input);
    furnfc_ascii_show_text(app);
}

static void furnfc_ascii_open_key_input(FurNfcApp* app) {
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Cle Mifare (12 hex)");
    text_input_set_validator(app->text_input, furnfc_ascii_key_validator, app);
    text_input_set_result_callback(
        app->text_input, furnfc_ascii_key_input_done, app, app->key_input, sizeof(app->key_input), false);
    text_input_set_minimum_length(app->text_input, 12);
    app->current_view = FurNfcViewTextInput;
    view_dispatcher_switch_to_view(app->view_dispatcher, FurNfcViewTextInput);
}

static bool furnfc_ascii_pick_file(FurNfcApp* app) {
    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".nfc", NULL);
    browser_options.base_path = EXT_PATH("nfc");
    browser_options.hide_dot_files = true;

    return dialog_file_browser_show(
        app->dialogs, app->selected_path, app->selected_path, &browser_options);
}

static void furnfc_ascii_menu_callback(void* context, uint32_t index) {
    FurNfcApp* app = context;

    if(index == FurNfcMenuReadTag) {
        furnfc_ascii_start_live_read(app);
    } else if(index == FurNfcMenuChooseFile) {
        if(furnfc_ascii_pick_file(app)) {
            furnfc_ascii_decode_file(app, app->selected_path);
            furnfc_ascii_show_text(app);
        }
    } else if(index == FurNfcMenuSetKey) {
        furnfc_ascii_open_key_input(app);
    } else if(index == FurNfcMenuShowHelp) {
        furnfc_ascii_show_help(app);
    }
}

static FurNfcApp* furnfc_ascii_alloc(void) {
    FurNfcApp* app = malloc(sizeof(FurNfcApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->text_box = text_box_alloc();
    app->popup = popup_alloc();
    app->text_input = text_input_alloc();
    app->nfc = nfc_alloc();
    app->selected_path = furi_string_alloc_set(EXT_PATH("nfc"));
    app->text = furi_string_alloc();
    memset(app->key_input, 0, sizeof(app->key_input));
    app->custom_key_valid = false;
    app->live_read_thread = NULL;
    app->live_read_stop = false;
    app->current_view = FurNfcViewMenu;

    submenu_add_item(
        app->submenu, "Lire le tag", FurNfcMenuReadTag, furnfc_ascii_menu_callback, app);
    submenu_add_item(
        app->submenu,
        "Choisir un .nfc",
        FurNfcMenuChooseFile,
        furnfc_ascii_menu_callback,
        app);
    submenu_add_item(
        app->submenu, "Cle Mifare", FurNfcMenuSetKey, furnfc_ascii_menu_callback, app);
    submenu_add_item(
        app->submenu, "Aide", FurNfcMenuShowHelp, furnfc_ascii_menu_callback, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, furnfc_ascii_custom_event_callback);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, furnfc_ascii_navigation_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, FurNfcViewMenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, FurNfcViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(
        app->view_dispatcher, FurNfcViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(
        app->view_dispatcher, FurNfcViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void furnfc_ascii_free(FurNfcApp* app) {
    if(!app) return;

    furnfc_ascii_stop_live_read(app);
    view_dispatcher_remove_view(app->view_dispatcher, FurNfcViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, FurNfcViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, FurNfcViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, FurNfcViewTextInput);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    text_box_free(app->text_box);
    popup_free(app->popup);
    text_input_free(app->text_input);
    nfc_free(app->nfc);
    furi_string_free(app->selected_path);
    furi_string_free(app->text);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t furnfc_ascii_app(void* p) {
    UNUSED(p);

    FurNfcApp* app = furnfc_ascii_alloc();
    view_dispatcher_switch_to_view(app->view_dispatcher, FurNfcViewMenu);
    view_dispatcher_run(app->view_dispatcher);
    furnfc_ascii_free(app);

    return 0;
}
