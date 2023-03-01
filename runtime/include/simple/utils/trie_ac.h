#pragma once

#include <simple/config.h>

#include <string>
#include <string_view>
// mutex
#include <mutex>
#include <vector>

namespace simple {

/// <summary>
/// simple::trie_ac ac;
/// ac.init();
/// ac.update("ra");
/// ac.update("rr");
/// ac.update("ta");
/// ac.update("aa");
/// ac.update("hello");
/// ac.final();
/// std::string s1 = "hello raarrata";
/// ac.is_taboo(s1); // true
/// ac.replace(s1); // ***** ********
///
/// ac.init(true);
/// ac.update("Ra");
/// ac.final();
/// std::string s2 = "hello raarrata";
/// ac.replace(s2); // hello **ar**ta
/// </summary>


class trie_ac {
  public:
    ~trie_ac() noexcept = default;
    trie_ac() = default;

    SIMPLE_NON_COPYABLE(trie_ac)

    SIMPLE_API bool is_taboo(std::string_view strv);

    SIMPLE_API void replace(std::string& str);

    SIMPLE_API void init(bool ignore_case = false);
    SIMPLE_API void update(std::string_view strv);
    SIMPLE_API void final();

  private:
    struct node {
        uint8_t character;
        int32_t next;
        bool operator<(const node& other) const { return character < other.character; }
    };
    using goto_table = std::vector<std::vector<node>>;
    using output_table = std::vector<size_t>;

    struct ac_dfa {
        goto_table goto_t;
        output_table output_t;
        bool ignore_case{false};
    };

    std::shared_ptr<ac_dfa> get_data();

    static int32_t next_state(goto_table& table, int32_t state, uint8_t ch);

    void build_fail_table();

    void convert_to_dfa();

    std::mutex mtx_;
    std::shared_ptr<ac_dfa> data_;
    std::shared_ptr<ac_dfa> reserve_;
    std::vector<int32_t> fail_table_;
    int32_t state_{0};
};

}  // namespace simple
