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

#include "error_log.hpp"

#include <windows.h>

#include <gsl/gsl>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

std::mutex log_mutex;
std::filesystem::path log_file;
bool log_started = false;

constexpr std::uintmax_t max_size = 1024 * 1024;

std::string timestamp()
{
    ::SYSTEMTIME time{};
    ::GetLocalTime(&time);
    std::array<char, 32> buffer{};
    auto const length = std::snprintf(
        buffer.data(), buffer.size(), "%04u-%02u-%02u %02u:%02u:%02u.%03u",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond, time.wMilliseconds);
    if (length <= 0)
    {
        return {};
    }
    return std::string{buffer.data()};
}

std::string as_chars(std::u8string_view text)
{
    // UTF-8 text, written as bytes.
    std::string result;
    result.reserve(text.size());
    for (auto const c : text)
    {
        result.push_back(gsl::narrow_cast<char>(c));
    }
    return result;
}

}

void windower::error_log::install(
    std::filesystem::path const& user_path) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock{log_mutex};
        log_file    = user_path / u8"logs" / u8"errors.log";
        log_started = false;
    }
    catch (...)
    {
    }
}

void windower::error_log::write(
    std::u8string_view component, std::u8string_view text) noexcept
{
    try
    {
        std::lock_guard<std::mutex> lock{log_mutex};
        if (log_file.empty())
        {
            return;
        }

        std::error_code error;
        if (!log_started)
        {
            // Once a session: make the folder, and start afresh past 1 MB.
            log_started = true;
            std::filesystem::create_directories(log_file.parent_path(), error);
            if (std::filesystem::exists(log_file, error) &&
                std::filesystem::file_size(log_file, error) > max_size)
            {
                auto old = log_file;
                old.replace_filename(u8"errors.old.log");
                std::filesystem::rename(log_file, old, error);
            }
        }

        std::ofstream stream{log_file, std::ios::binary | std::ios::app};
        if (!stream)
        {
            return;
        }
        stream << timestamp();
        if (!component.empty())
        {
            stream << " [" << as_chars(component) << ']';
        }
        stream << ' ' << as_chars(text) << "\r\n";
    }
    catch (...)
    {
    }
}
