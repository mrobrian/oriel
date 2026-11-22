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

#include "diag.hpp"

#include <windows.h>

#include <gsl/gsl>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

void windower::diag(std::string_view text) noexcept
{
    try
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> const lock{mutex};

        static std::ofstream stream = [] {
            std::array<wchar_t, MAX_PATH + 1> buffer{};
            ::GetTempPathW(gsl::narrow_cast<::DWORD>(buffer.size()), buffer.data());
            auto const dir = std::filesystem::path{buffer.data()} / L"Oriel";
            std::filesystem::create_directories(dir);
            return std::ofstream{
                dir / L"diag.log", std::ios::binary | std::ios::app};
        }();

        stream << ::GetTickCount64() << " [" << ::GetCurrentThreadId() << "] "
               << text << "\r\n";
        stream.flush();
    }
    catch (...)
    {
    }
}

std::string windower::diag_stack(unsigned long skip) noexcept
{
    try
    {
        std::array<void*, 16> frames{};
        auto const count = ::RtlCaptureStackBackTrace(
            skip + 1, gsl::narrow_cast<::DWORD>(frames.size()), frames.data(),
            nullptr);

        std::ostringstream out;
        for (auto i = 0; i < count; ++i)
        {
            auto const address = gsl::at(frames, i);
            ::HMODULE module   = nullptr;
            out << " <- ";
            if (::GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    static_cast<::LPCWSTR>(address), &module))
            {
                std::array<wchar_t, MAX_PATH + 1> name{};
                ::GetModuleFileNameW(
                    module, name.data(), gsl::narrow_cast<::DWORD>(name.size()));
                out << std::filesystem::path{name.data()}.filename().string()
                    << "+0x" << std::hex
                    << (std::bit_cast<std::uintptr_t>(address) -
                        std::bit_cast<std::uintptr_t>(module))
                    << std::dec;
            }
            else
            {
                out << "0x" << std::hex << std::bit_cast<std::uintptr_t>(address)
                    << std::dec;
            }
        }
        return out.str();
    }
    catch (...)
    {
        return {};
    }
}
