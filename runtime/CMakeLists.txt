cmake_minimum_required(VERSION 3.5)

# Set the project name
project(libruntime)

if (${CMAKE_SIZEOF_VOID_P} MATCHES "4")
    message(FATAL_ERROR "only support x64 toolchain")
endif ()

set(CMAKE_CXX_STANDARD 20)

file(GLOB
        SIMPLE_HEADS
        "include/simple/config.h"
        "include/simple/error.h"

        # containers
        "include/simple/containers/buffer.hpp"
        "include/simple/containers/heap.hpp"
        "include/simple/containers/pool.hpp"
        "include/simple/containers/queue.h"
        "include/simple/containers/time_queue.h"

        # log
        "include/simple/log/appender.h"
        "include/simple/log/console_appender.h"
        "include/simple/log/default_flags.hpp"
        "include/simple/log/file_appender.h"
        "include/simple/log/flag.h"
        "include/simple/log/formatter.h"
        "include/simple/log/log.h"
        "include/simple/log/logger.h"
        "include/simple/log/logmsg.h"
        "include/simple/log/msvc_appender.h"
        "include/simple/log/log_system.h"
        "include/simple/log/types.h"

        # utils
        "include/simple/utils/fixed_string.hpp"
        "include/simple/utils/func_traits.hpp"
        "include/simple/utils/os.h"
        "include/simple/utils/time.h"
        "include/simple/utils/type_traits.hpp"
        "include/simple/utils/coro_traits.hpp"
        "include/simple/utils/toml_types.hpp"
        "include/simple/utils/multiple_exceptions.hpp"
        "include/simple/utils/crypt.h"

        # net
        "include/simple/net/socket_types.h"
        "include/simple/net/socket_system.h"
        src/net/impl/socket_impl.hpp
        src/net/impl/tcp_server_impl.h
        src/net/impl/tcp_session_impl.h
        src/net/impl/tcp_client_impl.h
        src/net/impl/ssl_client_impl.h
        src/net/impl/ssl_session_impl.h
        src/net/impl/ssl_server_impl.h
        src/net/impl/kcp_client_impl.h
        src/net/impl/kcp_session_impl.h
        src/net/impl/kcp_server_impl.h

        # coro
        "include/simple/coro/thread_pool.h"
        "include/simple/coro/scheduler.h"
        "include/simple/coro/timed_awaiter.h"
        src/coro/cancellation_state.h
        "include/simple/coro/cancellation_token.h"
        "include/simple/coro/cancellation_source.h"
        "include/simple/coro/cancellation_registration.h"
        "include/simple/coro/task.hpp"
        "include/simple/coro/detached_task.hpp"
        "include/simple/coro/co_start.hpp"
        "include/simple/coro/sync_wait.hpp"
        "include/simple/coro/task_operators.hpp"
        "include/simple/coro/parallel_task.hpp"
        "include/simple/coro/mutex.hpp"
        "include/simple/coro/async_session.h"
        "include/simple/coro/network.h"
        "include/simple/coro/current_coroutine.hpp"
        "include/simple/coro/condition_variable.h"

        # app
        "include/simple/application/service.hpp"
        "src/application/service_loader.h"
        "include/simple/application/application.h"
        "include/simple/application/frame_awaiter.h"

        # shm
        "include/simple/shm/shm.h"
        "include/simple/shm/shm_buffer.h"
        "include/simple/shm/shm_channel.h"
        "include/simple/shm/shm_channel_select.h"

        # web
        "include/simple/web/http.h"
        "include/simple/web/websocket.h"
        )

file(GLOB
        SIMPLE_SOURCES
        src/error.cpp

        # containers
        src/containers/queue.cpp
        src/containers/time_queue.cpp

        # log
        "src/log/appender.cpp"
        "src/log/console_appender.cpp"
        "src/log/file_appender.cpp"
        "src/log/formatter.cpp"
        "src/log/log.cpp"
        "src/log/logger.cpp"
        "src/log/msvc_appender.cpp"
        "src/log/log_system.cpp"
        "src/log/types.cpp"

        # utils
        src/utils/os.cpp
        src/utils/time.cpp
        src/utils/crypt.cpp

        # net
        "src/net/socket_system.cpp"
        src/net/impl/tcp_server_impl.cpp
        src/net/impl/tcp_session_impl.cpp
        src/net/impl/tcp_client_impl.cpp
        src/net/impl/ssl_client_impl.cpp
        src/net/impl/ssl_session_impl.cpp
        src/net/impl/ssl_server_impl.cpp
        src/net/impl/kcp_client_impl.cpp
        src/net/impl/kcp_session_impl.cpp
        src/net/impl/kcp_server_impl.cpp

        #coro
        src/coro/thread_pool.cpp
        src/coro/scheduler.cpp
        src/coro/timed_awaiter.cpp
        src/coro/cancellation_state.cpp
        src/coro/cancellation_token.cpp
        src/coro/cancellation_source.cpp
        src/coro/cancellation_registration.cpp
        src/coro/mutex.cpp
        src/coro/async_session.cpp
        src/coro/network.cpp
        src/coro/condition_variable.cpp

        #app
        src/application/service_loader.cpp
        src/application/application.cpp
        src/application/frame_awaiter.cpp

        #shm
        src/shm/shm.cpp
        src/shm/shm_buffer.cpp
        src/shm/shm_channel.cpp
        src/shm/shm_channel_select.cpp

        # web
        src/web/http.cpp
        src/web/websocket.cpp
        )

if (WIN32)
    add_library(${PROJECT_NAME} SHARED
            ${SIMPLE_HEADS}
            ${SIMPLE_SOURCES})
    target_compile_definitions(${PROJECT_NAME}
            PUBLIC
            WIN32_LEAN_AND_MEAN
            _CRT_SECURE_NO_WARNINGS
            _WINSOCK_DEPRECATED_NO_WARNINGS
            _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
            _WIN32_WINNT=0x0A00
            NOMINMAX)
    source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SIMPLE_HEADS} ${SIMPLE_SOURCES})
    target_compile_definitions(${PROJECT_NAME} INTERFACE SIMPLE_DLL_IMPORT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SIMPLE_DLL_EXPORT ASIO_NO_DEPRECATED)
else ()
    add_library(${PROJECT_NAME} SHARED ${SIMPLE_SOURCES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_NO_DEPRECATED)
    target_compile_definitions(${PROJECT_NAME} PUBLIC SIMPLE_LIB_VISIBILITY)
endif ()

target_compile_options(${PROJECT_NAME}
        PUBLIC
        "$<$<C_COMPILER_ID:MSVC>:/MP>"
        "$<$<CXX_COMPILER_ID:MSVC>:/MP>")


target_include_directories(${PROJECT_NAME}
        PUBLIC ${PROJECT_SOURCE_DIR}/include
        )

find_package(lz4 CONFIG REQUIRED)
if (NOT lz4_FOUND)
    message(WARNING "lz4 not found.")
endif ()

find_package(fmt CONFIG REQUIRED)
if (NOT fmt_FOUND)
    message(WARNING "fmt not found.")
endif ()

find_package(asio CONFIG REQUIRED)
if (NOT asio_FOUND)
    message(WARNING "asio not found.")
endif ()

find_package(OpenSSL REQUIRED)
if (NOT OpenSSL_FOUND)
    message(WARNING "OpenSSL not found.")
endif ()

find_package(kcp CONFIG REQUIRED)
if (NOT kcp_FOUND)
    message(WARNING "kcp not found.")
endif ()

find_package(toml11 CONFIG REQUIRED)
if (NOT toml11_FOUND)
    message(WARNING "toml11 not found.")
endif ()


if (WIN32)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            lz4::lz4
            asio::asio
            kcp::kcp
            OpenSSL::SSL
            OpenSSL::Crypto
            ws2_32.lib
            mswsock.lib
            crypt32.lib

            PUBLIC
            fmt::fmt-header-only
            toml11::toml11
            )
else ()
    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            lz4::lz4
            asio::asio
            kcp::kcp
            OpenSSL::SSL
            OpenSSL::Crypto

            PUBLIC
            fmt::fmt-header-only
            toml11::toml11
            )
endif ()



