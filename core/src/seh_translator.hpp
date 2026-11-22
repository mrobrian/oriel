/*
 * Copyright © Windower Dev Team
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"),to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WINDOWER_SEH_TRANSLATOR_HPP
#define WINDOWER_SEH_TRANSLATOR_HPP

#include <stdexcept>
#include <string>

namespace windower
{

// Oriel: the core is built with asynchronous exceptions (/EHa), so its
// catch (...) blocks also catch hardware faults (access violations) raised
// while running add-ons. Without a translator, such a fault can't be
// described, and re-throwing it to report it crashed the game. With it, the
// fault becomes this exception, whose message names the fault, the module
// and offset it happened at, and the address involved.
class seh_error : public std::runtime_error
{
public:
    explicit seh_error(std::string const& message) : std::runtime_error{message}
    {}
};

// Installs the translator on the calling thread (once per thread).
void install_seh_translator() noexcept;

// The Lua state running on this thread, if any (a lua_State*). When a fault
// happens while it runs, the message also lists its Lua call stack as it was
// at the fault (source:line per frame), since unwinding loses it.
class running_lua_state
{
public:
    explicit running_lua_state(void* state) noexcept;
    ~running_lua_state();
    running_lua_state(running_lua_state const&) = delete;
    running_lua_state& operator=(running_lua_state const&) = delete;
    running_lua_state(running_lua_state&&) = delete;
    running_lua_state& operator=(running_lua_state&&) = delete;

private:
    void* m_previous;
};

}

#endif
