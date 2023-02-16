#include <simple/coro/network.h>
#include <simple/web/http.h>

#include <cctype>
#include <charconv>
#include <stdexcept>

namespace simple::http {

namespace detail {
using namespace std::string_view_literals;
constexpr auto ok = "HTTP/1.1 200 OK\r\n"sv;
constexpr auto switching_protocols = "HTTP/1.1 101 Switching Protocols\r\n"sv;
constexpr auto created = "HTTP/1.1 201 Created\r\n"sv;
constexpr auto accepted = "HTTP/1.1 202 Accepted\r\n"sv;
constexpr auto no_content = "HTTP/1.1 204 No Content\r\n"sv;
constexpr auto multiple_choices = "HTTP/1.1 300 Multiple Choices\r\n"sv;
constexpr auto moved_permanently = "HTTP/1.1 301 Moved Permanently\r\n"sv;
constexpr auto moved_temporarily = "HTTP/1.1 302 Moved Temporarily\r\n"sv;
constexpr auto not_modified = "HTTP/1.1 304 Not Modified\r\n"sv;
constexpr auto bad_request = "HTTP/1.1 400 Bad Request\r\n"sv;
constexpr auto unauthorized = "HTTP/1.1 401 Unauthorized\r\n"sv;
constexpr auto forbidden = "HTTP/1.1 403 Forbidden\r\n"sv;
constexpr auto not_found = "HTTP/1.1 404 Not Found\r\n"sv;
constexpr auto internal_server_error = "HTTP/1.1 500 Internal Server Error\r\n"sv;
constexpr auto not_implemented = "HTTP/1.1 501 Not Implemented\r\n"sv;
constexpr auto bad_gateway = "HTTP/1.1 502 Bad Gateway\r\n"sv;
constexpr auto service_unavailable = "HTTP/1.1 503 Service Unavailable\r\n"sv;
}  // namespace detail

constexpr std::string_view to_string_view(reply::status_t status) {
    switch (status) {
        case reply::status_t::switching_protocols:
            return detail::switching_protocols;
        case reply::status_t::ok:
            return detail::ok;
        case reply::status_t::created:
            return detail::created;
        case reply::status_t::accepted:
            return detail::accepted;
        case reply::status_t::no_content:
            return detail::no_content;
        case reply::status_t::multiple_choices:
            return detail::multiple_choices;
        case reply::status_t::moved_permanently:
            return detail::moved_permanently;
        case reply::status_t::moved_temporarily:
            return detail::moved_temporarily;
        case reply::status_t::not_modified:
            return detail::not_modified;
        case reply::status_t::bad_request:
            return detail::bad_request;
        case reply::status_t::unauthorized:
            return detail::unauthorized;
        case reply::status_t::forbidden:
            return detail::forbidden;
        case reply::status_t::not_found:
            return detail::not_found;
        case reply::status_t::internal_server_error:
            return detail::internal_server_error;
        case reply::status_t::not_implemented:
            return detail::not_implemented;
        case reply::status_t::bad_gateway:
            return detail::bad_gateway;
        case reply::status_t::service_unavailable:
            return detail::service_unavailable;
        default:  // NOLINT(clang-diagnostic-covered-switch-default)
            return detail::internal_server_error;
    }
}

namespace misc_strings {
using namespace std::string_view_literals;
constexpr auto name_value_separator = ": "sv;
constexpr auto crlf = "\r\n"sv;
}  // namespace misc_strings

memory_buffer_ptr reply::to_buffer() const {
    auto status_strv = to_string_view(status);
    auto ptr = std::make_shared<memory_buffer>(status_strv.data(), status_strv.size());
    for (auto& h : headers) {
        ptr->append(h.name.c_str(), h.name.size());
        ptr->append(misc_strings::name_value_separator.data(), misc_strings::name_value_separator.size());
        ptr->append(h.value.c_str(), h.value.size());
        ptr->append(misc_strings::crlf.data(), misc_strings::crlf.size());
    }

    ptr->append(misc_strings::crlf.data(), misc_strings::crlf.size());
    ptr->append(content.c_str(), content.size());
    return ptr;
}

namespace stock_replies {
using namespace std::string_view_literals;
constexpr auto switching_protocols = ""sv;
constexpr auto ok = ""sv;
constexpr auto created =
    "<html>"
    "<head><title>Created</title></head>"
    "<body><h1>201 Created</h1></body>"
    "</html>"sv;
constexpr auto accepted =
    "<html>"
    "<head><title>Accepted</title></head>"
    "<body><h1>202 Accepted</h1></body>"
    "</html>"sv;
constexpr auto no_content =
    "<html>"
    "<head><title>No Content</title></head>"
    "<body><h1>204 Content</h1></body>"
    "</html>"sv;
constexpr auto multiple_choices =
    "<html>"
    "<head><title>Multiple Choices</title></head>"
    "<body><h1>300 Multiple Choices</h1></body>"
    "</html>"sv;
constexpr auto moved_permanently =
    "<html>"
    "<head><title>Moved Permanently</title></head>"
    "<body><h1>301 Moved Permanently</h1></body>"
    "</html>"sv;
constexpr auto moved_temporarily =
    "<html>"
    "<head><title>Moved Temporarily</title></head>"
    "<body><h1>302 Moved Temporarily</h1></body>"
    "</html>"sv;
constexpr auto not_modified =
    "<html>"
    "<head><title>Not Modified</title></head>"
    "<body><h1>304 Not Modified</h1></body>"
    "</html>"sv;
constexpr auto bad_request =
    "<html>"
    "<head><title>Bad Request</title></head>"
    "<body><h1>400 Bad Request</h1></body>"
    "</html>"sv;
constexpr auto unauthorized =
    "<html>"
    "<head><title>Unauthorized</title></head>"
    "<body><h1>401 Unauthorized</h1></body>"
    "</html>"sv;
constexpr auto forbidden =
    "<html>"
    "<head><title>Forbidden</title></head>"
    "<body><h1>403 Forbidden</h1></body>"
    "</html>"sv;
constexpr auto not_found =
    "<html>"
    "<head><title>Not Found</title></head>"
    "<body><h1>404 Not Found</h1></body>"
    "</html>"sv;
constexpr auto internal_server_error =
    "<html>"
    "<head><title>Internal Server Error</title></head>"
    "<body><h1>500 Internal Server Error</h1></body>"
    "</html>"sv;
constexpr auto not_implemented =
    "<html>"
    "<head><title>Not Implemented</title></head>"
    "<body><h1>501 Not Implemented</h1></body>"
    "</html>"sv;
constexpr auto bad_gateway =
    "<html>"
    "<head><title>Bad Gateway</title></head>"
    "<body><h1>502 Bad Gateway</h1></body>"
    "</html>"sv;
constexpr auto service_unavailable =
    "<html>"
    "<head><title>Service Unavailable</title></head>"
    "<body><h1>503 Service Unavailable</h1></body>"
    "</html>"sv;

std::string_view to_replies(reply::status_t status) {
    switch (status) {
        case reply::status_t::switching_protocols:
            return switching_protocols;
        case reply::status_t::ok:
            return ok;
        case reply::status_t::created:
            return created;
        case reply::status_t::accepted:
            return accepted;
        case reply::status_t::no_content:
            return no_content;
        case reply::status_t::multiple_choices:
            return multiple_choices;
        case reply::status_t::moved_permanently:
            return moved_permanently;
        case reply::status_t::moved_temporarily:
            return moved_temporarily;
        case reply::status_t::not_modified:
            return not_modified;
        case reply::status_t::bad_request:
            return bad_request;
        case reply::status_t::unauthorized:
            return unauthorized;
        case reply::status_t::forbidden:
            return forbidden;
        case reply::status_t::not_found:
            return not_found;
        case reply::status_t::internal_server_error:
            return internal_server_error;
        case reply::status_t::not_implemented:
            return not_implemented;
        case reply::status_t::bad_gateway:
            return bad_gateway;
        case reply::status_t::service_unavailable:
            return service_unavailable;
        default:
            return internal_server_error;
    }
}
}  // namespace stock_replies

reply reply::stock(reply::status_t status) {
    reply rep;
    rep.status = status;
    rep.content = stock_replies::to_replies(status);
    rep.headers.resize(2);
    rep.headers[0].name = "Content-Length";
    rep.headers[0].value = std::to_string(rep.content.size());
    rep.headers[1].name = "Content-Type";
    rep.headers[1].value = "text/html";
    return rep;
}

/// Check if a byte is an HTTP character.
static constexpr bool is_char(int c) { return c >= 0 && c <= 127; }

/// Check if a byte is an HTTP control character.
static constexpr bool is_ctl(int c) { return (c >= 0 && c <= 31) || (c == 127); }

/// Check if a byte is defined as an HTTP special character.
static constexpr bool is_special(int c) {
    switch (c) {
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ',':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '[':
        case ']':
        case '?':
        case '=':
        case '{':
        case '}':
        case ' ':
        case '\t':
            return true;
        default:
            return false;
    }
}

static simple::task<std::string_view> parser_line(uint32_t socket, memory_buffer& buf) {
    buf.clear();
    if (co_await simple::network::instance().read_until(socket, misc_strings::crlf, buf) == 0) {
        throw std::logic_error("recv eof");
    }

    auto line = std::string_view(buf);
    co_return line.substr(0, line.size() - misc_strings::crlf.size());
}

enum class line_state {
    method_start,
    method,
    uri,
    version_h,
    version_t_1,
    version_t_2,
    version_p,
    version_slash,
    version_major_start,
    version_major,
    version_minor_start,
    version_minor,
    status_start,
    status,
    reason
};

static bool parser_request_line(request& req, std::string_view line) {
    line_state state{line_state::method_start};
    for (auto ch : line) {
        switch (state) {
            case line_state::method_start:
                if (!is_char(ch) || is_ctl(ch) || is_special(ch)) {
                    return false;
                }

                state = line_state::method;
                req.method.push_back(ch);
                break;

            case line_state::method:
                if (ch == ' ') {
                    state = line_state::uri;
                    break;
                }

                if (!is_char(ch) || is_ctl(ch) || is_special(ch)) {
                    return false;
                }

                req.method.push_back(ch);
                break;

            case line_state::uri:
                if (ch == ' ') {
                    state = line_state::version_h;
                    break;
                }

                if (is_ctl(ch)) {
                    return false;
                }

                req.uri.push_back(ch);
                break;

            case line_state::version_h:
                if (ch != 'H') {
                    return false;
                }

                state = line_state::version_t_1;
                break;

            case line_state::version_t_1:
                if (ch != 'T') {
                    return false;
                }

                state = line_state::version_t_2;
                break;

            case line_state::version_t_2:
                if (ch != 'T') {
                    return false;
                }

                state = line_state::version_p;
                break;

            case line_state::version_p:
                if (ch != 'P') {
                    return false;
                }

                state = line_state::version_slash;
                break;

            case line_state::version_slash:
                if (ch != '/') {
                    return false;
                }

                state = line_state::version_major_start;
                break;

            case line_state::version_major_start:
                if (!std::isdigit(ch)) {
                    return false;
                }

                req.version_major = ch - '0';
                state = line_state::version_major;
                break;

            case line_state::version_major:
                if (ch == '.') {
                    state = line_state::version_minor_start;
                    break;
                }

                if (!std::isdigit(ch)) {
                    return false;
                }

                req.version_major = req.version_major * 10 + ch - '0';
                break;

            case line_state::version_minor_start:
                if (!std::isdigit(ch)) {
                    return false;
                }

                req.version_minor = ch - '0';
                state = line_state::version_minor;
                break;

            case line_state::version_minor:
                if (!std::isdigit(ch)) {
                    return false;
                }

                req.version_minor = req.version_minor * 10 + ch - '0';
                break;

            default:
                break;
        }
    }

    return state == line_state::version_minor;
}

static simple::task<> parser_request_line(request& req, uint32_t socket) {
    memory_buffer buf;
    auto line = co_await parser_line(socket, buf);
    if (!parser_request_line(req, line)) {
        throw std::logic_error("parser request line fail");
    }
}

static bool parser_reply_line(reply& rep, std::string_view line) {
    line_state state{line_state::version_h};
    for (auto ch : line) {
        switch (state) {
            case line_state::version_h:
                if (ch != 'H') {
                    return false;
                }

                state = line_state::version_t_1;
                break;

            case line_state::version_t_1:
                if (ch != 'T') {
                    return false;
                }

                state = line_state::version_t_2;
                break;

            case line_state::version_t_2:
                if (ch != 'T') {
                    return false;
                }

                state = line_state::version_p;
                break;

            case line_state::version_p:
                if (ch != 'P') {
                    return false;
                }

                state = line_state::version_slash;
                break;

            case line_state::version_slash:
                if (ch != '/') {
                    return false;
                }

                state = line_state::version_major_start;
                break;

            case line_state::version_major_start:
                if (!std::isdigit(ch)) {
                    return false;
                }

                state = line_state::version_major;
                break;

            case line_state::version_major:
                if (ch == '.') {
                    state = line_state::version_minor_start;
                    break;
                }

                if (!std::isdigit(ch)) {
                    return false;
                }

                break;

            case line_state::version_minor_start:
                if (!std::isdigit(ch)) {
                    return false;
                }

                state = line_state::version_minor;
                break;

            case line_state::version_minor:
                if (ch == ' ') {
                    state = line_state::status_start;
                    break;
                }

                if (!std::isdigit(ch)) {
                    return false;
                }

                break;

            case line_state::status_start:
                if (!std::isdigit(ch)) {
                    return false;
                }

                rep.status = static_cast<reply::status_t>(ch - '0');
                state = line_state::status;
                break;

            case line_state::status:
                if (ch == ' ') {
                    state = line_state::reason;
                    break;
                }

                if (!std::isdigit(ch)) {
                    return false;
                }

                rep.status = static_cast<reply::status_t>(static_cast<int32_t>(rep.status) * 10 + ch - '0');
                break;

            default:
                break;
        }
    }

    return state == line_state::reason;
}

static simple::task<> parser_reply_line(reply& rep, uint32_t socket) {
    memory_buffer buf;
    auto line = co_await parser_line(socket, buf);
    if (!parser_reply_line(rep, line)) {
        throw std::logic_error("parser reply line fail");
    }
}

enum class header_state { start, lws, name, space_before_value, value };

static bool parser_header(std::vector<header>& headers, std::string_view line) {
    header_state state{header_state::start};
    header* current;
    for (auto ch : line) {
        switch (state) {
            case header_state::start:
                if (!headers.empty() && (ch == ' ' || ch == '\t')) {
                    state = header_state::lws;
                    break;
                }

                if (!is_char(ch) || is_ctl(ch) || is_special(ch)) {
                    return false;
                }

                current = &headers.emplace_back();
                current->name.push_back(ch);
                state = header_state::name;
                break;

            case header_state::lws:
                if (ch == ' ' || ch == '\t') {
                    break;
                }

                if (is_ctl(ch)) {
                    return false;
                }

                state = header_state::value;
                current = &headers.back();
                current->value.push_back(ch);
                break;

            case header_state::name:
                if (ch == ':') {
                    state = header_state::space_before_value;
                    break;
                }

                if (!is_char(ch) || is_ctl(ch) || is_special(ch)) {
                    return false;
                }

                current->name.push_back(ch);
                break;

            case header_state::space_before_value:
                if (ch != ' ') {
                    return false;
                }

                state = header_state::value;
                break;

            case header_state::value:
                if (is_ctl(ch)) {
                    return false;
                }
                current->value.push_back(ch);
                break;

            default:
                break;
        }
    }

    return state == header_state::value;
}

static simple::task<> parser_header_body(std::vector<header>& headers, std::string& body, uint32_t socket) {
    auto& network = simple::network::instance();
    memory_buffer buf;
    size_t body_size = 0;
    for (;;) {
        auto line = co_await parser_line(socket, buf);
        if (line.empty()) {
            // headers 接收完了
            break;
        }

        if (!parser_header(headers, line)) {
            throw std::logic_error("parser header fail");
        }

        if (const auto& header = headers.back(); header.name == "Content-Length") {
            std::from_chars(header.value.data(), header.value.data() + header.value.size(), body_size, 10);
        }
    }

    if (body_size > 0) {
        body.resize(body_size);
        if (co_await network.read_size(socket, body.data(), body_size) == 0) {
            throw std::logic_error("recv eof");
        }
    }
}

simple::task<> parser(request& req, uint32_t socket) {
    // 先读请求行
    co_await parser_request_line(req, socket);
    // 接着读 请求头 和 请求体
    co_await parser_header_body(req.headers, req.content, socket);
}

simple::task<> parser(reply& rep, uint32_t socket) {
    // 先读回应行
    co_await parser_reply_line(rep, socket);
    // 接着读 回应头 和 回应体
    co_await parser_header_body(rep.headers, rep.content, socket);
}

}  // namespace simple::http
