/*
 * MIT License
 *
 * Copyright (c) 2016 Caetano Sauer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FOSTER_BTREE_DEBUG_LOG_H
#define FOSTER_BTREE_DEBUG_LOG_H

/**
 * \file debug_log.h
 *
 * Logging infrastructure for debug messages, warnings, info, errors, etc.
 */

#include "assertions.h"

// #if USE_SPDLOG
#include <spdlog/spdlog.h>
// #endif

namespace foster {
namespace dbg {

/*
 * Global compile-time options
 */
constexpr bool EnableColors = false;

std::shared_ptr<spdlog::logger> get_logger()
{
    static std::shared_ptr<spdlog::logger> LOGGER =
        spdlog::stdout_logger_mt("foster_logger", EnableColors);

    return LOGGER;
}

/**
 * Log function statically defined depending on debug level. If the given level is lower than what
 * we're using, the function does nothing (see below). The same technique was used in the assert
 * function (see assertions.h).
 */
template <unsigned L = DefaultDebugLevel, typename... Args>
typename std::enable_if<(L <= DebugLevel)>::type
    log(const Args&... args)
{
    get_logger()->info(std::forward<const Args&>(args)...);
}

template <unsigned L = DefaultDebugLevel, typename... Args>
typename std::enable_if<!(L <= DebugLevel)>::type
    log(const Args&...)
{
}

} // namespace dbg
} // namespace foster

#endif
