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

#ifndef WINDOWER_ERROR_LOG_HPP
#define WINDOWER_ERROR_LOG_HPP

#include <filesystem>
#include <string_view>

// Oriel: every error the core reports (core::error: add-on errors, failed
// commands, startup failures) is also appended to
// <user path>\logs\errors.log, with the time, so it can be read after the
// game's chat has scrolled past it. The file is started afresh (the old one
// kept as errors.old.log) when it passes 1 MB.
namespace windower::error_log
{

// Sets the folder (<user path>\logs). Until then, nothing is written.
void install(std::filesystem::path const& user_path) noexcept;

void write(std::u8string_view component, std::u8string_view text) noexcept;

}

#endif
