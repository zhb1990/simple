#include <simple/utils/trie_ac.h>

#include <cctype>
#include <cstring>
#include <queue>

namespace simple {

constexpr size_t trie_ac_size_default = 100000;
constexpr int32_t trie_ac_ac_undef_fail = -1;

static const char *next_utf8(const char *str, size_t size) noexcept {
    uint8_t val = str[0];
    if (val < 128u) {
        // 英文字符
        return str + 1;
    }
    size_t cnt;
    if (val < 224u) {
        // 2个字符
        cnt = 2;
    } else if (val < 240u) {
        // 3个字符
        cnt = 3;
    } else if (val < 248u) {
        // 4个字符
        cnt = 4;
    } else if (val < 252u) {
        // 5个字符
        cnt = 5;
    } else if (val < 254u) {
        // 6个字符
        cnt = 6;
    } else {
        return str + 1;
    }

    for (size_t i = 1; i < cnt; ++i) {
        val = str[i];
        if (i >= size || val >= 192 || val < 128) {
            return str + 1;
        }
    }

    return str + cnt;
}

static size_t cnt_utf8(const char *str, size_t len) noexcept {
    size_t cnt = 0;
    for (; len > 0;) {
        const auto *next = next_utf8(str, len);
        len -= (next - str);
        str = next;
        ++cnt;
    }

    return cnt;
}

bool trie_ac::in_trie(std::string_view strv) {
    auto data = get_data();
    if (!data || data->goto_t.empty()) return false;

    auto state = 0;
    for (auto ch : strv) {
        if (ch == 0) break;
        if (data->ignore_case) {
            ch = static_cast<char>(tolower(ch));
        }
        state = next_state(data->goto_t, state, ch);
        if (state < 0) state = 0;
        if (data->output_t[state] > 0) {
            return true;
        }
    }

    return false;
}

void trie_ac::replace(std::string &str) {
    auto data = get_data();
    if (!data || data->goto_t.empty()) return;

    auto state = 0;
    auto sz = str.size();
    std::vector<bool> replace_idx;
    auto add_idx = [&](size_t pos) {
        auto max_sz = data->output_t[state];
        if (max_sz > 0) {
            if (replace_idx.empty()) {
                replace_idx.resize(sz, false);
            }
            for (auto i = pos - max_sz; i < pos; ++i) {
                replace_idx[i] = true;
            }
        }
    };

    for (size_t i = 0; i < sz; ++i) {
        auto ch = str[i];
        if (ch == 0) break;
        if (data->ignore_case) {
            ch = static_cast<char>(tolower(ch));
        }
        state = next_state(data->goto_t, state, ch);
        if (state < 0) {
            state = 0;
        }
        add_idx(i + 1);
    }

    if (replace_idx.empty()) return;

    std::string temp;
    size_t last = 0;
    for (size_t start = 0; start < sz;) {
        if (replace_idx[start]) {
            temp.append(str.c_str() + last, start - last);
            auto end = start + 1;
            for (; end < sz; ++end) {
                if (!replace_idx[end]) {
                    break;
                }
            }

            auto cnt = cnt_utf8(str.c_str() + start, end - start);
            for (size_t i = 0; i < cnt; ++i) {
                temp.push_back('*');
            }

            start = end;
            last = end;
        } else {
            ++start;
        }
    }
    if (last < sz) {
        temp.append(str.c_str() + last, sz - last);
    }
    temp.swap(str);
}

void trie_ac::init(bool ignore_case) {
    if (!reserve_) {
        reserve_ = std::make_shared<ac_dfa>();
        reserve_->goto_t.reserve(trie_ac_size_default);
        reserve_->output_t.reserve(trie_ac_size_default);
    }
    reserve_->goto_t.resize(1, {});
    reserve_->output_t.resize(1, 0);
    reserve_->ignore_case = ignore_case;
    if (fail_table_.capacity() == 0) {
        fail_table_.reserve(trie_ac_size_default);
    }
    state_ = 0;
}

void trie_ac::update(std::string_view strv) {
    auto now = 0;
    for (auto ch : strv) {
        if (reserve_->ignore_case) {
            ch = static_cast<char>(tolower(ch));
        }
        const auto next = next_state(reserve_->goto_t, now, ch);
        if (next <= 0) {
            reserve_->goto_t[now].emplace(ch, ++state_);
            reserve_->goto_t.emplace_back();
            reserve_->output_t.emplace_back();
            now = state_;
        } else {
            now = next;
        }
    }

    reserve_->output_t[now] = std::max(reserve_->output_t[now], strv.size());
}

void trie_ac::final() {
    build_fail_table();
    convert_to_dfa();
    {
        std::unique_lock<std::mutex> lock(mtx_);
        reserve_.swap(data_);
    }
    if (reserve_.use_count() > 1) {
        reserve_.reset();
    }
}

std::shared_ptr<trie_ac::ac_dfa> trie_ac::get_data() {
    std::unique_lock<std::mutex> lock(mtx_);
    return data_;
}

int32_t trie_ac::next_state(goto_table &table, int32_t state, uint8_t ch) {
    if (state < 0 || static_cast<size_t>(state) >= table.size()) {
        return 0;
    }

    auto &item = table[state];
    const auto it = item.find(ch);
    if (it == item.end()) {
        if (state == 0) {
            return 0;
        }
        return -1;
    }

    return it->next;
}

void trie_ac::build_fail_table() {
    if (reserve_->goto_t.empty()) return;

    auto reset_sz = fail_table_.size();
    fail_table_.resize(reserve_->goto_t.size(), -1);
    reset_sz = std::min(reset_sz, fail_table_.size());
    if (reset_sz > 0) {
        memset(fail_table_.data(), 0xFF, reset_sz * sizeof(int32_t));
    }

    std::queue<int32_t> q;
    for (auto &it : reserve_->goto_t[0]) {
        fail_table_[it.next] = 0;
        q.push(it.next);
    }

    while (!q.empty()) {
        auto state = q.front();
        q.pop();
        for (auto &it : reserve_->goto_t[state]) {
            if (fail_table_[it.next] != trie_ac_ac_undef_fail) {
                continue;
            }
            q.push(it.next);
            auto prev_state = state;

            int32_t ret;
            do {
                prev_state = fail_table_[prev_state];
                ret = next_state(reserve_->goto_t, prev_state, it.character);
            } while (ret < 0);

            fail_table_[it.next] = ret;
            auto val = reserve_->output_t[ret];
            if (val > 0) {
                reserve_->output_t[it.next] = std::max(val, reserve_->output_t[it.next]);
            }
        }
    }
}

void trie_ac::convert_to_dfa() {
    if (reserve_->goto_t.empty() || fail_table_.empty()) {
        return;
    }

    std::queue<int32_t> q;
    for (const auto &[character, next] : reserve_->goto_t[0]) {
        q.push(next);
    }

    while (!q.empty()) {
        const auto state = q.front();
        q.pop();

        for (uint8_t ch = 0;; ++ch) {
            auto next = next_state(reserve_->goto_t, state, ch);
            if (next > 0) {
                q.push(next);
            } else {
                next = next_state(reserve_->goto_t, fail_table_[state], ch);
                if (next > 0) {
                    reserve_->goto_t[state].emplace(ch, next);
                }
            }

            if (ch == 0xFF) {
                break;
            }
        }
    }
}

}  // namespace simple
