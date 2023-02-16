#include "ssl_client_impl.h"

#include <simple/log/log.h>
#include <simple/net/socket_system.h>

#if defined(_WIN32)
#include <wincrypt.h>
#endif

#include <asio/connect.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/awaitable_operators.hpp>
#include <asio/ssl/host_name_verification.hpp>

namespace simple {

ssl_client_impl::ssl_client_impl(uint32_t socket_id)  // NOLINT(cppcoreguidelines-pro-type-member-init)
    : socket_base(socket_id),
      ctx_(asio::ssl::context::sslv23),
      socket_(socket_system::instance().context(), ctx_),
      write_blocker_(socket_.get_executor()),
      connect_(socket_.get_executor()) {}

constexpr ssl_client_impl::asio_token use_awaitable_as_tuple;

#if defined(_WIN32)

static std::error_code add_cert_for_store(X509_STORE* store, const char* name) {
    std::ignore = GetLastError();
    const auto sys_store = CertOpenSystemStoreA(0, name);
    if (!sys_store) {
        return {static_cast<int>(GetLastError()), std::system_category()};
    }

    ERR_clear_error();
    PCCERT_CONTEXT ctx = nullptr;
    std::error_code ec;
    while ((ctx = CertEnumCertificatesInStore(sys_store, ctx))) {
        if (X509* x509 = d2i_X509(nullptr, const_cast<unsigned char const**>(&ctx->pbCertEncoded),
                                  static_cast<long>(ctx->cbCertEncoded))) {
            X509_STORE_add_cert(store, x509);
            X509_free(x509);
        } else {
            const auto err = ERR_get_error();
#if (OPENSSL_VERSION_NUMBER >= 0x30000000L)
            if (ERR_SYSTEM_ERROR(err)) {
                ec = {ERR_GET_REASON(err), asio::error::get_system_category()};
                break;
            }
#endif  // (OPENSSL_VERSION_NUMBER >= 0x30000000L)
            ec = {static_cast<int>(err), asio::error::get_ssl_category()};
            break;
        }
    }

    CertCloseStore(sys_store, 0);
    return ec;
}

#endif

void ssl_client_impl::start(const std::string& host, const std::string& service, const asio_timer::duration& timeout,
                            const std::string& verify, bool ignore_cert) {
    using namespace asio::experimental::awaitable_operators;
    info("ssl client {} start", socket_id_);

    std::error_code ec;
    socket_.set_verify_mode(asio::ssl::verify_peer, ec);
    if (ec) return stop(ec);

    if (!verify.empty()) {
        ctx_.load_verify_file(verify, ec);
    } else {
#if defined(_WIN32)
        auto* store = SSL_CTX_get_cert_store(ctx_.native_handle());
        ec = add_cert_for_store(store, "CA");
        if (ec) return stop(ec);
        ec = add_cert_for_store(store, "AuthRoot");
        if (ec) return stop(ec);
        ec = add_cert_for_store(store, "ROOT");
#else
        ctx_.set_default_verify_paths(ec);
#endif
    }
    if (ec) return stop(ec);

    if (ignore_cert) {
        socket_.set_verify_callback([](bool, asio::ssl::verify_context&) { return true; }, ec);
    } else {
        socket_.set_verify_callback(asio::ssl::host_name_verification(host), ec);
    }
    if (ec) return stop(ec);

    write_blocker_.expires_at(asio_timer::clock_type::time_point::max());
    auto self = shared_from_this();
    auto& system = socket_system::instance();
    system.insert(socket_id_, self);

    co_spawn(
        system.context(),
        [self, timeout, host, service, this]() -> asio::awaitable<void> {
            std::ignore = self;
            co_await (co_connect(host, service) || co_timeout(timeout));
        },
        asio::detached);
}

void ssl_client_impl::stop(const std::error_code& ec) {
    auto& socket_raw = socket_.next_layer();
    if (!socket_raw.is_open()) return;

    info("ssl client {} stop", socket_id_);
    std::error_code ignore;
    socket_.shutdown(ignore);
    socket_raw.close(ignore);
    try {
        write_blocker_.cancel();
        connect_.cancel();
    } catch (...) {
    }
    auto& system = socket_system::instance();
    system.hand_stop(socket_id_, ec);
    system.erase(socket_id_);
}

void ssl_client_impl::write(const memory_buffer_ptr& ptr) {
    trace_write_queue(ptr->readable());
    write_deque_.emplace_back(ptr);
    try {
        write_blocker_.cancel();
    } catch (...) {
    }
}

void ssl_client_impl::no_delay(bool on) {
    std::error_code ec;
    socket_.next_layer().set_option(tcp::no_delay{on}, ec);
}

asio::awaitable<void> ssl_client_impl::co_connect(const std::string& host, const std::string& service) {
    using tcp_resolver = asio_token::as_default_on_t<tcp::resolver>;
    auto& system = socket_system::instance();
    auto& context = system.context();

    // 域名解析
    tcp_resolver resolver(context);
    auto [ec, results] = co_await resolver.async_resolve(host, service);
    if (ec) {
        stop(ec);
        co_return;
    }

    info("tcp client {} resolve", socket_id_);

    // 建立tcp连接
    auto& socket_raw = socket_.next_layer();
    std::tie(ec, std::ignore) = co_await async_connect(socket_raw, results, use_awaitable_as_tuple);
    if (ec) {
        stop(ec);
        co_return;
    }

    info("tcp client {} connect", socket_id_);

    // ssl握手
    std::tie(ec) = co_await socket_.async_handshake(ssl_socket::client, use_awaitable_as_tuple);
    if (ec) {
        stop(ec);
        co_return;
    }

    auto self = shared_from_this();
    system.hand_start(socket_id_);

    // 发送协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_write();
        },
        asio::detached);

    // 接收协程
    co_spawn(
        context,
        [self, this]() {
            std::ignore = self;
            return co_read();
        },
        asio::detached);
}

asio::awaitable<void> ssl_client_impl::co_timeout(const asio_timer::duration& timeout) {
    connect_.expires_after(timeout);
    if (auto [ec] = co_await connect_.async_wait(); !ec) {
        stop(std::make_error_code(std::errc::timed_out));
    }
}

asio::awaitable<void> ssl_client_impl::co_read() {
    const auto& system = socket_system::instance();
    uint8_t data[1024];
    for (;;) {
        auto [ec, len] = co_await socket_.async_read_some(asio::buffer(data), use_awaitable_as_tuple);
        if (ec || len == 0) {
            stop(ec);
            co_return;
        }

        system.hand_read(socket_id_, data, len);
        trace_read(len);
    }
}

asio::awaitable<void> ssl_client_impl::co_write() {
    std::vector<memory_buffer_ptr> cache_write;
    std::vector<asio::const_buffer> buffers;
    const auto max_buffers = socket_system::instance().max_buffers();
    cache_write.reserve(max_buffers);
    buffers.reserve(max_buffers);
    const auto& socket_raw = socket_.next_layer();

    while (socket_raw.is_open()) {
        if (write_deque_.empty()) {
            co_await write_blocker_.async_wait();
            if (!socket_raw.is_open()) co_return;
        }

        const auto size = std::min(write_deque_.size(), max_buffers);
        const auto it_begin = write_deque_.begin();
        const auto it_end = it_begin + static_cast<int64_t>(size);
        for (auto it = it_begin; it != it_end; ++it) {
            memory_buffer_ptr temp = *it;
            buffers.emplace_back(temp->begin_read(), temp->readable());
            cache_write.emplace_back(std::move(temp));
        }

        write_deque_.erase(it_begin, it_end);
        auto [ec, len] = co_await async_write(socket_, buffers, use_awaitable_as_tuple);
        buffers.clear();
        cache_write.clear();
        trace_write_queue(-static_cast<int64_t>(len));
        trace_write(len);
    }
}

}  // namespace simple
