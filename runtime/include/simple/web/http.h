#pragma once
#include <simple/config.h>

#include <cstdint>
#include <simple/containers/buffer.hpp>
#include <simple/coro/task.hpp>
#include <string>
#include <vector>

namespace simple::http {

struct header {
    std::string name;
    std::string value;

    bool operator<(const header& other) const { return name < other.name; }
};

struct request {
    std::string method;
    std::string uri;
    int32_t version_major{0};
    int32_t version_minor{0};
    std::vector<header> headers;
    std::string content;

    void reset() {
        method.clear();
        uri.clear();
        version_major = 0;
        version_minor = 0;
        headers.clear();
        content.clear();
    }
};

struct reply {
    enum class status_t {
        switching_protocols = 101,
        ok = 200,
        created = 201,
        accepted = 202,
        no_content = 204,
        multiple_choices = 300,
        moved_permanently = 301,
        moved_temporarily = 302,
        not_modified = 304,
        bad_request = 400,
        unauthorized = 401,
        forbidden = 403,
        not_found = 404,
        internal_server_error = 500,
        not_implemented = 501,
        bad_gateway = 502,
        service_unavailable = 503
    } status{status_t::ok};

    std::vector<header> headers;
    std::string content;

    void reset() {
        status = status_t::ok;
        headers.clear();
        content.clear();
    }

    [[nodiscard]] DS_API memory_buffer_ptr to_buffer() const;

    DS_API static reply stock(status_t status);
};

DS_API simple::task<> parser(request& req, uint32_t socket);

DS_API simple::task<> parser(reply& req, uint32_t socket);

}  // namespace simple::http
