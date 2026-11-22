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

#include "wrappers/direct_input_keyboard.hpp"

#include "wrappers/direct_input_device.hpp"
#include "core.hpp"

#include <dinput.h>

#include <algorithm>
#include <vector>

windower::direct_input_keyboard::direct_input_keyboard(
    ::IDirectInputDevice8A* impl) noexcept :
    direct_input_device{impl}
{}

::HRESULT STDMETHODCALLTYPE windower::direct_input_keyboard::GetDeviceState(
    ::DWORD cbData, void* lpvData) noexcept
{
    if (!lpvData)
    {
        return E_POINTER;
    }

    auto keys = static_cast<::BYTE*>(lpvData);
    auto& bindings = core::instance().binding_manager;

    // Oriel: behave like a real foreground DirectInput keyboard. While the
    // game isn't the active window, real DirectInput reports the input as
    // lost, and the game resets its keyboard handling (closing the Alt macro
    // bar, for example). Without this the game kept seeing the Alt from
    // Alt+Tab held while in the background, and the macro bar stayed open
    // after returning. Focus is checked here, every poll, because the game
    // window's focus messages don't reach Oriel.
    static auto was_active = true;
    auto foreground_pid    = ::DWORD{0};
    ::GetWindowThreadProcessId(::GetForegroundWindow(), &foreground_pid);
    auto const active = foreground_pid == ::GetCurrentProcessId();
    if (active != was_active)
    {
        was_active = active;
        try
        {
            // Clears Oriel's held keys on both transitions: releases made in
            // another window never reach us.
            bindings.lost_focus();
        }
        catch (...)
        {
        }
    }
    if (!active)
    {
        std::fill_n(keys, cbData, ::BYTE{0});
        return DIERR_INPUTLOST;
    }

    auto& state = bindings.client_state();
    std::copy_n(
        state.begin(), std::min(state.size(), std::size_t(cbData)), keys);

    return DI_OK;
}

::HRESULT STDMETHODCALLTYPE windower::direct_input_keyboard::GetDeviceData(
    ::DWORD, ::DIDEVICEOBJECTDATA* rgdod, ::DWORD* pdwInOut, ::DWORD) noexcept
{
    if (!rgdod || !pdwInOut)
    {
        return E_POINTER;
    }

    *pdwInOut = INFINITE;
    direct_input_device::GetDeviceData(
        sizeof(::DIDEVICEOBJECTDATA), nullptr, pdwInOut, 0);
    *pdwInOut = 0;
    return DI_OK;
}
