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

#ifndef FOSTER_BTREE_ASSERTIONS_H
#define FOSTER_BTREE_ASSERTIONS_H

/**
 * \file assertions.h
 *
 * Provides a generic assertion mechanism for verifying code invariants and pre/post-conditions.
 * Uses generic programming techniques to allow throwing any kind of exception with arbitrary
 * arguments. Also provides a mechanism for including file and line-number information in messages.
 */

#include <stdexcept>
#include <type_traits>
#include <sstream>
#include <cstring>

using std::string;

// If debug level not defined by compiler arguments, use a default value
#define DEFAULT_DEBUG_LEVEL 1
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEFAULT_DEBUG_LEVEL
#endif

// Disable the standard assert macro
#ifndef NDEBUG
#define NDEBUG
#endif
#ifdef assert
#undef assert
#endif

/** Convenience macro for creating a DbgInfo object */
#define DBGINFO foster::DbgInfo {__FILE__, __LINE__, __func__}

namespace foster {

/**
 * \brief Statically specifies the global debug level when compiling.
 *
 * The debug level is an unsigned integer which specifies which assertions and debug features should
 * be compiled. When running in debug level 2, for example, only assertions using levels 0, 1, and 2
 * will be included in the compiled code.
 */
constexpr unsigned DebugLevel = DEBUG_LEVEL;
constexpr unsigned DefaultDebugLevel = DEFAULT_DEBUG_LEVEL;
constexpr bool IsDebugLevel(unsigned level) { return DebugLevel >= level; }

/**
 * \brief Helper object to keep track of file, line number, and function of the assert call.
 */
struct DbgInfo {
    const char* file;
    const int line;
    const char* function;

    /**
     * \brief Prepends the debug information to an existing message string
     */
    string append_msg(const string& msg)
    {
        // Extract just the file name, without the filesystem path
        const char* f = strrchr(file, '/') ? strrchr(file, '/') + 1 : file;
        std::stringstream ss;
        ss << "At " << f << ':' << line << " [" << function << "]: " << msg;
        return ss.str();
    }
};

/**
 * \brief Generic exception class for failed assertions.
 */
struct AssertionFailure : public std::runtime_error {
    /** Standard cosntructor, using an error message */
    AssertionFailure(const string& msg) : std::runtime_error(msg) {}

    /**
     * This constructor includes debug information into the given message. To be used in
     * combination with the DBGINFO macro. Example:
     *     throw AssertionFailure(DBGINFO, "Error")
     *
     * It is usally not used directly though, but with the generic assert function below.
     */
    AssertionFailure(DbgInfo info, const string& msg)
        : std::runtime_error(info.append_msg(msg))
    {}
};

/**
 * \brief Function object that encapsulates an assertion call.
 *
 * A function object is used instead of a plain function because it allows choosing an empty
 * implementation (effectively disabling the assertion) if a lower debug level is in use.
 *
 * The template class and associated function-call operator use variadic arguments, which allows
 * constructing an arbitrary exception object with an arbitrary list of parameters.
 *
 * @see NoAssert, assert
 */
template <class E = AssertionFailure, typename... Args>
struct Assert
{
    void operator()(bool assertion, Args... args)
    {
        if (assertion) { return; }

        // Assertion does not hold -- handle error accordingly
        // (std::forward is used to properly pass rvalue arguments to the constructor)
        throw E(std::forward<Args>(args)...);
    }
};

/**
 * \brief Empty function object used when debug level is lower than the given assertion.
 */
struct NoAssert
{
    // No implementation -- code deactivated by debug level
    void operator()(bool, ...)
    {
    }
};

/**
 * \brief Generic assertion function with support for debug levels.
 * \tparam L Debug level of the assertion; generates empty code (no-op) if higher than global DebugLevel.
 * \tparam E Exception class to throw if assertion fails.
 * \tparam Args Argument types for the exception constructor. These are not used explicitly, but
 *      rather deduced from the call parameters.
 * \param[in] assertion Condition to verify (i.e., assert). Throws given exception if false.
 * \param[in] args An arbitrary argument list passed down to the exception constructor when the
 *      assertion fails.
 *
 * A special use case occurs in combination with the DBGINFO macro and exception classes that
 * support the DbgInfo struct. Example:
 *
 *     assert(a > b, DBGINFO, "a must be larger than b");
 *
 * In this case, an AssertionFailure exception is thrown and the message is prepended with the name
 * of the source file, the line number, and the function in which the assertion was invoked:
 *
 *     At file.cpp:47 [myFunction]: a must be larger than b
 *
 */
template <unsigned L = DefaultDebugLevel, class E = AssertionFailure, typename... Args>
void assert(bool assertion, Args... args)
{
    using AssertType = typename std::conditional<L <= DebugLevel, Assert<E, Args...>, NoAssert>::type;
    AssertType a;
    a(assertion, args...);
}

} // namespace foster

#endif
