#include "qr_export.h"

#include "editor.h"
#include "theme.h"
#include "src/libs/qrcode/lv_qrcode.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct QrExportState {
    bool active = false;
    bool ascii_error = false;
    bool auto_cycle = true;
    uint32_t last_cycle_ms = 0;
    size_t current_index = 0;
    std::vector<std::string> payloads;
    std::string filename;
    std::string error_message;
    lv_obj_t* overlay = nullptr;
    lv_obj_t* title_label = nullptr;
    lv_obj_t* info_label = nullptr;
    lv_obj_t* hint_label = nullptr;
    lv_obj_t* qr_code = nullptr;
};

QrExportState g_qr_export;

std::string base64_encode(const std::string& input) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < input.size()) {
        uint32_t n = (static_cast<uint8_t>(input[i]) << 16) |
                     (static_cast<uint8_t>(input[i + 1]) << 8) |
                     static_cast<uint8_t>(input[i + 2]);
        output.push_back(chars[(n >> 18) & 0x3F]);
        output.push_back(chars[(n >> 12) & 0x3F]);
        output.push_back(chars[(n >> 6) & 0x3F]);
        output.push_back(chars[n & 0x3F]);
        i += 3;
    }

    if (i < input.size()) {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) {
            n |= static_cast<uint8_t>(input[i + 1]) << 8;
        }
        output.push_back(chars[(n >> 18) & 0x3F]);
        output.push_back(chars[(n >> 12) & 0x3F]);
        output.push_back(i + 1 < input.size() ? chars[(n >> 6) & 0x3F] : '=');
        output.push_back('=');
    }

    return output;
}

char hex_upper(uint8_t value) {
    return static_cast<char>((value < 10U) ? ('0' + value) : ('A' + (value - 10U)));
}

size_t find_max_qr_payload_len(lv_obj_t* qr_code) {
    constexpr size_t kMaxProbeBytes = 2950;
    std::string probe(kMaxProbeBytes, 'A');

    size_t low = 1;
    size_t high = kMaxProbeBytes;
    size_t best = 0;
    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        lv_result_t result = lv_qrcode_update(qr_code, probe.data(), static_cast<uint32_t>(mid));
        if (result == LV_RESULT_OK) {
            best = mid;
            low = mid + 1;
        } else {
            if (mid == 0) {
                break;
            }
            high = mid - 1;
        }
    }
    return best;
}

void append_percent_encoded_byte(std::string& out, uint8_t byte) {
    out.push_back('%');
    out.push_back(hex_upper(static_cast<uint8_t>((byte >> 4) & 0x0FU)));
    out.push_back(hex_upper(static_cast<uint8_t>(byte & 0x0FU)));
}

bool is_qr_payload_safe_ascii(uint8_t byte) {
    return byte >= 32U && byte <= 126U && byte != static_cast<uint8_t>('%') && byte != static_cast<uint8_t>('|');
}

bool encode_chunk_for_qr(
    const std::string& text,
    size_t start_index,
    size_t max_encoded_len,
    std::string& out_encoded,
    size_t& out_raw_consumed
) {
    out_encoded.clear();
    out_raw_consumed = 0;

    size_t i = start_index;
    while (i < text.size()) {
        uint8_t byte = static_cast<uint8_t>(text[i]);
        size_t add_len = is_qr_payload_safe_ascii(byte) ? 1U : 3U;
        if (out_encoded.size() + add_len > max_encoded_len) {
            break;
        }

        if (add_len == 1U) {
            out_encoded.push_back(static_cast<char>(byte));
        } else {
            append_percent_encoded_byte(out_encoded, byte);
        }
        i++;
        out_raw_consumed++;
    }

    return out_raw_consumed > 0;
}

bool build_qt2_encoded_chunks(
    const std::string& text,
    size_t qr_capacity,
    size_t filename_b64_len,
    size_t total_digits,
    std::vector<std::string>& out_chunks
) {
    out_chunks.clear();
    if (text.empty()) {
        return true;
    }

    // Header format: JT2|<index>/<total>|<filename_b64>|<chunk_encoded>
    size_t max_header_len = 4U + total_digits + 1U + total_digits + 1U + filename_b64_len + 1U;
    if (max_header_len >= qr_capacity) {
        return false;
    }

    size_t chunk_space = qr_capacity - max_header_len;
    if (chunk_space == 0) {
        return false;
    }

    size_t start = 0;
    while (start < text.size()) {
        std::string encoded;
        size_t consumed = 0;
        if (!encode_chunk_for_qr(text, start, chunk_space, encoded, consumed)) {
            return false;
        }
        out_chunks.push_back(std::move(encoded));
        start += consumed;
    }
    return true;
}

std::string basename_from_path(const std::string& path) {
    if (path.empty()) return "Untitled.txt";
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return path;
    if (slash + 1 >= path.size()) return "Untitled.txt";
    return path.substr(slash + 1);
}

bool find_non_ascii_passage(const std::string& text, size_t& out_index, std::string& out_passage) {
    for (size_t i = 0; i < text.size(); i++) {
        if (static_cast<uint8_t>(text[i]) > 127U) {
            out_index = i;
            size_t start = (i > 12) ? (i - 12) : 0;
            size_t end = std::min(text.size(), i + 13);
            std::string passage;
            for (size_t j = start; j < end; j++) {
                uint8_t c = static_cast<uint8_t>(text[j]);
                if (c >= 32U && c <= 126U) {
                    passage.push_back(static_cast<char>(c));
                } else if (c == '\n') {
                    passage += "\\n";
                } else if (c == '\t') {
                    passage += "\\t";
                } else {
                    char hex_buf[8];
                    std::snprintf(hex_buf, sizeof(hex_buf), "<%02X>", c);
                    passage += hex_buf;
                }
            }
            out_passage = passage;
            return true;
        }
    }
    return false;
}

void update_qr_export_popup_content() {
    if (!g_qr_export.active || !g_qr_export.info_label) {
        return;
    }

    if (g_qr_export.ascii_error) {
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());
        if (g_qr_export.hint_label) {
            lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        }
        return;
    }

    if (!g_qr_export.qr_code || g_qr_export.payloads.empty()) {
        return;
    }

    if (g_qr_export.current_index >= g_qr_export.payloads.size()) {
        g_qr_export.current_index = 0;
    }

    const std::string& payload = g_qr_export.payloads[g_qr_export.current_index];
    lv_result_t result = lv_qrcode_update(g_qr_export.qr_code, payload.data(), static_cast<uint32_t>(payload.size()));
    if (result != LV_RESULT_OK) {
        g_qr_export.ascii_error = true;
        g_qr_export.error_message = "Failed to generate QR code payload for this chunk.";
        if (g_qr_export.qr_code) {
            lv_obj_add_flag(g_qr_export.qr_code, LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());
        if (g_qr_export.hint_label) {
            lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        }
        return;
    }

    char info_buf[160];
    std::snprintf(
        info_buf,
        sizeof(info_buf),
        "%s | part %zu/%zu | %s",
        g_qr_export.filename.c_str(),
        g_qr_export.current_index + 1,
        g_qr_export.payloads.size(),
        g_qr_export.auto_cycle ? "auto 0.5s" : "manual"
    );
    lv_label_set_text(g_qr_export.info_label, info_buf);
    if (g_qr_export.hint_label) {
        lv_label_set_text(g_qr_export.hint_label, "Arrows: manual  Enter: resume auto  Esc: close");
    }
}

void step_qr_export(int delta) {
    if (!g_qr_export.active || g_qr_export.ascii_error || g_qr_export.payloads.empty()) {
        return;
    }
    const int count = static_cast<int>(g_qr_export.payloads.size());
    int next_index = static_cast<int>(g_qr_export.current_index) + delta;
    while (next_index < 0) next_index += count;
    next_index %= count;
    g_qr_export.current_index = static_cast<size_t>(next_index);
    update_qr_export_popup_content();
}

}  // namespace

void qr_export_open(const EditorState& editor) {
    qr_export_close();

    g_qr_export.active = true;
    g_qr_export.auto_cycle = true;
    g_qr_export.last_cycle_ms = lv_tick_get();
    g_qr_export.current_index = 0;
    g_qr_export.filename = basename_from_path(editor.current_file_path);

    const Theme& theme = get_theme(editor.dark_theme);
    lv_obj_t* parent = lv_scr_act();

    g_qr_export.overlay = lv_obj_create(parent);
    lv_obj_remove_style_all(g_qr_export.overlay);
    lv_obj_set_size(g_qr_export.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_qr_export.overlay, theme.background, 0);
    lv_obj_set_style_bg_opa(g_qr_export.overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(g_qr_export.overlay, 16, 0);
    lv_obj_set_style_pad_row(g_qr_export.overlay, 12, 0);
    lv_obj_set_flex_flow(g_qr_export.overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_qr_export.overlay, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_move_foreground(g_qr_export.overlay);

    g_qr_export.title_label = lv_label_create(g_qr_export.overlay);
    lv_obj_set_style_text_color(g_qr_export.title_label, theme.edit_text, 0);
    lv_obj_set_style_text_font(g_qr_export.title_label, &lv_font_montserrat_24, 0);

    const char* editor_text = lv_textarea_get_text(editor.textarea);
    std::string text = editor_text ? std::string(editor_text) : std::string();
    size_t non_ascii_index = 0;
    std::string non_ascii_passage;

    if (find_non_ascii_passage(text, non_ascii_index, non_ascii_passage)) {
        g_qr_export.ascii_error = true;
        lv_label_set_text(g_qr_export.title_label, "QR Export Error");

        g_qr_export.info_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_width(g_qr_export.info_label, LV_PCT(95));
        lv_label_set_long_mode(g_qr_export.info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(g_qr_export.info_label, theme.edit_text, 0);
        lv_obj_set_style_text_font(g_qr_export.info_label, &lv_font_montserrat_16, 0);
        g_qr_export.error_message =
            "Current file contains non-ASCII bytes.\n"
            "First non-ASCII byte at position " + std::to_string(non_ascii_index) +
            ".\nPassage: " + non_ascii_passage;
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());

        g_qr_export.hint_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_style_text_color(g_qr_export.hint_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(g_qr_export.hint_label, &lv_font_montserrat_14, 0);
        lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        return;
    }

    lv_label_set_text(g_qr_export.title_label, "QR Export");

    g_qr_export.qr_code = lv_qrcode_create(g_qr_export.overlay);
    int32_t min_side = std::min(lv_obj_get_width(parent), lv_obj_get_height(parent));
    int32_t qr_size = std::max(220, std::min(480, min_side - 140));
    lv_qrcode_set_size(g_qr_export.qr_code, qr_size);
    lv_qrcode_set_dark_color(g_qr_export.qr_code, theme.edit_text);
    lv_qrcode_set_light_color(g_qr_export.qr_code, theme.edit_bg);

    std::string filename_b64 = base64_encode(g_qr_export.filename);
    size_t qr_capacity = find_max_qr_payload_len(g_qr_export.qr_code);
    if (qr_capacity == 0) {
        g_qr_export.ascii_error = true;
        lv_label_set_text(g_qr_export.title_label, "QR Export Error");
        g_qr_export.info_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_width(g_qr_export.info_label, LV_PCT(95));
        lv_label_set_long_mode(g_qr_export.info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(g_qr_export.info_label, theme.edit_text, 0);
        lv_obj_set_style_text_font(g_qr_export.info_label, &lv_font_montserrat_16, 0);
        g_qr_export.error_message = "Unable to determine QR payload capacity for this display size.";
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());
        g_qr_export.hint_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_style_text_color(g_qr_export.hint_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(g_qr_export.hint_label, &lv_font_montserrat_14, 0);
        lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        return;
    }

    std::vector<std::string> encoded_chunks;
    size_t assumed_digits = 1;
    bool built = false;
    for (int i = 0; i < 8; i++) {
        if (!build_qt2_encoded_chunks(text, qr_capacity, filename_b64.size(), assumed_digits, encoded_chunks)) {
            break;
        }
        size_t total_digits = std::to_string(std::max<size_t>(1, encoded_chunks.size())).size();
        if (total_digits == assumed_digits) {
            built = true;
            break;
        }
        assumed_digits = total_digits;
    }

    if (!built) {
        g_qr_export.ascii_error = true;
        lv_label_set_text(g_qr_export.title_label, "QR Export Error");
        g_qr_export.info_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_width(g_qr_export.info_label, LV_PCT(95));
        lv_label_set_long_mode(g_qr_export.info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_color(g_qr_export.info_label, theme.edit_text, 0);
        lv_obj_set_style_text_font(g_qr_export.info_label, &lv_font_montserrat_16, 0);
        g_qr_export.error_message = "Display is too small for current QR export envelope.";
        lv_label_set_text(g_qr_export.info_label, g_qr_export.error_message.c_str());
        g_qr_export.hint_label = lv_label_create(g_qr_export.overlay);
        lv_obj_set_style_text_color(g_qr_export.hint_label, theme.text_dim, 0);
        lv_obj_set_style_text_font(g_qr_export.hint_label, &lv_font_montserrat_14, 0);
        lv_label_set_text(g_qr_export.hint_label, "Press Esc to close");
        return;
    }

    if (text.empty()) {
        g_qr_export.payloads.push_back("JT2|1/1|" + filename_b64 + "|");
    } else {
        size_t total_chunks = encoded_chunks.size();
        g_qr_export.payloads.reserve(total_chunks);
        for (size_t i = 0; i < total_chunks; i++) {
            std::string payload = "JT2|" + std::to_string(i + 1) + "/" + std::to_string(total_chunks) +
                                  "|" + filename_b64 + "|" + encoded_chunks[i];
            g_qr_export.payloads.push_back(std::move(payload));
        }
    }

    g_qr_export.info_label = lv_label_create(g_qr_export.overlay);
    lv_obj_set_style_text_color(g_qr_export.info_label, theme.edit_text, 0);
    lv_obj_set_style_text_font(g_qr_export.info_label, &lv_font_montserrat_16, 0);

    g_qr_export.hint_label = lv_label_create(g_qr_export.overlay);
    lv_obj_set_style_text_color(g_qr_export.hint_label, theme.text_dim, 0);
    lv_obj_set_style_text_font(g_qr_export.hint_label, &lv_font_montserrat_14, 0);

    update_qr_export_popup_content();
}

void qr_export_close() {
    if (g_qr_export.overlay) {
        lv_obj_delete(g_qr_export.overlay);
    }
    g_qr_export = {};
}

bool qr_export_is_active() {
    return g_qr_export.active;
}

bool qr_export_handle_event(const platform::PlatformEvent& event) {
    if (!g_qr_export.active) {
        return false;
    }

    if (event.type == platform::PlatformEvent::Type::KeyDown) {
        if (event.key == platform::KeyCode::Escape) {
            qr_export_close();
        } else if (event.key == platform::KeyCode::Left || event.key == platform::KeyCode::Up) {
            g_qr_export.auto_cycle = false;
            step_qr_export(-1);
        } else if (event.key == platform::KeyCode::Right || event.key == platform::KeyCode::Down) {
            g_qr_export.auto_cycle = false;
            step_qr_export(1);
        } else if (event.key == platform::KeyCode::Enter) {
            g_qr_export.auto_cycle = true;
            g_qr_export.last_cycle_ms = lv_tick_get();
            update_qr_export_popup_content();
        }
        return true;
    }

    if (event.type == platform::PlatformEvent::Type::TextInput ||
        event.type == platform::PlatformEvent::Type::KeyUp) {
        return true;
    }

    return false;
}

void qr_export_update_autocycle() {
    if (!g_qr_export.active || g_qr_export.ascii_error || !g_qr_export.auto_cycle || g_qr_export.payloads.size() <= 1) {
        return;
    }

    uint32_t now = lv_tick_get();
    if ((now - g_qr_export.last_cycle_ms) < 500) {
        return;
    }
    g_qr_export.last_cycle_ms = now;
    step_qr_export(1);
}
