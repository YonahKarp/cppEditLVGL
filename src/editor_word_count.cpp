#include "editor.h"
#include "editor_word_count.h"

#include <cstdio>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

struct EditorWordCountState {
    bool worker_running = false;
    bool request_pending = false;
    bool result_ready = false;
    bool refresh_pending = false;
    uint32_t change_time = 0;
    uint64_t latest_enqueued_id = 0;
    uint64_t pending_request_id = 0;
    uint64_t result_id = 0;
    int result = 0;
    std::string request_text;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable cv;
};

static void refresh_word_count_label(EditorState& state) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d words", state.cached_word_count);
    lv_label_set_text(state.word_count_label, buffer);
}

int count_words(const char* text, int text_len) {
    int count = 0;
    bool in_word = false;
    for (int i = 0; i < text_len; i++) {
        char c = text[i];
        bool is_whitespace = (c == ' ' || c == '\n' || c == '\t' || c == '\r');
        if (is_whitespace) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            count++;
        }
    }
    return count;
}

static void ensure_word_count_state(EditorState& state) {
    if (!state.word_count) {
        state.word_count = new EditorWordCountState();
    }
}

static void word_count_worker_main(EditorState* state) {
    EditorWordCountState& worker_state = *state->word_count;
    std::unique_lock<std::mutex> lock(worker_state.mutex);
    while (true) {
        worker_state.cv.wait(lock, [&worker_state]() {
            return !worker_state.worker_running || worker_state.request_pending;
        });

        if (!worker_state.worker_running) {
            break;
        }

        std::string text = std::move(worker_state.request_text);
        uint64_t request_id = worker_state.pending_request_id;
        worker_state.request_pending = false;

        lock.unlock();
        int result = count_words(text.c_str(), static_cast<int>(text.size()));
        lock.lock();

        if (request_id == worker_state.latest_enqueued_id) {
            worker_state.result = result;
            worker_state.result_id = request_id;
            worker_state.result_ready = true;
        }
    }
}

void start_editor_background_tasks(EditorState& state) {
    ensure_word_count_state(state);
    if (state.word_count->worker_running) {
        return;
    }

    state.word_count->worker_running = true;
    state.word_count->worker = std::thread(word_count_worker_main, &state);
}

void stop_editor_background_tasks(EditorState& state) {
    if (!state.word_count) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state.word_count->mutex);
        state.word_count->worker_running = false;
        state.word_count->request_pending = false;
    }
    state.word_count->cv.notify_one();

    if (state.word_count->worker.joinable()) {
        state.word_count->worker.join();
    }

    delete state.word_count;
    state.word_count = nullptr;
}

void set_word_count(EditorState& state, int count) {
    state.cached_word_count = count;
    refresh_word_count_label(state);
}

void show_pending_word_count(EditorState& state) {
    lv_label_set_text(state.word_count_label, "...");
}

void request_word_count(EditorState& state, std::string text) {
    ensure_word_count_state(state);
    if (!state.word_count->worker_running) {
        set_word_count(state, count_words(text.c_str(), static_cast<int>(text.size())));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state.word_count->mutex);
        state.word_count->request_text = std::move(text);
        state.word_count->request_pending = true;
        state.word_count->pending_request_id = ++state.word_count->latest_enqueued_id;
        state.word_count->result_ready = false;
    }
    state.word_count->cv.notify_one();
}

void update_word_count(EditorState& state) {
    ensure_word_count_state(state);
    state.word_count->refresh_pending = true;
    state.word_count->change_time = lv_tick_get();
}

void process_pending_word_count(EditorState& state, uint32_t debounce_delay) {
    ensure_word_count_state(state);
    uint32_t now = lv_tick_get();

    if (state.word_count->refresh_pending && (now - state.word_count->change_time >= debounce_delay)) {
        const char* text = lv_textarea_get_text(state.textarea);
        request_word_count(state, text ? std::string(text) : std::string());
        state.word_count->refresh_pending = false;
    }

    int completed_count = 0;
    bool has_completed_count = false;
    {
        std::lock_guard<std::mutex> lock(state.word_count->mutex);
        if (state.word_count->result_ready &&
            state.word_count->result_id == state.word_count->latest_enqueued_id) {
            completed_count = state.word_count->result;
            state.word_count->result_ready = false;
            has_completed_count = true;
        }
    }

    if (has_completed_count) {
        set_word_count(state, completed_count);
    }
}
