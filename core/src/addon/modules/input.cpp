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

#include "addon/modules/input.hpp"

#include "addon/addon_manager.hpp"
#include "addon/lua.hpp"
#include "addon/modules/event.hpp"
#include "addon/modules/input.lua.hpp"
#include "core.hpp"

#include <windowsx.h>

#include <gsl/gsl>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string_view>

namespace
{

std::byte dispatch_key;

// Mouse event kinds, numbered as in Windower 4.
enum mouse_kind : std::int32_t
{
    mouse_move         = 0,
    left_down          = 1,
    left_up            = 2,
    left_double_click  = 3,
    right_down         = 4,
    right_up           = 5,
    right_double_click = 6,
    middle_down        = 7,
    middle_up          = 8,
    middle_double_click = 9,
    wheel              = 10,
};

// Calls every interpreter's dispatcher. Each sees whether an earlier one
// blocked the event; the result is true if any did.
template<typename Push>
bool dispatch(std::u8string_view kind, int argc, Push&& push) noexcept
{
    using namespace windower;

    auto blocked = false;
    try
    {
        auto& core = core::instance();
        if (core.addon_manager)
        {
            core.addon_manager->hold_changes();
        }
        auto const release = gsl::finally([&core]() noexcept {
            if (core.addon_manager)
            {
                try
                {
                    core.addon_manager->release_changes();
                }
                catch (...)
                {
                }
            }
        });

        run_on_all_interpreters([&](lua::state s) {
            try
            {
                lua::stack_guard guard{s};
                lua::push(guard, &::dispatch_key);
                lua::raw_get(guard, lua::registry);
                if (lua::typeof(guard, -1) != lua::type::function)
                {
                    return;
                }
                lua::push(guard, kind);
                push(guard);
                lua::push(guard, blocked);
                lua::call(guard, argc + 2, 1);
                blocked |= lua::get<bool>(guard, -1);
            }
            catch (...)
            {
                core::error(u8"input", std::current_exception());
            }
        });
    }
    catch (...)
    {
    }
    return blocked;
}

// Client pixels to UI pixels (the coordinates add-ons draw in).
std::pair<double, double> to_ui(::HWND hwnd, ::LPARAM lparam) noexcept
{
    auto x = gsl::narrow_cast<double>(GET_X_LPARAM(lparam));
    auto y = gsl::narrow_cast<double>(GET_Y_LPARAM(lparam));
    ::RECT client{};
    auto const& ui_size = windower::core::instance().settings.ui_size;
    if (::GetClientRect(hwnd, &client) && client.right > 0 && client.bottom > 0 &&
        ui_size.width > 0 && ui_size.height > 0)
    {
        x = x * ui_size.width / client.right;
        y = y * ui_size.height / client.bottom;
    }
    return {x, y};
}

}

std::optional<::LRESULT>
windower::process_input_message(::MSG const& msg) noexcept
{
    auto mouse = [&](std::int32_t type, std::int32_t delta,
                     ::LPARAM position) -> std::optional<::LRESULT> {
        auto const [x, y] = to_ui(msg.hwnd, position);
        auto const blocked = dispatch(u8"mouse", 4, [&](auto const& guard) {
            lua::push(guard, type);
            lua::push(guard, x);
            lua::push(guard, y);
            lua::push(guard, delta);
        });
        if (blocked)
        {
            return 0;
        }
        return std::nullopt;
    };

    switch (msg.message)
    {
    default: return std::nullopt;

    case WM_MOUSEMOVE: return mouse(mouse_move, 0, msg.lParam);
    case WM_LBUTTONDOWN: return mouse(left_down, 0, msg.lParam);
    case WM_LBUTTONUP: return mouse(left_up, 0, msg.lParam);
    case WM_LBUTTONDBLCLK: return mouse(left_double_click, 0, msg.lParam);
    case WM_RBUTTONDOWN: return mouse(right_down, 0, msg.lParam);
    case WM_RBUTTONUP: return mouse(right_up, 0, msg.lParam);
    case WM_RBUTTONDBLCLK: return mouse(right_double_click, 0, msg.lParam);
    case WM_MBUTTONDOWN: return mouse(middle_down, 0, msg.lParam);
    case WM_MBUTTONUP: return mouse(middle_up, 0, msg.lParam);
    case WM_MBUTTONDBLCLK: return mouse(middle_double_click, 0, msg.lParam);

    case WM_MOUSEWHEEL:
    {
        // Wheel messages carry screen coordinates.
        ::POINT point{GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam)};
        ::ScreenToClient(msg.hwnd, &point);
        auto const delta = GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA;
        return mouse(wheel, delta, MAKELPARAM(point.x, point.y));
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        // DirectInput key code: scan code, plus 0x80 for extended keys.
        auto const dik = gsl::narrow_cast<std::int32_t>(
            (msg.lParam >> 16 & 0x7F) | (msg.lParam >> 17 & 0x80));
        auto const pressed =
            msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN;
        auto const flags = gsl::narrow_cast<std::int32_t>(msg.lParam >> 30 & 1); // repeat
        auto const blocked = dispatch(u8"keyboard", 3, [&](auto const& guard) {
            lua::push(guard, dik);
            lua::push(guard, pressed);
            lua::push(guard, flags);
        });
        if (blocked)
        {
            return 0;
        }
        return std::nullopt;
    }
    }
}

int windower::load_input_module(lua::state s)
{
    lua::stack_guard guard{s};

    lua::load(guard, lua_input_source, u8"core.input");
    lua::copy(guard, lua::registry);
    lua::push(guard, &::dispatch_key);
    lua::call(guard, 2);

    return guard.release();
}
