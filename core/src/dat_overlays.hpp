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

#ifndef WINDOWER_DAT_OVERLAYS_HPP
#define WINDOWER_DAT_OVERLAYS_HPP

#include <filesystem>

// DAT overlays: serve replacement game data files (HD textures, remapped
// models and so on) from overlay folders instead of the game's ROM folders,
// without touching the game install.
//
// The overlay list is read from <user path>\dat_overlays.txt, one folder per
// line, highest priority first. A line is either a folder name inside
// <user path>\overlays\ or an absolute path. Blank lines and lines starting
// with '#' are ignored. Each overlay folder mirrors the game's layout, e.g.
// <overlay>\ROM\0\1.DAT or <overlay>\ROM3\12\34.DAT.

namespace windower::dat_overlays
{

void install(std::filesystem::path const& user_path);
void uninstall() noexcept;

}

#endif
