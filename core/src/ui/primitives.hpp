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

#ifndef WINDOWER_UI_PRIMITIVES_HPP
#define WINDOWER_UI_PRIMITIVES_HPP

#include "ui/color.hpp"
#include "ui/context.hpp"
#include "ui/ffxi_image.hpp"
#include "ui/patch.hpp"
#include "ui/rectangle.hpp"
#include "ui/text_layout_engine.hpp"
#include "ui/texture_token.hpp"
#include "ui/vector.hpp"

#include <array>
#include <span>

namespace windower::ui
{

class context;

namespace primitive
{

void set_texture(
    context& ctx, std::u8string_view name,
    std::size_t time_to_live = 1) noexcept;

void set_texture(
    context& ctx, ffxi_image const& image,
    std::size_t time_to_live = 1) noexcept;

void set_texture(context& ctx, texture_token texture) noexcept;

texture_token get_texture(
    context& ctx, std::u8string_view name,
    std::size_t time_to_live = 1) noexcept;

texture_token get_texture(
    context& ctx, ffxi_image const& image,
    std::size_t time_to_live = 1) noexcept;

void rectangle(
    context& ctx, ui::rectangle const& bounds,
    color color = colors::white) noexcept;

void rectangle(
    context& ctx, ui::rectangle const& bounds, patch const& patch,
    color color = colors::white) noexcept;

void rectangle(
    context& ctx, ui::rectangle const& bounds, nine_patch const& patch,
    color color = colors::white) noexcept;

void rectangle(
    context& ctx, ui::rectangle const& bounds, h_patch const& patch,
    color color = colors::white) noexcept;

void rectangle(
    context& ctx, ui::rectangle const& bounds, v_patch const& patch,
    color color = colors::white) noexcept;

// Oriel: the ellipse (a circle, for square bounds) inscribed in bounds. With
// a patch, that part of the texture is stretched over bounds and cut to the
// ellipse, so a round minimap is one call. rotation (radians, clockwise)
// turns the texture inside the shape, about the shape's center.
void ellipse(
    context& ctx, ui::rectangle const& bounds,
    color color = colors::white) noexcept;

void ellipse(
    context& ctx, ui::rectangle const& bounds, patch const& patch,
    color color = colors::white, float rotation = 0.f) noexcept;

// Oriel: rectangles (flat, and with a patch) are placed on whole pixels, so
// they're crisp. With this on, they're placed exactly instead: for small
// images that move with something drawn smoothly (dots and markers on a map
// that moves by fractions of a pixel), which otherwise jiggle against it.
// Whoever turns it on turns it off again before others draw.
void set_subpixel(bool enabled) noexcept;

// Oriel: a filled polygon with a smooth (one pixel, feathered) edge and an
// optional outline band inside it, outline_width UI pixels wide. points are
// its corners in order, either way round. It's drawn as a fan from the
// average of the corners, so the shape must be star-shaped about that point
// (any convex shape, or an arrow like the minimap's).
void polygon(
    context& ctx, std::span<ui::vector const> points, color fill,
    color outline = colors::transparent, float outline_width = 0.f) noexcept;

// void poly_line(
//     context& ctx, std::span<ui::vector> points,
//     color color = colors::white) noexcept;

void text(
    context& ctx, ui::rectangle const& bounds, std::u8string_view text,
    text_rasterization_options const& options = {},
    text_layout_options const& layout_options = {}) noexcept;

void text(
    context& ctx, vector const& position, text_layout const& layout,
    text_rasterization_options const& rasterizeration_options = {}) noexcept;

text_layout layout_text(
    context& ctx, dimension const& size, std::u8string_view text,
    text_layout_options const& layout_options = {}) noexcept;

}

}

#endif
