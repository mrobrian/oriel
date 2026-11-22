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

#include "seh_translator.hpp"

#include "error_log.hpp"

#include <windows.h>

#include <eh.h>

#include <gsl/gsl>
#include <lua.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace
{

char const* describe(unsigned int code) noexcept
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    default: break;
    }
    // LuaJIT raises its errors as exceptions with codes 0xE24C4A0x.
    if ((code & 0xFFFFFFF0u) == 0xE24C4A00u)
    {
        return "Lua error outside a protected call";
    }
    return "structured exception";
}

// "module.dll+0x1234" for an address.
std::string locate(void const* address)
{
    ::HMODULE module = nullptr;
    if (::GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            static_cast<::LPCWSTR>(address), &module) &&
        module != nullptr)
    {
        std::array<char, MAX_PATH> path{};
        auto const length = ::GetModuleFileNameA(
            module, path.data(), gsl::narrow_cast<::DWORD>(path.size()));
        std::string name{path.data(), length};
        if (auto const slash = name.find_last_of("\\/");
            slash != std::string::npos)
        {
            name.erase(0, slash + 1);
        }
        std::array<char, 32> offset{};
        std::snprintf(
            offset.data(), offset.size(), "+0x%X",
            gsl::narrow_cast<unsigned int>(
                reinterpret_cast<std::uintptr_t>(address) -
                reinterpret_cast<std::uintptr_t>(module)));
        return name + offset.data();
    }
    std::array<char, 64> text{};
    std::snprintf(
        text.data(), text.size(), "0x%p (no module: JIT-compiled Lua)",
        address);
    return text.data();
}

thread_local void* current_state = nullptr;

// The running Lua state's frames, innermost first. Only reads frame info
// (nothing is pushed), as this runs at the fault, before unwinding.
#pragma warning(push)
#pragma warning(disable : 26485)   // lua_Debug::short_src is a C array
std::string lua_frames()
{
    auto const s = static_cast<::lua_State*>(current_state);
    if (s == nullptr)
    {
        return {};
    }
    std::string text;
    ::lua_Debug record{};
    for (auto level = 0; level < 16 && ::lua_getstack(s, level, &record);
         ++level)
    {
        if (::lua_getinfo(s, "Sl", &record) == 0)
        {
            break;
        }
        if (record.currentline < 0)
        {
            continue;   // native frames
        }
        text += "\n  ";
        text += record.short_src;
        text += ':';
        text += std::to_string(record.currentline);
    }
    return text.empty() ? std::string{} : "\nLua stack at the fault:" + text;
}
#pragma warning(pop)

// Likely return addresses: stack values that point into a loaded module's
// code, innermost first (a heuristic walk; frame pointers aren't reliable).
#pragma warning(push)
#pragma warning(disable : 26490 26481 26429)
std::string native_frames(::EXCEPTION_POINTERS const* info)
{
    if (info == nullptr || info->ContextRecord == nullptr)
    {
        return {};
    }
    auto const esp = info->ContextRecord->Esp;
    ::MEMORY_BASIC_INFORMATION region{};
    if (::VirtualQuery(reinterpret_cast<void const*>(esp), &region, sizeof region) == 0)
    {
        return {};
    }
    auto const region_end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) +
                            region.RegionSize;
    auto const* slot = reinterpret_cast<std::uintptr_t const*>(esp);
    std::string text;
    auto found = 0;
    for (auto i = 0; i < 1024 && found < 24 &&
                     reinterpret_cast<std::uintptr_t>(slot + i + 1) <= region_end;
         ++i)
    {
        auto const value = slot[i];
        ::MEMORY_BASIC_INFORMATION target{};
        if (value < 0x10000 ||
            ::VirtualQuery(reinterpret_cast<void const*>(value), &target, sizeof target) == 0 ||
            target.Type != MEM_IMAGE ||
            (target.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                               PAGE_EXECUTE_WRITECOPY)) == 0)
        {
            continue;
        }
        text += "\n  " + locate(reinterpret_cast<void const*>(value));
        ++found;
    }
    return text.empty() ? std::string{} : "\nnative stack (likely return addresses):" + text;
}
#pragma warning(pop)

bool is_fault(unsigned int code) noexcept
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_PRIV_INSTRUCTION: return true;
    default: return false;
    }
}

// The signature is _set_se_translator's (info can't be const).
#pragma warning(push)
#pragma warning(disable : 26461)
void translate(unsigned int code, ::EXCEPTION_POINTERS* info)
{
    // Only hardware faults. Anything else (LuaJIT's own errors, which it
    // raises as structured exceptions and unwinds itself, debugger
    // notifications) goes on unchanged: returning leaves it as it was.
    if (!is_fault(code))
    {
        return;
    }

    std::string message = describe(code);
    std::array<char, 16> hex{};
    std::snprintf(hex.data(), hex.size(), " (0x%08X)", code);
    message += hex.data();

    if (info != nullptr && info->ExceptionRecord != nullptr)
    {
        auto const& record = *info->ExceptionRecord;
        message += " at " + locate(record.ExceptionAddress);
        if (code == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
        {
            auto const kind = gsl::at(record.ExceptionInformation, 0);
            std::array<char, 64> detail{};
            std::snprintf(
                detail.data(), detail.size(), ", %s address 0x%08X",
                kind == 0 ? "reading" : kind == 1 ? "writing" : "executing",
                gsl::narrow_cast<unsigned int>(
                    gsl::at(record.ExceptionInformation, 1)));
            message += detail.data();
        }
    }
    try
    {
        message += native_frames(info);
        message += lua_frames();
    }
    catch (...)
    {
    }
    // Logged here too: a Lua pcall that catches it only says "C++ exception".
    windower::error_log::write(u8"fault", std::u8string{message.begin(), message.end()});
    throw windower::seh_error{message};
}
#pragma warning(pop)

thread_local bool installed = false;

}

void windower::install_seh_translator() noexcept
{
    if (!installed)
    {
        installed = true;
        _set_se_translator(translate);
    }
}

windower::running_lua_state::running_lua_state(void* state) noexcept :
    m_previous{current_state}
{
    current_state = state;
}

windower::running_lua_state::~running_lua_state()
{
    current_state = m_previous;
}
