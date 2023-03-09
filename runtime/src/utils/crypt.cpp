#include <simple/utils/crypt.h>

#include <cstring>

namespace simple {

/****** base64 ********/
static constexpr std::string_view base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

constexpr bool is_base64(uint8_t c) {
    return (c == 43 ||               // +
            (c >= 47 && c <= 57) ||  // /-9
            (c >= 65 && c <= 90) ||  // A-Z
            (c >= 97 && c <= 122));  // a-z
}

void base64_encode(std::string& output, std::string_view input) {
    output.clear();
    auto encode_sz = (input.size() + 2u) / 3u * 4u;
    output.reserve(encode_sz);
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];
    auto char_array_3_to_4 = [&]() {
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
    };

    size_t i = 0;
    for (const auto& ch : input) {
        char_array_3[i++] = ch;
        if (i != 3) continue;
        char_array_3_to_4();
        for (auto& c : char_array_4) {
            output += base64_chars[c];
        }
        i = 0;
    }

    if (i != 0) {
        for (auto j = i; j < 3u; j++) {
            char_array_3[j] = '\0';
        }
        char_array_3_to_4();
        for (size_t j = 0; j <= i; j++) {
            output += base64_chars[char_array_4[j]];
        }
        while ((i++ < 3)) {
            output += '=';
        }
    }
}

void base64_decode(std::string& output, std::string_view input) {
    uint8_t char_array_4[4], char_array_3[3];
    auto char_array_4_to_3 = [&]() {
        for (auto& c : char_array_4) {
            c = static_cast<uint8_t>(base64_chars.find(static_cast<char>(c)));
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
    };

    output.clear();
    auto decode_sz = (input.size() + 3u) / 4u * 3u;
    output.reserve(decode_sz);

    size_t i = 0;
    for (const auto& ch : input) {
        if (!is_base64(ch)) break;
        char_array_4[i++] = ch;
        if (i != 4) continue;
        char_array_4_to_3();
        for (auto& c : char_array_3) {
            output += static_cast<char>(c);
        }
        i = 0;
    }

    if (i != 0) {
        for (auto j = i; j < 4u; j++) char_array_4[j] = 0;
        char_array_4_to_3();
        for (size_t j = 0; j < i - 1; j++) {
            output += static_cast<char>(char_array_3[j]);
        }
    }
}

/****** md5 ********/

void md5(md5_data& output, std::string_view input) {
    md5_context ctx;
    ctx.init();
    ctx.update(input);
    ctx.final(output);
}

template <size_t Size>
void xor_key(uint8_t (&key)[Size], uint32_t xor_num) {
    for (size_t i = 0; i < Size; i += sizeof(uint32_t)) {
        auto& k = *reinterpret_cast<uint32_t*>(key + i);
        k ^= xor_num;
    }
}

void hmac_md5(md5_data& output, std::string_view input, std::string_view input_key) {
    uint8_t key[md5_sha1_block_size]{};
    auto key_sz = input_key.size();
    if (key_sz > md5_sha1_block_size) {
        md5_data data_key{};
        md5(data_key, input_key);
        key_sz = data_key.size();
        memcpy(key, data_key.data(), key_sz);
    } else {
        memcpy(key, input_key.data(), key_sz);
    }

    xor_key(key, 0x5c5c5c5cu);
    md5_context ctx1;
    ctx1.init();
    ctx1.update({reinterpret_cast<const char*>(key), md5_sha1_block_size});
    xor_key(key, 0x5c5c5c5c ^ 0x36363636);

    md5_context ctx2;
    ctx2.init();
    ctx2.update({reinterpret_cast<const char*>(key), md5_sha1_block_size});
    ctx2.update(input);
    md5_data data{};
    ctx2.final(data);

    ctx1.update({reinterpret_cast<const char*>(data.data()), data.size()});
    ctx1.final(output);
}

void md5_context::init() {
    count = 0;
    state[0] = 0x67452301u;
    state[1] = 0xEFCDAB89u;
    state[2] = 0x98BADCFEu;
    state[3] = 0x10325476u;
}

constexpr void put_64_bit_le(uint8_t* output, uint64_t value) {
    output[0] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[1] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[2] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[3] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[4] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[5] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[6] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[7] = (static_cast<uint8_t>(value & 0xffu));
}

constexpr void put_64_bit_be(uint8_t* output, uint64_t value) {
    output[7] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[6] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[5] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[4] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[3] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[2] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[1] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[0] = (static_cast<uint8_t>(value & 0xffu));
}

constexpr void put_32_bit_le(uint8_t* output, uint32_t value) {
    output[0] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[1] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[2] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[3] = (static_cast<uint8_t>(value & 0xffu));
}

constexpr void put_32_bit_be(uint8_t* output, uint32_t value) {
    output[3] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[2] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[1] = (static_cast<uint8_t>(value & 0xffu));
    value >>= 8;
    output[0] = (static_cast<uint8_t>(value & 0xffu));
}

constexpr uint32_t get_32_bit_le(const uint8_t* value) {
    return ((static_cast<uint32_t>(value[3]) << 8u | static_cast<uint32_t>(value[2])) << 8u | static_cast<uint32_t>(value[1]))
               << 8u |
           static_cast<uint32_t>(value[0]);
}

constexpr uint32_t get_32_bit_be(const uint8_t* value) {
    return ((static_cast<uint32_t>(value[0]) << 8u | static_cast<uint32_t>(value[1])) << 8u | static_cast<uint32_t>(value[2]))
               << 8u |
           static_cast<uint32_t>(value[3]);
}

constexpr uint32_t left_rotate(uint32_t d, uint32_t num) { return (d << num) | (d >> (32u - num)); }

constexpr size_t md5_sha1_block_size_32 = md5_sha1_block_size / 4;
constexpr char md5_sha1_padding[md5_sha1_block_size * 2] = {
    '\200', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr uint32_t md5_fn_f(uint32_t x, uint32_t y, uint32_t z) { return (((y ^ z) & x) ^ z); }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ArgumentSelectionDefects"
constexpr uint32_t md5_fn_g(uint32_t x, uint32_t y, uint32_t z) { return md5_fn_f(z, x, y); }
#pragma clang diagnostic pop

constexpr uint32_t md5_fn_h(uint32_t x, uint32_t y, uint32_t z) { return (x ^ y ^ z); }

constexpr uint32_t md5_fn_i(uint32_t x, uint32_t y, uint32_t z) { return (y ^ (x | ~z)); }

template <size_t Size>
void bytes_to_word32_le(uint32_t (&x)[Size], const uint8_t* pt) {
    for (size_t i = 0; i < Size; i++) {
        x[i] = get_32_bit_le(pt + i * 4);
    }
}

template <size_t Size>
void bytes_to_word32_be(uint32_t (&x)[Size], const uint8_t* pt) {
    for (size_t i = 0; i < Size; i++) {
        x[i] = get_32_bit_be(pt + i * 4);
    }
}

void md5_context::transform(const uint8_t* block) {
    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    uint32_t in[md5_sha1_block_size_32];
    bytes_to_word32_le(in, block);

    auto step = [](auto f, uint32_t& w, uint32_t x, uint32_t y, uint32_t z, uint32_t data, uint32_t s) {
        w += f(x, y, z) + data;
        w = left_rotate(w, s);
        w += x;
    };

    step(md5_fn_f, a, b, c, d, in[0] + 0xd76aa478, 7);
    step(md5_fn_f, d, a, b, c, in[1] + 0xe8c7b756, 12);
    step(md5_fn_f, c, d, a, b, in[2] + 0x242070db, 17);
    step(md5_fn_f, b, c, d, a, in[3] + 0xc1bdceee, 22);
    step(md5_fn_f, a, b, c, d, in[4] + 0xf57c0faf, 7);
    step(md5_fn_f, d, a, b, c, in[5] + 0x4787c62a, 12);
    step(md5_fn_f, c, d, a, b, in[6] + 0xa8304613, 17);
    step(md5_fn_f, b, c, d, a, in[7] + 0xfd469501, 22);
    step(md5_fn_f, a, b, c, d, in[8] + 0x698098d8, 7);
    step(md5_fn_f, d, a, b, c, in[9] + 0x8b44f7af, 12);
    step(md5_fn_f, c, d, a, b, in[10] + 0xffff5bb1, 17);
    step(md5_fn_f, b, c, d, a, in[11] + 0x895cd7be, 22);
    step(md5_fn_f, a, b, c, d, in[12] + 0x6b901122, 7);
    step(md5_fn_f, d, a, b, c, in[13] + 0xfd987193, 12);
    step(md5_fn_f, c, d, a, b, in[14] + 0xa679438e, 17);
    step(md5_fn_f, b, c, d, a, in[15] + 0x49b40821, 22);

    step(md5_fn_g, a, b, c, d, in[1] + 0xf61e2562, 5);
    step(md5_fn_g, d, a, b, c, in[6] + 0xc040b340, 9);
    step(md5_fn_g, c, d, a, b, in[11] + 0x265e5a51, 14);
    step(md5_fn_g, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    step(md5_fn_g, a, b, c, d, in[5] + 0xd62f105d, 5);
    step(md5_fn_g, d, a, b, c, in[10] + 0x02441453, 9);
    step(md5_fn_g, c, d, a, b, in[15] + 0xd8a1e681, 14);
    step(md5_fn_g, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    step(md5_fn_g, a, b, c, d, in[9] + 0x21e1cde6, 5);
    step(md5_fn_g, d, a, b, c, in[14] + 0xc33707d6, 9);
    step(md5_fn_g, c, d, a, b, in[3] + 0xf4d50d87, 14);
    step(md5_fn_g, b, c, d, a, in[8] + 0x455a14ed, 20);
    step(md5_fn_g, a, b, c, d, in[13] + 0xa9e3e905, 5);
    step(md5_fn_g, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    step(md5_fn_g, c, d, a, b, in[7] + 0x676f02d9, 14);
    step(md5_fn_g, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    step(md5_fn_h, a, b, c, d, in[5] + 0xfffa3942, 4);
    step(md5_fn_h, d, a, b, c, in[8] + 0x8771f681, 11);
    step(md5_fn_h, c, d, a, b, in[11] + 0x6d9d6122, 16);
    step(md5_fn_h, b, c, d, a, in[14] + 0xfde5380c, 23);
    step(md5_fn_h, a, b, c, d, in[1] + 0xa4beea44, 4);
    step(md5_fn_h, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    step(md5_fn_h, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    step(md5_fn_h, b, c, d, a, in[10] + 0xbebfbc70, 23);
    step(md5_fn_h, a, b, c, d, in[13] + 0x289b7ec6, 4);
    step(md5_fn_h, d, a, b, c, in[0] + 0xeaa127fa, 11);
    step(md5_fn_h, c, d, a, b, in[3] + 0xd4ef3085, 16);
    step(md5_fn_h, b, c, d, a, in[6] + 0x04881d05, 23);
    step(md5_fn_h, a, b, c, d, in[9] + 0xd9d4d039, 4);
    step(md5_fn_h, d, a, b, c, in[12] + 0xe6db99e5, 11);
    step(md5_fn_h, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    step(md5_fn_h, b, c, d, a, in[2] + 0xc4ac5665, 23);

    step(md5_fn_i, a, b, c, d, in[0] + 0xf4292244, 6);
    step(md5_fn_i, d, a, b, c, in[7] + 0x432aff97, 10);
    step(md5_fn_i, c, d, a, b, in[14] + 0xab9423a7, 15);
    step(md5_fn_i, b, c, d, a, in[5] + 0xfc93a039, 21);
    step(md5_fn_i, a, b, c, d, in[12] + 0x655b59c3, 6);
    step(md5_fn_i, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    step(md5_fn_i, c, d, a, b, in[10] + 0xffeff47d, 15);
    step(md5_fn_i, b, c, d, a, in[1] + 0x85845dd1, 21);
    step(md5_fn_i, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    step(md5_fn_i, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    step(md5_fn_i, c, d, a, b, in[6] + 0xa3014314, 15);
    step(md5_fn_i, b, c, d, a, in[13] + 0x4e0811a1, 21);
    step(md5_fn_i, a, b, c, d, in[4] + 0xf7537e82, 6);
    step(md5_fn_i, d, a, b, c, in[11] + 0xbd3af235, 10);
    step(md5_fn_i, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    step(md5_fn_i, b, c, d, a, in[9] + 0xeb86d391, 21);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void md5_context::update(std::string_view in) {
    auto have = static_cast<size_t>((count >> 3u) & (md5_sha1_block_size - 1u));
    auto need = md5_sha1_block_size - have;
    const auto* data = in.data();
    auto len = in.size();
    count += (static_cast<uint64_t>(len) << 3u);
    if (len >= need) {
        if (have != 0) {
            memcpy(buff + have, data, need);
            transform(buff);
            data += need;
            len -= need;
            have = 0;
        }

        /* Process data in md5_base::block_size chunks. */
        while (len >= md5_sha1_block_size) {
            transform(reinterpret_cast<const uint8_t*>(data));
            data += md5_sha1_block_size;
            len -= md5_sha1_block_size;
        }
    }

    /* Handle any remaining bytes of data. */
    if (len != 0) {
        memcpy(buff + have, data, len);
    }
}

void md5_context::final(md5_data& output) {
    uint8_t count_temp[8]{};
    put_64_bit_le(count_temp, count);

    /* Pad out to 56 mod 64. */
    auto len = md5_sha1_block_size - static_cast<size_t>((count >> 3u) & (md5_sha1_block_size - 1u));
    if (len < 1 + 8) len += md5_sha1_block_size;
    update({md5_sha1_padding, len - 8});
    update({reinterpret_cast<const char*>(count_temp), 8});

    size_t i = 0;
    for (auto& s : state) {
        put_32_bit_le(output.data() + i * 4, s);
        ++i;
    }
}

void sha1(sha1_data& output, std::string_view input) {
    sha1_context ctx;
    ctx.init();
    ctx.update(input);
    ctx.final(output);
}

void hmac_sha1(sha1_data& output, std::string_view input, std::string_view input_key) {
    uint8_t key[md5_sha1_block_size]{};
    auto key_sz = input_key.size();
    if (key_sz > md5_sha1_block_size) {
        sha1_data data_key{};
        sha1(data_key, input_key);
        key_sz = data_key.size();
        memcpy(key, data_key.data(), key_sz);
    } else {
        memcpy(key, input_key.data(), key_sz);
    }

    xor_key(key, 0x5c5c5c5cu);
    sha1_context ctx1;
    ctx1.init();
    ctx1.update({reinterpret_cast<const char*>(key), md5_sha1_block_size});
    xor_key(key, 0x5c5c5c5c ^ 0x36363636);

    sha1_context ctx2;
    sha1_data data{};
    ctx2.init();
    ctx2.update({reinterpret_cast<const char*>(key), md5_sha1_block_size});
    ctx2.update(input);
    ctx2.final(data);

    ctx1.update({reinterpret_cast<const char*>(data.data()), data.size()});
    ctx1.final(output);
}

void sha1_context::init() {
    state[0] = 0x67452301u;
    state[1] = 0xefcdab89u;
    state[2] = 0x98badcfeu;
    state[3] = 0x10325476u;
    state[4] = 0xc3d2e1f0u;
}

static uint32_t sha1_blk(const uint32_t (&block)[md5_sha1_block_size_32], size_t i) {
    return left_rotate(block[(i + 13u) & 15u] ^ block[(i + 8u) & 15u] ^ block[(i + 2u) & 15u] ^ block[i], 1u);
}

constexpr uint32_t sha1_f1(uint32_t x, uint32_t y, uint32_t w) { return ((w & (x ^ y)) ^ y); }

constexpr uint32_t sha1_f2(uint32_t x, uint32_t y, uint32_t w) { return (w ^ x ^ y); }

constexpr uint32_t sha1_f3(uint32_t x, uint32_t y, uint32_t w) { return (((w | x) & y) | (w & x)); }

void sha1_context::transform(const uint8_t* in) {
    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];

    uint32_t block[md5_sha1_block_size_32];
    bytes_to_word32_be(block, in);

    auto step0 = [&](auto fn, uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, uint32_t t, size_t i) {
        z += fn(x, y, w) + block[i] + t + left_rotate(v, 5);
        w = left_rotate(w, 30);
    };

    auto step1 = [&](auto fn, uint32_t v, uint32_t& w, uint32_t x, uint32_t y, uint32_t& z, uint32_t t, size_t i) {
        block[i] = sha1_blk(block, i);
        z += fn(x, y, w) + block[i] + t + left_rotate(v, 5);
        w = left_rotate(w, 30);
    };

    step0(sha1_f1, a, b, c, d, e, 0x5a827999u, 0);
    step0(sha1_f1, e, a, b, c, d, 0x5a827999u, 1);
    step0(sha1_f1, d, e, a, b, c, 0x5a827999u, 2);
    step0(sha1_f1, c, d, e, a, b, 0x5a827999u, 3);
    step0(sha1_f1, b, c, d, e, a, 0x5a827999u, 4);
    step0(sha1_f1, a, b, c, d, e, 0x5a827999u, 5);
    step0(sha1_f1, e, a, b, c, d, 0x5a827999u, 6);
    step0(sha1_f1, d, e, a, b, c, 0x5a827999u, 7);
    step0(sha1_f1, c, d, e, a, b, 0x5a827999u, 8);
    step0(sha1_f1, b, c, d, e, a, 0x5a827999u, 9);
    step0(sha1_f1, a, b, c, d, e, 0x5a827999u, 10);
    step0(sha1_f1, e, a, b, c, d, 0x5a827999u, 11);
    step0(sha1_f1, d, e, a, b, c, 0x5a827999u, 12);
    step0(sha1_f1, c, d, e, a, b, 0x5a827999u, 13);
    step0(sha1_f1, b, c, d, e, a, 0x5a827999u, 14);
    step0(sha1_f1, a, b, c, d, e, 0x5a827999u, 15);

    step1(sha1_f1, e, a, b, c, d, 0x5a827999u, 0);
    step1(sha1_f1, d, e, a, b, c, 0x5a827999u, 1);
    step1(sha1_f1, c, d, e, a, b, 0x5a827999u, 2);
    step1(sha1_f1, b, c, d, e, a, 0x5a827999u, 3);

    step1(sha1_f2, a, b, c, d, e, 0x6ed9eba1u, 4);
    step1(sha1_f2, e, a, b, c, d, 0x6ed9eba1u, 5);
    step1(sha1_f2, d, e, a, b, c, 0x6ed9eba1u, 6);
    step1(sha1_f2, c, d, e, a, b, 0x6ed9eba1u, 7);
    step1(sha1_f2, b, c, d, e, a, 0x6ed9eba1u, 8);
    step1(sha1_f2, a, b, c, d, e, 0x6ed9eba1u, 9);
    step1(sha1_f2, e, a, b, c, d, 0x6ed9eba1u, 10);
    step1(sha1_f2, d, e, a, b, c, 0x6ed9eba1u, 11);
    step1(sha1_f2, c, d, e, a, b, 0x6ed9eba1u, 12);
    step1(sha1_f2, b, c, d, e, a, 0x6ed9eba1u, 13);
    step1(sha1_f2, a, b, c, d, e, 0x6ed9eba1u, 14);
    step1(sha1_f2, e, a, b, c, d, 0x6ed9eba1u, 15);
    step1(sha1_f2, d, e, a, b, c, 0x6ed9eba1u, 0);
    step1(sha1_f2, c, d, e, a, b, 0x6ed9eba1u, 1);
    step1(sha1_f2, b, c, d, e, a, 0x6ed9eba1u, 2);
    step1(sha1_f2, a, b, c, d, e, 0x6ed9eba1u, 3);
    step1(sha1_f2, e, a, b, c, d, 0x6ed9eba1u, 4);
    step1(sha1_f2, d, e, a, b, c, 0x6ed9eba1u, 5);
    step1(sha1_f2, c, d, e, a, b, 0x6ed9eba1u, 6);
    step1(sha1_f2, b, c, d, e, a, 0x6ed9eba1u, 7);

    step1(sha1_f3, a, b, c, d, e, 0x8f1bbcdcu, 8);
    step1(sha1_f3, e, a, b, c, d, 0x8f1bbcdcu, 9);
    step1(sha1_f3, d, e, a, b, c, 0x8f1bbcdcu, 10);
    step1(sha1_f3, c, d, e, a, b, 0x8f1bbcdcu, 11);
    step1(sha1_f3, b, c, d, e, a, 0x8f1bbcdcu, 12);
    step1(sha1_f3, a, b, c, d, e, 0x8f1bbcdcu, 13);
    step1(sha1_f3, e, a, b, c, d, 0x8f1bbcdcu, 14);
    step1(sha1_f3, d, e, a, b, c, 0x8f1bbcdcu, 15);
    step1(sha1_f3, c, d, e, a, b, 0x8f1bbcdcu, 0);
    step1(sha1_f3, b, c, d, e, a, 0x8f1bbcdcu, 1);
    step1(sha1_f3, a, b, c, d, e, 0x8f1bbcdcu, 2);
    step1(sha1_f3, e, a, b, c, d, 0x8f1bbcdcu, 3);
    step1(sha1_f3, d, e, a, b, c, 0x8f1bbcdcu, 4);
    step1(sha1_f3, c, d, e, a, b, 0x8f1bbcdcu, 5);
    step1(sha1_f3, b, c, d, e, a, 0x8f1bbcdcu, 6);
    step1(sha1_f3, a, b, c, d, e, 0x8f1bbcdcu, 7);
    step1(sha1_f3, e, a, b, c, d, 0x8f1bbcdcu, 8);
    step1(sha1_f3, d, e, a, b, c, 0x8f1bbcdcu, 9);
    step1(sha1_f3, c, d, e, a, b, 0x8f1bbcdcu, 10);
    step1(sha1_f3, b, c, d, e, a, 0x8f1bbcdcu, 11);

    step1(sha1_f2, a, b, c, d, e, 0xca62c1d6u, 12);
    step1(sha1_f2, e, a, b, c, d, 0xca62c1d6u, 13);
    step1(sha1_f2, d, e, a, b, c, 0xca62c1d6u, 14);
    step1(sha1_f2, c, d, e, a, b, 0xca62c1d6u, 15);
    step1(sha1_f2, b, c, d, e, a, 0xca62c1d6u, 0);
    step1(sha1_f2, a, b, c, d, e, 0xca62c1d6u, 1);
    step1(sha1_f2, e, a, b, c, d, 0xca62c1d6u, 2);
    step1(sha1_f2, d, e, a, b, c, 0xca62c1d6u, 3);
    step1(sha1_f2, c, d, e, a, b, 0xca62c1d6u, 4);
    step1(sha1_f2, b, c, d, e, a, 0xca62c1d6u, 5);
    step1(sha1_f2, a, b, c, d, e, 0xca62c1d6u, 6);
    step1(sha1_f2, e, a, b, c, d, 0xca62c1d6u, 7);
    step1(sha1_f2, d, e, a, b, c, 0xca62c1d6u, 8);
    step1(sha1_f2, c, d, e, a, b, 0xca62c1d6u, 9);
    step1(sha1_f2, b, c, d, e, a, 0xca62c1d6u, 10);
    step1(sha1_f2, a, b, c, d, e, 0xca62c1d6u, 11);
    step1(sha1_f2, e, a, b, c, d, 0xca62c1d6u, 12);
    step1(sha1_f2, d, e, a, b, c, 0xca62c1d6u, 13);
    step1(sha1_f2, c, d, e, a, b, 0xca62c1d6u, 14);
    step1(sha1_f2, b, c, d, e, a, 0xca62c1d6u, 15);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

void sha1_context::update(std::string_view in) {
    auto len = in.size();
    const auto* data = in.data();
    auto used = static_cast<size_t>(count & (md5_sha1_block_size - 1u));
    count += len;
    auto need = md5_sha1_block_size - used;
    if (len >= need) {
        if (used != 0) {
            memcpy(buff + used, data, need);
            transform(buff);
            data += need;
            len -= need;
            used = 0;
        }

        /* Process data in md5_base::block_size chunks. */
        while (len >= md5_sha1_block_size) {
            transform(reinterpret_cast<const uint8_t*>(data));
            data += md5_sha1_block_size;
            len -= md5_sha1_block_size;
        }
    }

    /* Handle any remaining bytes of data. */
    if (len != 0) {
        memcpy(buff + used, data, len);
    }
}

void sha1_context::final(sha1_data& output) {
    uint8_t count_temp[8]{};
    put_64_bit_be(count_temp, (count << 3u));
    auto used = static_cast<size_t>(count & (md5_sha1_block_size - 1));
    auto len = md5_sha1_block_size - used;
    if (len < 1 + 8) len += md5_sha1_block_size;
    update({md5_sha1_padding, len - 8});
    update({reinterpret_cast<const char*>(count_temp), 8});

    size_t i = 0;
    for (auto& s : state) {
        put_32_bit_be(output.data() + i * 4, s);
        ++i;
    }
}

}  // namespace simple
