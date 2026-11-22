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

#include "dat_overlays.hpp"

#include "hooklib/hook.hpp"
#include "utility.hpp"

#include <windows.h>

#include <gsl/gsl>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{

namespace hooks
{
windower::hooklib::hook<decltype(::CreateFileA)> CreateFileA;
windower::hooklib::hook<decltype(::FindFirstFileA)> FindFirstFileA;
}

std::mutex overlay_mutex;
std::vector<std::filesystem::path> overlay_roots;
// Lower-cased "rom...\..." suffix -> replacement file (empty: no overlay has it)
std::unordered_map<std::string, std::filesystem::path> resolved;

constexpr char lower(char c) noexcept
{
    return c >= 'A' && c <= 'Z' ? gsl::narrow_cast<char>(c - 'A' + 'a') : c;
}

constexpr bool is_rom_folder(std::string_view segment) noexcept
{
    if (segment.size() < 3)
    {
        return false;
    }
    auto it = segment.begin();
    if (lower(*it++) != 'r' || lower(*it++) != 'o' || lower(*it++) != 'm')
    {
        return false;
    }
    return std::all_of(
        it, segment.end(), [](char c) noexcept { return c >= '0' && c <= '9'; });
}

// Returns the part of a game path starting at its ROM folder, e.g.
// "C:\...\FINAL FANTASY XI//ROM3/12/34.DAT" -> "ROM3/12/34.DAT".
//
// Only matches a ROM folder preceded by a forward slash. The game engine
// requests its data files with Unix-style paths ("...//ROM3/12/34.DAT"),
// while PlayOnline and the updater use normal backslash paths, so this keeps
// the updater checking the real files. (Same rule XIPivot uses.)
std::optional<std::string_view> rom_suffix(std::string_view path) noexcept
{
    auto result = std::optional<std::string_view>{};
    auto start  = std::size_t{0};
    while (start < path.size())
    {
        auto const end = path.find_first_of("\\/", start);
        if (end == std::string_view::npos)
        {
            break;
        }
        if (start > 0 && path.substr(start - 1, 1) == "/" &&
            is_rom_folder(path.substr(start, end - start)))
        {
            result = path.substr(start);
        }
        start = end + 1;
    }
    return result;
}

std::optional<std::filesystem::path> find_overlay(::LPCSTR path)
{
    if (!path)
    {
        return std::nullopt;
    }

    auto const suffix = rom_suffix(path);
    if (!suffix)
    {
        return std::nullopt;
    }

    auto key = std::string{*suffix};
    std::transform(key.begin(), key.end(), key.begin(), [](char c) noexcept {
        return c == '/' ? '\\' : lower(c);
    });

    std::lock_guard<std::mutex> const lock{overlay_mutex};
    if (overlay_roots.empty())
    {
        return std::nullopt;
    }

    auto it = resolved.find(key);
    if (it == resolved.end())
    {
        auto found = std::filesystem::path{};
        for (auto const& root : overlay_roots)
        {
            auto candidate = root / key;
            auto const attributes =
                ::GetFileAttributesW(candidate.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                !(attributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                found = std::move(candidate);
                break;
            }
        }
        it = resolved.emplace(std::move(key), std::move(found)).first;
    }

    if (it->second.empty())
    {
        return std::nullopt;
    }
    return it->second;
}

constexpr bool is_read_only(::DWORD access, ::DWORD disposition) noexcept
{
    constexpr auto write_access = GENERIC_WRITE | GENERIC_ALL |
                                  FILE_WRITE_DATA | FILE_APPEND_DATA |
                                  DELETE | WRITE_DAC | WRITE_OWNER;
    return disposition == OPEN_EXISTING && !(access & write_access);
}

void load_overlay_list(std::filesystem::path const& user)
{
    std::ifstream stream{user / u8"dat_overlays.txt", std::ios::binary};
    if (!stream)
    {
        return;
    }

    auto line = std::string{};
    auto roots = std::vector<std::filesystem::path>{};
    while (std::getline(stream, line))
    {
        if (line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
        {
            line.erase(0, 3);
        }
        auto const first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line.at(first) == '#')
        {
            continue;
        }
        auto const last = line.find_last_not_of(" \t\r");
        auto const text = line.substr(first, last - first + 1);

        auto path = std::filesystem::path{
            std::u8string{text.begin(), text.end()}};
        if (path.is_relative())
        {
            path = user / u8"overlays" / path;
        }
        if (std::filesystem::is_directory(path))
        {
            roots.push_back(std::move(path));
        }
    }

    std::lock_guard<std::mutex> const lock{overlay_mutex};
    overlay_roots = std::move(roots);
    resolved.clear();
}

namespace callbacks
{

::HANDLE WINAPI CreateFileA(
    ::LPCSTR lpFileName, ::DWORD dwDesiredAccess, ::DWORD dwShareMode,
    ::LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    ::DWORD dwCreationDisposition, ::DWORD dwFlagsAndAttributes,
    ::HANDLE hTemplateFile) noexcept
{
    if (is_read_only(dwDesiredAccess, dwCreationDisposition))
    {
        try
        {
            if (auto const overlay = find_overlay(lpFileName))
            {
                auto const handle = ::CreateFileW(
                    overlay->c_str(), dwDesiredAccess, dwShareMode,
                    lpSecurityAttributes, dwCreationDisposition,
                    dwFlagsAndAttributes, hTemplateFile);
                if (handle != INVALID_HANDLE_VALUE)
                {
                    return handle;
                }
            }
        }
        catch (...)
        {
        }
    }
    return hooks::CreateFileA(
        lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
        dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

::HANDLE WINAPI FindFirstFileA(
    ::LPCSTR lpFileName, ::LPWIN32_FIND_DATAA lpFindFileData) noexcept
{
    try
    {
        if (auto const overlay = find_overlay(lpFileName))
        {
            auto const ansi = overlay->string();
            auto const handle =
                hooks::FindFirstFileA(ansi.c_str(), lpFindFileData);
            if (handle != INVALID_HANDLE_VALUE)
            {
                return handle;
            }
        }
    }
    catch (...)
    {
    }
    return hooks::FindFirstFileA(lpFileName, lpFindFileData);
}

}

}

void windower::dat_overlays::install(std::filesystem::path const& user_path)
{
    if (!hooks::CreateFileA)
    {
        try
        {
            load_overlay_list(user_path);
        }
        catch (...)
        {
            return;
        }

        if (overlay_roots.empty())
        {
            return;
        }

        hooks::CreateFileA = hooklib::make_hook(
            u8"kernel32.dll", u8"CreateFileA", callbacks::CreateFileA);
        hooks::FindFirstFileA = hooklib::make_hook(
            u8"kernel32.dll", u8"FindFirstFileA", callbacks::FindFirstFileA);
    }
}

void windower::dat_overlays::uninstall() noexcept
{
    hooks::CreateFileA    = {};
    hooks::FindFirstFileA = {};
}
