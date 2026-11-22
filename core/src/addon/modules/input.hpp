/*
 * Copyright © Oriel contributors
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

#ifndef WINDOWER_ADDON_MODULES_INPUT_HPP
#define WINDOWER_ADDON_MODULES_INPUT_HPP

#include "addon/lua.hpp"

#include <windows.h>

#include <optional>

namespace windower
{

// Oriel: core.input, raw mouse and keyboard events for add-ons, delivered
// synchronously so a handler can keep the event from the game (like
// Windower 4's 'mouse' and 'keyboard' events).
int load_input_module(lua::state);

// Offers a window message to the add-ons' input handlers. Returns a result
// when a handler blocked it.
std::optional<::LRESULT> process_input_message(::MSG const&) noexcept;

}

#endif
