#pragma once

#include "platform.h"

struct EditorState;

void qr_export_open(const EditorState& editor);
void qr_export_close();
bool qr_export_is_active();
bool qr_export_handle_event(const platform::PlatformEvent& event);
void qr_export_update_autocycle();
