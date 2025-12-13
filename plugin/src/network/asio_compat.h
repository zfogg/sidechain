#pragma once

//==============================================================================
/**
 * ASIO Compatibility Layer
 *
 * Compatibility layer for websocketpp to work with newer standalone ASIO.
 *
 * Problem:
 * - websocketpp expects `io_service` but newer ASIO (1.18+) uses `io_context`
 * - websocketpp hasn't been updated for ASIO 1.18+
 *
 * Solution:
 * - Provides aliases and compatibility wrappers to bridge the gap
 * - Maps `io_service` -> `io_context`
 * - Maps deprecated APIs to modern equivalents
 *
 * @note This is a known compatibility issue in the websocketpp library
 */

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>
#include <asio/strand.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/post.hpp>
#include <asio/ip/tcp.hpp>

namespace asio {
    // Alias io_service to io_context for backward compatibility
    using io_service = io_context;

    // Alias io_service::strand to strand<io_context::executor_type>
    using io_service_strand = strand<io_context::executor_type>;

    // Compatibility: io_context::work (deprecated, use executor_work_guard)
    namespace io_context_work {
        using work = executor_work_guard<io_context::executor_type>;
    }

    // Add post() method to io_context for compatibility
    template<typename CompletionHandler>
    void post(io_context& ctx, CompletionHandler&& handler) {
        asio::post(ctx.get_executor(), std::forward<CompletionHandler>(handler));
    }

    // Note: resolver::query and iterator were removed in newer ASIO
    // websocketpp should use resolver::results directly instead
    // If compatibility is needed, it would require a custom resolver wrapper
}

// For websocketpp's namespace usage
namespace websocketpp {
namespace lib {
namespace asio {
    using io_service = ::asio::io_context;
    using io_service_strand = ::asio::strand<::asio::io_context::executor_type>;

    // Compatibility wrapper for io_context::work
    namespace io_context_work {
        using work = ::asio::executor_work_guard<::asio::io_context::executor_type>;
    }
}
}
}

// Compatibility for steady_timer::expires_from_now (replaced with expires_after)
// Note: This requires patching websocketpp source or using a wrapper
// For now, we'll need to patch the websocketpp files directly
