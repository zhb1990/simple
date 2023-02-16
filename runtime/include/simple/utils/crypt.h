#pragma once

#include <simple/config.h>

#include <array>
#include <string>
#include <string_view>

namespace simple {

SIMPLE_API void base64_encode(std::string& output, std::string_view input);
SIMPLE_API void base64_decode(std::string& output, std::string_view input);

inline constexpr size_t md5_length = 16;
using md5_data = std::array<uint8_t, md5_length>;

SIMPLE_API void md5(md5_data& output, std::string_view input);
SIMPLE_API void hmac_md5(md5_data& output, std::string_view input, std::string_view input_key);

inline constexpr size_t md5_sha1_block_size = 64u;

struct md5_context {
    uint32_t state[4]{};                 /* state */
    uint64_t count{0};                   /* number of bits, mod 2^64 */
    uint8_t buff[md5_sha1_block_size]{}; /* input buffer */

    SIMPLE_API void init();
    SIMPLE_API void transform(const uint8_t* block);
    SIMPLE_API void update(std::string_view in);
    SIMPLE_API void final(md5_data& output);
};

inline constexpr size_t sha1_length = 20;
using sha1_data = std::array<uint8_t, sha1_length>;

SIMPLE_API void sha1(sha1_data& output, std::string_view input);
SIMPLE_API void hmac_sha1(sha1_data& output, std::string_view input, std::string_view input_key);

struct sha1_context {
    uint32_t state[5]{};
    uint64_t count{};
    uint8_t buff[md5_sha1_block_size]{};

    SIMPLE_API void init();
    void transform(const uint8_t* in);
    SIMPLE_API void update(std::string_view in);
    SIMPLE_API void final(sha1_data& output);
};

}  // namespace simple
