#include <fmt/format.h>
#include <simple/web/websocket.h>

#include <random>
#include <stdexcept>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif

#include "simple/coro/network.h"
#include "simple/utils/crypt.h"
#include "simple/web/http.h"

namespace simple {

websocket::websocket(websocket_type tp, uint32_t socket) : tp_(tp), socket_(socket) {}

template <size_t Size>
void rand_key(char (&key)[Size]) {
    thread_local std::default_random_engine en(std::random_device{}());
    thread_local std::uniform_int_distribution<uint16_t> dist(0, 255);
    for (char& i : key) {
        i = static_cast<char>(static_cast<uint8_t>(dist(en)));
    }
}

constexpr std::string_view websocket_key_name = "Sec-WebSocket-Key";
constexpr std::string_view websocket_accept_name = "Sec-WebSocket-Accept";
constexpr std::string_view connection_name = "Connection";
constexpr std::string_view upgrade_name = "Upgrade";
constexpr std::string_view upgrade_value_right = "websocket";
constexpr std::string_view handshake_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

simple::task<> websocket::handshake(std::string_view host, std::string_view uri) const {
    if (tp_ != websocket_type::client) {
        throw std::logic_error("tp_ must be websocket_type::client");
    }

    std::string key_base64;
    {
        // 发送握手请求
        char key[16];
        rand_key(key);
        base64_encode(key_base64, {key, sizeof(key)});
        auto req = std::make_shared<memory_buffer>();
        fmt::format_to(std::back_inserter(*req),
                       "GET {} HTTP/1.1\r\n"
                       "Host: {}\r\n"
                       "Connection: Upgrade\r\n"
                       "Upgrade: websocket\r\n"
                       "Sec-WebSocket-Version: 13\r\n"
                       "Sec-WebSocket-Key: {}\r\n\r\n",
                       uri, host, key_base64);

        simple::network::instance().write(socket_, req);
    }

    http::reply rep;
    co_await parser(rep, socket_);

    std::string_view websocket_accept_value;
    std::string_view connection_value;
    std::string_view upgrade_value;
    for (auto& header : rep.headers) {
        if (websocket_accept_value.empty() && header.name == websocket_accept_name) {
            websocket_accept_value = header.value;
        } else if (connection_value.empty() && header.name == connection_name) {
            connection_value = header.value;
        } else if (upgrade_value.empty() && header.name == upgrade_name) {
            upgrade_value = header.value;
        }
    }

    if (upgrade_value != upgrade_value_right) {
        throw std::logic_error("handshake upgrade_value is invalid");
    }

    if (connection_value.find(upgrade_name) == std::string_view::npos) {
        throw std::logic_error("handshake connection_value is invalid");
    }

    if (websocket_accept_value.empty()) {
        throw std::logic_error("handshake websocket_accept_value is invalid");
    }

    sha1_data digest;
    sha1_context ctx;
    ctx.init();
    ctx.update(key_base64);
    ctx.update(handshake_guid);
    ctx.final(digest);

    std::string right_accept;
    base64_encode(right_accept, {reinterpret_cast<char*>(digest.data()), digest.size()});
    if (right_accept != websocket_accept_value) {
        throw std::logic_error("handshake websocket_accept_value is invalid");
    }
}

simple::task<> websocket::handshake() const {
    if (tp_ != websocket_type::server) {
        throw std::logic_error("tp_ must be websocket_type::server");
    }

    http::request req;
    co_await parser(req, socket_);

    std::string_view websocket_key_value;
    std::string_view connection_value;
    std::string_view upgrade_value;
    for (auto& header : req.headers) {
        if (websocket_key_value.empty() && header.name == websocket_key_name) {
            websocket_key_value = header.value;
        } else if (connection_value.empty() && header.name == connection_name) {
            connection_value = header.value;
        } else if (upgrade_value.empty() && header.name == upgrade_name) {
            upgrade_value = header.value;
        }
    }

    auto& network = simple::network::instance();
    if (upgrade_value != upgrade_value_right) {
        auto reply = http::reply::stock(http::reply::status_t::bad_request);
        network.write(socket_, reply.to_buffer());
        throw std::logic_error("handshake upgrade_value is invalid");
    }

    if (connection_value.find(upgrade_name) == std::string_view::npos) {
        auto reply = http::reply::stock(http::reply::status_t::bad_request);
        network.write(socket_, reply.to_buffer());
        throw std::logic_error("handshake connection_value is invalid");
    }

    if (websocket_key_value.empty()) {
        auto reply = http::reply::stock(http::reply::status_t::bad_request);
        network.write(socket_, reply.to_buffer());
        throw std::logic_error("handshake websocket_key_value is invalid");
    }

    sha1_data digest;
    sha1_context ctx;
    ctx.init();
    ctx.update(websocket_key_value);
    ctx.update(handshake_guid);
    ctx.final(digest);

    http::reply reply;
    reply.status = http::reply::status_t::switching_protocols;
    reply.headers.resize(3);
    reply.headers[0].name = connection_name;
    reply.headers[0].value = upgrade_name;
    reply.headers[1].name = upgrade_name;
    reply.headers[1].value = upgrade_value_right;
    reply.headers[2].name = websocket_accept_name;
    base64_encode(reply.headers[2].value, {reinterpret_cast<char*>(digest.data()), digest.size()});
    network.write(socket_, reply.to_buffer());
}

constexpr uint16_t c_test_endian = 0x1234;
static const bool c_is_big_endian = (*(reinterpret_cast<const uint8_t*>(&c_test_endian)) == 0x12);

union uint64_helper {
    struct {
        uint32_t high;
        uint32_t low;
    } s;

    uint64_t v;
};

static uint64_t htonll(uint64_t val) {
    uint64_helper h;  // NOLINT
    h.v = val;
    h.s.low = htonl(h.s.low);
    // ReSharper disable once CppObjectMemberMightNotBeInitialized
    h.s.high = htonl(h.s.high);
    if (!c_is_big_endian) {
        std::swap(h.s.low, h.s.high);
    }

    return h.v;
}

static uint64_t ntohll(uint64_t val) {
    uint64_helper h;  // NOLINT
    h.v = val;
    h.s.low = ntohl(h.s.low);
    // ReSharper disable once CppObjectMemberMightNotBeInitialized
    h.s.high = ntohl(h.s.high);
    if (!c_is_big_endian) {
        std::swap(h.s.low, h.s.high);
    }

    return h.v;
}

static void umask(uint8_t* data, std::size_t len, const char (&mask)[4]) {
    for (std::size_t i = 0; i < len; ++i) {
        *(data + i) ^= *(mask + (i % 4));
    }
}

simple::task<websocket_opcode> websocket::read(memory_buffer& buf) const {
    buf.clear();
    auto& network = simple::network::instance();
    websocket_opcode last = websocket_opcode::none;
    for (;;) {
        uint8_t val;
        if (co_await network.read_size(socket_, &val, 1) == 0) {
            throw std::logic_error("recv eof");
        }

        const auto fin = ((val & 0x80) != 0);
        auto op = static_cast<websocket_opcode>(val & 0x0F);
        if (is_websocket_control(op)) {
            if (!fin) {
                throw std::logic_error("websocket read fail");
            }
        } else if ((op == websocket_opcode::continuation) == (last == websocket_opcode::none)) {
            throw std::logic_error("websocket read fail");
        }

        if (co_await network.read_size(socket_, &val, 1) == 0) {
            throw std::logic_error("recv eof");
        }

        const auto mask = ((val & 0x80) != 0);
        if (mask != (tp_ == websocket_type::server)) {
            throw std::logic_error("websocket read fail");
        }

        uint64_t payload_len;
        if (const auto len = (val & 0x7F); len < 0x7E) {
            payload_len = len;
        } else if (len == 0x7E) {
            uint16_t len_16;
            if (co_await network.read_size(socket_, &len_16, sizeof(len_16)) == 0) {
                throw std::logic_error("recv eof");
            }

            payload_len = ntohs(len_16);
        } else {
            uint64_t len_64;
            if (co_await network.read_size(socket_, &len_64, sizeof(len_64)) == 0) {
                throw std::logic_error("recv eof");
            }

            payload_len = ntohll(len_64);
        }

        char masking_key[4];
        if (mask && co_await network.read_size(socket_, masking_key, sizeof(masking_key)) == 0) {
            throw std::logic_error("recv eof");
        }

        buf.make_sure_writable(payload_len);
        auto* write_pos = buf.begin_write();
        if (co_await network.read_size(socket_, write_pos, payload_len) == 0) {
            throw std::logic_error("recv eof");
        }

        if (mask) {
            umask(write_pos, payload_len, masking_key);
        }
        buf.written(payload_len);

        if (fin) {
            if (op == websocket_opcode::continuation) {
                co_return last;
            }

            co_return op;
        }

        if (op != websocket_opcode::continuation) {
            last = op;
        }
    }
}

void websocket::write(websocket_opcode op, const void* data, size_t size) const {
    auto buf = std::make_shared<memory_buffer>();
    encode_header(*buf, op, size, true);
    encode_body(*buf, data, size);
    simple::network::instance().write(socket_, buf);
}

void websocket::write(websocket_opcode op, const void* data, size_t size, size_t payload_max) const {
    if (size <= payload_max || is_websocket_control(op)) {
        return write(op, data, size);
    }

    const char* temp = static_cast<const char*>(data);
    auto buf = std::make_shared<memory_buffer>();
    encode_header(*buf, op, payload_max, false);
    encode_body(*buf, temp, payload_max);
    size -= payload_max;
    temp += payload_max;
    while (size > payload_max) {
        encode_header(*buf, websocket_opcode::continuation, payload_max, false);
        encode_body(*buf, temp, payload_max);
        size -= payload_max;
        temp += payload_max;
    }

    encode_header(*buf, websocket_opcode::continuation, size, true);
    encode_body(*buf, temp, size);

    simple::network::instance().write(socket_, buf);
}

void websocket::encode_header(memory_buffer& buf, websocket_opcode op, size_t size, bool fin) const {
    // FIN RSV1, RSV2, RSV3 Opcode
    uint8_t val = 0;
    if (fin) val = 0x80;
    val |= static_cast<uint8_t>(op);
    buf.append(&val, sizeof(val));

    // Mask Payload length
    if (tp_ == websocket_type::client) {
        val = 0x80;
    } else {
        val = 0;
    }

    if (size < 0x7E) {
        val |= static_cast<uint8_t>(size);
        buf.append(&val, sizeof(val));
    } else if (size <= 0xFFFF) {
        val |= static_cast<uint8_t>(0x7E);
        buf.append(&val, sizeof(val));
        auto val_16 = static_cast<uint16_t>(size);
        val_16 = htons(val_16);
        buf.append(&val_16, sizeof(val_16));
    } else {
        val |= static_cast<uint8_t>(0x7F);
        buf.append(&val, sizeof(val));
        auto val_64 = htonll(size);
        buf.append(&val_64, sizeof(val_64));
    }
}

void websocket::encode_body(memory_buffer& buf, const void* data, size_t size) const {
    if (tp_ == websocket_type::client) {
        char mask[4];
        rand_key(mask);
        buf.append(mask, sizeof(mask));
        auto readable = buf.readable();
        buf.append(data, size);
        umask(buf.begin_read() + readable, size, mask);
    } else {
        buf.append(data, size);
    }
}

}  // namespace simple
