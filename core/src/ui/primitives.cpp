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

#include "ui/primitives.hpp"

#include "ui/color.hpp"
#include "ui/context.hpp"
#include "ui/ffxi_image.hpp"
#include "ui/patch.hpp"
#include "ui/rectangle.hpp"
#include "ui/texture_loaders.hpp"
#include "ui/vector.hpp"
#include "ui/vertex.hpp"

#include <gsl/gsl>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace windower::ui::primitive
{

namespace
{
// Oriel: see set_subpixel.
bool subpixel_enabled = false;

// A rectangle's edge, in physical pixels: on a whole pixel (crisp), unless
// sub-pixel placement is on.
float snap(float value) noexcept
{
    return subpixel_enabled ? value : std::round(value);
}
}

void set_subpixel(bool enabled) noexcept { subpixel_enabled = enabled; }

void set_texture(
    context& ctx, std::u8string_view name, std::size_t time_to_live) noexcept
{
    set_texture(ctx, get_texture(ctx, name, time_to_live));
}

void set_texture(
    context& ctx, ffxi_image const& image, std::size_t time_to_live) noexcept
{
    set_texture(ctx, get_texture(ctx, image, time_to_live));
}

void set_texture(context& ctx, texture_token texture) noexcept
{
    ctx.set_texture(texture);
}

texture_token get_texture(
    context& ctx, std::u8string_view name, std::size_t time_to_live) noexcept
{
    return load_texture(ctx, name, time_to_live).token;
}

texture_token get_texture(
    context& ctx, ffxi_image const& image, std::size_t time_to_live) noexcept
{
    return load_texture(ctx, image, time_to_live).token;
}

void rectangle(context& ctx, ui::rectangle const& bounds, color c) noexcept
{
    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();

    auto transform = ctx.current_transform();

    auto x0 = bounds.x0;
    auto y0 = bounds.y0;
    auto x1 = bounds.x1;
    auto y1 = bounds.y1;

    if (x1 < x0)
    {
        std::swap(x0, x1);
        transform = transform * transform::scale(-1, 1);
    }

    if (y1 < y0)
    {
        std::swap(y0, y1);
        transform = transform * transform::scale(1, -1);
    }

    transform = scale_correction(transform, scale);

    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;

    auto const scale_x = scale.x * zoom;
    auto const scale_y = scale.y * zoom;

    x0 = snap(scale_x * x0);
    y0 = snap(scale_y * y0);
    x1 = snap(scale_x * x1);
    y1 = snap(scale_y * y1);

    auto p0 = transform * vector{x0, y0};
    auto p1 = transform * vector{x1, y0};
    auto p2 = transform * vector{x0, y1};
    auto p3 = transform * vector{x1, y1};

    if (determinant(transform) < 0)
    {
        using std::swap;

        swap(p0, p3);
        swap(p1, p2);
    }

    c = to_associated_alpha(c);

    set_texture(ctx, no_texture);
    ctx.draw_triangle_list(
        {{p0.x + origin_x, p0.y + origin_y, depth, rhw, 0, 0, c},
         {p1.x + origin_x, p1.y + origin_y, depth, rhw, 0, 0, c},
         {p2.x + origin_x, p2.y + origin_y, depth, rhw, 0, 0, c},
         {p3.x + origin_x, p3.y + origin_y, depth, rhw, 0, 0, c}},
        {0, 1, 2, 3, 2, 1});
}

void rectangle(
    context& ctx, ui::rectangle const& bounds, patch const& patch,
    color c) noexcept
{
    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();

    auto transform = ctx.current_transform();

    auto x0 = bounds.x0;
    auto y0 = bounds.y0;
    auto x1 = bounds.x1;
    auto y1 = bounds.y1;

    if (x1 < x0)
    {
        std::swap(x0, x1);
        transform = transform * transform::scale(-1, 1);
    }

    if (y1 < y0)
    {
        std::swap(y0, y1);
        transform = transform * transform::scale(1, -1);
    }

    transform = scale_correction(transform, scale);

    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;

    auto const overdraw_l = patch.overdraw.left;
    auto const overdraw_t = patch.overdraw.top;
    auto const overdraw_r = patch.overdraw.right;
    auto const overdraw_b = patch.overdraw.bottom;

    auto const w0 = patch.bounds.width();
    auto const h0 = patch.bounds.height();

    auto const w = w0 - overdraw_l - overdraw_r;
    auto const h = h0 - overdraw_t - overdraw_b;

    auto const scale_x = scale.x * zoom;
    auto const scale_y = scale.y * zoom;

    x0 = snap(scale_x * x0);
    y0 = snap(scale_y * y0);
    x1 = snap(scale_x * x1);
    y1 = snap(scale_y * y1);

    if (x0 >= x1 || y0 >= y1)
    {
        return;
    }

    auto const x_ratio = (x1 - x0) / std::max(w, 1.f);
    auto const y_ratio = (y1 - y0) / std::max(h, 1.f);

    x0 -= x_ratio * overdraw_l;
    y0 -= y_ratio * overdraw_t;
    x1 += x_ratio * overdraw_r;
    y1 += y_ratio * overdraw_b;

    auto const tx0 = patch.bounds.x0 / patch.texture_size.width;
    auto const tx1 = patch.bounds.x1 / patch.texture_size.width;

    auto const ty0 = patch.bounds.y0 / patch.texture_size.height;
    auto const ty1 = patch.bounds.y1 / patch.texture_size.height;

    auto const p0 = transform * vector{x0, y0};
    auto const p1 = transform * vector{x1, y0};
    auto const p2 = transform * vector{x0, y1};
    auto const p3 = transform * vector{x1, y1};

    c = to_associated_alpha(c);

    ctx.draw_triangle_list(
        {{p0.x + origin_x, p0.y + origin_y, depth, rhw, tx0, ty0, c},
         {p1.x + origin_x, p1.y + origin_y, depth, rhw, tx1, ty0, c},
         {p2.x + origin_x, p2.y + origin_y, depth, rhw, tx0, ty1, c},
         {p3.x + origin_x, p3.y + origin_y, depth, rhw, tx1, ty1, c}},
        {0, 1, 2, 3, 2, 1}, determinant(transform) < 0);
}

void rectangle(
    context& ctx, ui::rectangle const& bounds, nine_patch const& patch,
    color c) noexcept
{
    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();

    auto transform = ctx.current_transform();

    auto x0 = bounds.x0;
    auto y0 = bounds.y0;
    auto x3 = bounds.x1;
    auto y3 = bounds.y1;

    if (x3 < x0)
    {
        std::swap(x0, x3);
        transform = transform * transform::scale(-1, 1);
    }

    if (y3 < y0)
    {
        std::swap(y0, y3);
        transform = transform * transform::scale(1, -1);
    }

    transform = scale_correction(transform, scale);

    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;

    auto const overdraw_l = patch.overdraw.left;
    auto const overdraw_t = patch.overdraw.top;
    auto const overdraw_r = patch.overdraw.right;
    auto const overdraw_b = patch.overdraw.bottom;

    auto const w0 = patch.slice.left;
    auto const h0 = patch.slice.top;
    auto const w2 = patch.slice.right;
    auto const h2 = patch.slice.bottom;

    auto const scale_x = scale.x * zoom;
    auto const scale_y = scale.y * zoom;

    x0 = std::round(scale_x * x0);
    y0 = std::round(scale_y * y0);
    x3 = std::round(scale_x * x3);
    y3 = std::round(scale_y * y3);

    auto x1 = x0 + scale_x * (w0 - overdraw_l);
    auto y1 = y0 + scale_y * (h0 - overdraw_t);
    auto x2 = x3 - scale_x * (w2 - overdraw_r);
    auto y2 = y3 - scale_y * (h2 - overdraw_b);

    if (x1 > x2)
    {
        auto const a = w0 - overdraw_l;
        auto const b = w2 - overdraw_r;

        auto const x_ratio = (x3 - x0) / (a + b);

        x1 = x0 + x_ratio * a;
        x2 = x1;
        x0 -= x_ratio * overdraw_l;
        x3 += x_ratio * overdraw_r;
    }
    else
    {
        x0 -= scale_x * overdraw_l;
        x3 += scale_x * overdraw_r;
    }

    if (y1 > y2)
    {
        auto const a = h0 - overdraw_t;
        auto const b = h2 - overdraw_b;

        auto const y_ratio = (y3 - y0) / (a + b);

        y1 = y0 + y_ratio * a;
        y2 = y1;
        y0 -= y_ratio * overdraw_t;
        y3 += y_ratio * overdraw_b;
    }
    else
    {
        y0 -= scale_y * overdraw_t;
        y3 += scale_y * overdraw_b;
    }

    auto const tx0 = patch.bounds.x0 / patch.texture_size.width;
    auto const tx3 = patch.bounds.x1 / patch.texture_size.width;
    auto const tx1 = tx0 + patch.slice.left / patch.texture_size.width;
    auto const tx2 = tx3 - patch.slice.right / patch.texture_size.width;

    auto const ty0 = patch.bounds.y0 / patch.texture_size.height;
    auto const ty3 = patch.bounds.y1 / patch.texture_size.height;
    auto const ty1 = ty0 + patch.slice.top / patch.texture_size.height;
    auto const ty2 = ty3 - patch.slice.bottom / patch.texture_size.height;

    auto const p00 = transform * vector{x0, y0};
    auto const p01 = transform * vector{x1, y0};
    auto const p02 = transform * vector{x2, y0};
    auto const p03 = transform * vector{x3, y0};
    auto const p04 = transform * vector{x0, y1};
    auto const p05 = transform * vector{x1, y1};
    auto const p06 = transform * vector{x2, y1};
    auto const p07 = transform * vector{x3, y1};
    auto const p08 = transform * vector{x0, y2};
    auto const p09 = transform * vector{x1, y2};
    auto const p10 = transform * vector{x2, y2};
    auto const p11 = transform * vector{x3, y2};
    auto const p12 = transform * vector{x0, y3};
    auto const p13 = transform * vector{x1, y3};
    auto const p14 = transform * vector{x2, y3};
    auto const p15 = transform * vector{x3, y3};

    c = to_associated_alpha(c);

    ctx.draw_triangle_list(
        {{p00.x + origin_x, p00.y + origin_y, depth, rhw, tx0, ty0, c},
         {p01.x + origin_x, p01.y + origin_y, depth, rhw, tx1, ty0, c},
         {p02.x + origin_x, p02.y + origin_y, depth, rhw, tx2, ty0, c},
         {p03.x + origin_x, p03.y + origin_y, depth, rhw, tx3, ty0, c},
         {p04.x + origin_x, p04.y + origin_y, depth, rhw, tx0, ty1, c},
         {p05.x + origin_x, p05.y + origin_y, depth, rhw, tx1, ty1, c},
         {p06.x + origin_x, p06.y + origin_y, depth, rhw, tx2, ty1, c},
         {p07.x + origin_x, p07.y + origin_y, depth, rhw, tx3, ty1, c},
         {p08.x + origin_x, p08.y + origin_y, depth, rhw, tx0, ty2, c},
         {p09.x + origin_x, p09.y + origin_y, depth, rhw, tx1, ty2, c},
         {p10.x + origin_x, p10.y + origin_y, depth, rhw, tx2, ty2, c},
         {p11.x + origin_x, p11.y + origin_y, depth, rhw, tx3, ty2, c},
         {p12.x + origin_x, p12.y + origin_y, depth, rhw, tx0, ty3, c},
         {p13.x + origin_x, p13.y + origin_y, depth, rhw, tx1, ty3, c},
         {p14.x + origin_x, p14.y + origin_y, depth, rhw, tx2, ty3, c},
         {p15.x + origin_x, p15.y + origin_y, depth, rhw, tx3, ty3, c}},
        {0, 1, 4,  4,  1, 5,  1, 2,  5,  5,  2,  6,  2,  3,  6,  6,  3,  7,
         4, 5, 8,  8,  5, 9,  5, 6,  9,  9,  6,  10, 6,  7,  10, 10, 7,  11,
         8, 9, 12, 12, 9, 13, 9, 10, 13, 13, 10, 14, 10, 11, 14, 14, 11, 15},
        determinant(transform) < 0);
}

void rectangle(
    context& ctx, ui::rectangle const& bounds, h_patch const& patch,
    color c) noexcept
{
    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();

    auto transform = ctx.current_transform();

    auto x0 = bounds.x0;
    auto y0 = bounds.y0;
    auto x3 = bounds.x1;
    auto y1 = bounds.y1;

    if (x3 < x0)
    {
        std::swap(x0, x3);
        transform = transform * transform::scale(-1, 1);
    }

    if (y1 < y0)
    {
        std::swap(y0, y1);
        transform = transform * transform::scale(1, -1);
    }

    transform = scale_correction(transform, scale);

    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;

    auto const overdraw_l = patch.overdraw.left;
    auto const overdraw_t = patch.overdraw.top;
    auto const overdraw_r = patch.overdraw.right;
    auto const overdraw_b = patch.overdraw.bottom;

    auto const w0 = patch.slice.left;
    auto const w2 = patch.slice.right;
    auto const h0 = patch.bounds.height();

    auto const h = h0 - overdraw_t - overdraw_b;

    auto const scale_x = scale.x * zoom;
    auto const scale_y = scale.y * zoom;

    x0 = std::round(scale_x * x0);
    y0 = std::round(scale_y * y0);
    x3 = std::round(scale_x * x3);
    y1 = std::round(scale_y * y1);

    auto x1 = x0 + scale_x * (w0 - overdraw_l);
    auto x2 = x3 - scale_x * (w2 - overdraw_r);

    if (x1 > x2)
    {
        auto const a = w0 - overdraw_l;
        auto const b = w2 - overdraw_r;

        auto const x_ratio = (x3 - x0) / (a + b);

        x1 = x0 + x_ratio * a;
        x2 = x1;
        x0 -= x_ratio * overdraw_l;
        x3 += x_ratio * overdraw_r;
    }
    else
    {
        x0 -= scale_x * overdraw_l;
        x3 += scale_x * overdraw_r;
    }

    auto const y_ratio = (y1 - y0) / std::max(h, 1.f);

    y0 -= y_ratio * overdraw_t;
    y1 += y_ratio * overdraw_b;

    auto const tx0 = patch.bounds.x0 / patch.texture_size.width;
    auto const tx3 = patch.bounds.x1 / patch.texture_size.width;
    auto const tx1 = tx0 + patch.slice.left / patch.texture_size.width;
    auto const tx2 = tx3 - patch.slice.right / patch.texture_size.width;

    auto const ty0 = patch.bounds.y0 / patch.texture_size.height;
    auto const ty1 = patch.bounds.y1 / patch.texture_size.height;

    auto const p0 = transform * vector{x0, y0};
    auto const p1 = transform * vector{x1, y0};
    auto const p2 = transform * vector{x2, y0};
    auto const p3 = transform * vector{x3, y0};
    auto const p4 = transform * vector{x0, y1};
    auto const p5 = transform * vector{x1, y1};
    auto const p6 = transform * vector{x2, y1};
    auto const p7 = transform * vector{x3, y1};

    c = to_associated_alpha(c);

    ctx.draw_triangle_list(
        {{p0.x + origin_x, p0.y + origin_y, depth, rhw, tx0, ty0, c},
         {p1.x + origin_x, p1.y + origin_y, depth, rhw, tx1, ty0, c},
         {p2.x + origin_x, p2.y + origin_y, depth, rhw, tx2, ty0, c},
         {p3.x + origin_x, p3.y + origin_y, depth, rhw, tx3, ty0, c},
         {p4.x + origin_x, p4.y + origin_y, depth, rhw, tx0, ty1, c},
         {p5.x + origin_x, p5.y + origin_y, depth, rhw, tx1, ty1, c},
         {p6.x + origin_x, p6.y + origin_y, depth, rhw, tx2, ty1, c},
         {p7.x + origin_x, p7.y + origin_y, depth, rhw, tx3, ty1, c}},
        {0, 1, 4, 4, 1, 5, 1, 2, 5, 5, 2, 6, 2, 3, 6, 6, 3, 7},
        determinant(transform) < 0);
}

void rectangle(
    context& ctx, ui::rectangle const& bounds, v_patch const& patch,
    color c) noexcept
{
    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();

    auto transform = ctx.current_transform();

    auto x0 = bounds.x0;
    auto y0 = bounds.y0;
    auto x1 = bounds.x1;
    auto y3 = bounds.y1;

    if (x1 < x0)
    {
        std::swap(x0, x1);
        transform = transform * transform::scale(-1, 1);
    }

    if (y3 < y0)
    {
        std::swap(y0, y3);
        transform = transform * transform::scale(1, -1);
    }

    transform = scale_correction(transform, scale);

    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;

    auto const overdraw_l = patch.overdraw.left;
    auto const overdraw_t = patch.overdraw.top;
    auto const overdraw_r = patch.overdraw.right;
    auto const overdraw_b = patch.overdraw.bottom;

    auto const w0 = patch.bounds.width();
    auto const h0 = patch.slice.top;
    auto const h2 = patch.slice.bottom;

    auto const w = w0 - overdraw_l - overdraw_r;

    auto const scale_x = scale.x * zoom;
    auto const scale_y = scale.y * zoom;

    x0 = std::round(scale_x * x0);
    y0 = std::round(scale_y * y0);
    x1 = std::round(scale_x * x1);
    y3 = std::round(scale_y * y3);

    auto const x_ratio = (x1 - x0) / std::max(w, 1.f);

    x0 -= x_ratio * overdraw_l;
    x1 += x_ratio * overdraw_r;

    auto y1 = y0 + scale_y * (h0 - overdraw_t);
    auto y2 = y3 - scale_y * (h2 - overdraw_b);

    if (y1 > y2)
    {
        auto const a = h0 - overdraw_t;
        auto const b = h2 - overdraw_b;

        auto const y_ratio = (y3 - y0) / (a + b);

        y1 = y0 + y_ratio * a;
        y2 = y1;
        y0 -= y_ratio * overdraw_t;
        y3 += y_ratio * overdraw_b;
    }
    else
    {
        y0 -= scale_y * overdraw_t;
        y3 += scale_y * overdraw_b;
    }

    auto const tx0 = patch.bounds.x0 / patch.texture_size.width;
    auto const tx1 = patch.bounds.x1 / patch.texture_size.width;

    auto const ty0 = patch.bounds.y0 / patch.texture_size.height;
    auto const ty3 = patch.bounds.y1 / patch.texture_size.height;
    auto const ty1 = ty0 + patch.slice.top / patch.texture_size.height;
    auto const ty2 = ty3 - patch.slice.bottom / patch.texture_size.height;

    auto const p0 = transform * vector{x0, y0};
    auto const p1 = transform * vector{x1, y0};
    auto const p2 = transform * vector{x0, y1};
    auto const p3 = transform * vector{x1, y1};
    auto const p4 = transform * vector{x0, y2};
    auto const p5 = transform * vector{x1, y2};
    auto const p6 = transform * vector{x0, y3};
    auto const p7 = transform * vector{x1, y3};

    c = to_associated_alpha(c);

    ctx.draw_triangle_list(
        {{p0.x + origin_x, p0.y + origin_y, depth, rhw, tx0, ty0, c},
         {p1.x + origin_x, p1.y + origin_y, depth, rhw, tx1, ty0, c},
         {p2.x + origin_x, p2.y + origin_y, depth, rhw, tx0, ty1, c},
         {p3.x + origin_x, p3.y + origin_y, depth, rhw, tx1, ty1, c},
         {p4.x + origin_x, p4.y + origin_y, depth, rhw, tx0, ty2, c},
         {p5.x + origin_x, p5.y + origin_y, depth, rhw, tx1, ty2, c},
         {p6.x + origin_x, p6.y + origin_y, depth, rhw, tx0, ty3, c},
         {p7.x + origin_x, p7.y + origin_y, depth, rhw, tx1, ty3, c}},
        {0, 1, 2, 2, 1, 3, 2, 3, 4, 4, 3, 5, 4, 5, 6, 6, 5, 7},
        determinant(transform) < 0);
}

// void poly_line(
//     context& ctx, [[maybe_unused]] std::span<ui::vector> points,
//     [[maybe_unused]] color c) noexcept
//{
//     auto const scale  = ctx.scale_factor();
//     auto const origin = ctx.origin();
//
//     data_segment<vertex> vertices;
//     data_segment<std::uint16_t> indices;
//     std::iota(indices.data.begin(), indices.data.end(), vertices.offset);
//
//     auto offset    = std::size_t{0};
//     auto remaining = points.size();
//     while (remaining > 1)
//     {
//         auto const chunk_size = std::min(remaining, vertices.data.size());
//         auto const chunk      = points.subspan(offset, chunk_size);
//         offset += chunk.size() - 1;
//         remaining -= chunk.size() - 1;
//         std::transform(
//             chunk.begin(), chunk.end(), vertices.data.begin(), [=](auto& p) {
//                 auto const x = (p.x + origin.x) * scale.x;
//                 auto const y = (p.y + origin.y) * scale.y;
//                 return vertex{x, y, c};
//             });
//         // auto const vertex_span = std::span{vertices}.subspan(0,
//         chunk_size);
//         // auto const index_span = std::span{indices}.subspan(0, chunk_size);
//         // ctx.line_strip(vertex_span, index_span);
//     }
// }

namespace
{
// Oriel: an ellipse as a fan of triangles around its center. tex is the
// texture region (normalized; all zero for a flat color).
void ellipse_impl(
    context& ctx, ui::rectangle const& bounds, ui::rectangle const& tex,
    color c, float rotation) noexcept
{
    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();

    auto transform = ctx.current_transform();

    auto x0 = bounds.x0;
    auto y0 = bounds.y0;
    auto x1 = bounds.x1;
    auto y1 = bounds.y1;

    if (x1 < x0)
    {
        std::swap(x0, x1);
        transform = transform * transform::scale(-1, 1);
    }

    if (y1 < y0)
    {
        std::swap(y0, y1);
        transform = transform * transform::scale(1, -1);
    }

    transform = scale_correction(transform, scale);

    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;

    auto const scale_x = scale.x * zoom;
    auto const scale_y = scale.y * zoom;

    x0 = scale_x * x0;
    y0 = scale_y * y0;
    x1 = scale_x * x1;
    y1 = scale_y * y1;

    if (x0 >= x1 || y0 >= y1)
    {
        return;
    }

    auto const cx = (x0 + x1) / 2;
    auto const cy = (y0 + y1) / 2;
    auto const rx = (x1 - x0) / 2;
    auto const ry = (y1 - y0) / 2;

    // About 3 pixels per edge, between 24 and 256 edges.
    constexpr auto pi = 3.14159265358979323846f;
    auto const perimeter = 2 * pi * std::max(rx, ry);
    auto const segments  = gsl::narrow_cast<std::uint16_t>(
        std::clamp(std::ceil(perimeter / 3.f), 24.f, 256.f));

    auto const cos_r = std::cos(rotation);
    auto const sin_r = std::sin(rotation);
    auto const tw    = tex.x1 - tex.x0;
    auto const th    = tex.y1 - tex.y0;

    c = to_associated_alpha(c);
    auto const clear = to_associated_alpha(ui::color{0});

    // (dx, dy): position relative to the center, in units of the radii
    // (-1..1). The texture is sampled turned back by the rotation, so the
    // image appears turned by it.
    auto const make_vertex = [&](float dx, float dy, color vc) noexcept {
        auto const p  = transform * vector{cx + rx * dx, cy + ry * dy};
        auto const tu = cos_r * dx + sin_r * dy;
        auto const tv = -sin_r * dx + cos_r * dy;
        return vertex{
            p.x + origin_x,
            p.y + origin_y,
            depth,
            rhw,
            tex.x0 + tw * (tu + 1) / 2,
            tex.y0 + th * (tv + 1) / 2,
            vc};
    };

    // The edge fades out over one pixel (half inside, half outside), so it
    // isn't jagged: a fan of triangles to an inner ring, then a band from
    // the inner ring to a transparent outer one.
    auto const in_x  = (rx - .5f) / rx;
    auto const in_y  = (ry - .5f) / ry;
    auto const out_x = (rx + .5f) / rx;
    auto const out_y = (ry + .5f) / ry;

    std::vector<vertex> vertices;
    std::vector<std::uint16_t> indices;
    vertices.reserve(2 * static_cast<std::size_t>(segments) + 1);
    indices.reserve(9 * static_cast<std::size_t>(segments));

    vertices.push_back(make_vertex(0, 0, c));
    for (std::uint16_t i = 0; i < segments; ++i)
    {
        auto const angle = 2 * pi * i / segments;
        auto const dx    = std::cos(angle);
        auto const dy    = std::sin(angle);
        vertices.push_back(make_vertex(in_x * dx, in_y * dy, c));
        vertices.push_back(make_vertex(out_x * dx, out_y * dy, clear));

        auto const inner      = gsl::narrow_cast<std::uint16_t>(2 * i + 1);
        auto const outer      = gsl::narrow_cast<std::uint16_t>(2 * i + 2);
        auto const next_inner = gsl::narrow_cast<std::uint16_t>(
            i + 1 < segments ? 2 * i + 3 : 1);
        auto const next_outer = gsl::narrow_cast<std::uint16_t>(
            i + 1 < segments ? 2 * i + 4 : 2);

        indices.push_back(0);
        indices.push_back(inner);
        indices.push_back(next_inner);

        indices.push_back(inner);
        indices.push_back(outer);
        indices.push_back(next_outer);

        indices.push_back(inner);
        indices.push_back(next_outer);
        indices.push_back(next_inner);
    }

    ctx.draw_triangle_list(
        std::span<vertex const>{vertices},
        std::span<std::uint16_t const>{indices}, determinant(transform) < 0);
}
}

void ellipse(context& ctx, ui::rectangle const& bounds, color c) noexcept
{
    set_texture(ctx, no_texture);
    ellipse_impl(ctx, bounds, {0, 0, 0, 0}, c, 0.f);
}

void ellipse(
    context& ctx, ui::rectangle const& bounds, patch const& patch, color c,
    float rotation) noexcept
{
    ui::rectangle const tex{
        patch.bounds.x0 / patch.texture_size.width,
        patch.bounds.y0 / patch.texture_size.height,
        patch.bounds.x1 / patch.texture_size.width,
        patch.bounds.y1 / patch.texture_size.height};
    ellipse_impl(ctx, bounds, tex, c, rotation);
}

// Oriel: see primitives.hpp. The corners are moved in and out along their
// miters to make rings: the fill is a fan from the center to the innermost
// ring, then bands between rings carry the outline and the feathered edge.
void polygon(
    context& ctx, std::span<ui::vector const> points, color fill,
    color outline, float outline_width) noexcept
{
    auto const n = points.size();
    if (n < 3 || n > 1000)
    {
        return;
    }
    set_texture(ctx, no_texture);

    auto const origin       = ctx.origin();
    auto const zoom         = ctx.zoom_factor();
    auto const scale        = ctx.scale_factor();
    auto const [depth, rhw] = ctx.depth();
    auto const transform =
        scale_correction(ctx.current_transform(), scale);
    auto const origin_x = std::round(scale.x * origin.x) - .5f;
    auto const origin_y = std::round(scale.y * origin.y) - .5f;
    auto const scale_x  = scale.x * zoom;
    auto const scale_y  = scale.y * zoom;

    // Physical pixels, turned clockwise (on screen) if they weren't.
    std::vector<float> px(n);
    std::vector<float> py(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        px.at(i) = scale_x * gsl::at(points, i).x;
        py.at(i) = scale_y * gsl::at(points, i).y;
    }
    auto area = 0.f;
    for (std::size_t i = 0; i < n; ++i)
    {
        auto const j = (i + 1) % n;
        area += px.at(i) * py.at(j) - px.at(j) * py.at(i);
    }
    if (area == 0)
    {
        return;
    }
    if (area < 0)
    {
        std::reverse(px.begin(), px.end());
        std::reverse(py.begin(), py.end());
    }

    // Outward normals of the edges (corner i to i + 1), then each corner's
    // miter: how far it moves for its edges to move one pixel out.
    std::vector<float> nx(n);
    std::vector<float> ny(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        auto const j   = (i + 1) % n;
        auto const dx  = px.at(j) - px.at(i);
        auto const dy  = py.at(j) - py.at(i);
        auto const len = std::sqrt(dx * dx + dy * dy);
        nx.at(i) = len > 0 ? dy / len : 0;
        ny.at(i) = len > 0 ? -dx / len : 0;
    }
    std::vector<float> mx(n);
    std::vector<float> my(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        auto const h   = (i + n - 1) % n;
        auto const dot = nx.at(h) * nx.at(i) + ny.at(h) * ny.at(i);
        auto const k   = 1 / std::max(1 + dot, .125f);
        auto x         = (nx.at(h) + nx.at(i)) * k;
        auto y         = (ny.at(h) + ny.at(i)) * k;
        auto const len = std::sqrt(x * x + y * y);
        if (len > 4)
        {
            x = x * 4 / len;
            y = y * 4 / len;
        }
        mx.at(i) = x;
        my.at(i) = y;
    }

    auto cx = 0.f;
    auto cy = 0.f;
    for (std::size_t i = 0; i < n; ++i)
    {
        cx += px.at(i);
        cy += py.at(i);
    }
    cx /= gsl::narrow_cast<float>(n);
    cy /= gsl::narrow_cast<float>(n);

    auto const fill_c    = to_associated_alpha(fill);
    auto const outline_c = to_associated_alpha(outline);
    auto const clear     = to_associated_alpha(ui::color{0});

    // Rings, inside out: (offset from the edge in pixels, color).
    std::vector<std::pair<float, color>> rings;
    auto const width = outline_width * scale_x;
    if (width > 0)
    {
        auto const w = std::max(width, 1.f);
        rings.emplace_back(-w - .5f, fill_c);
        rings.emplace_back(-w + .5f, outline_c);
        if (w > 1)
        {
            rings.emplace_back(-.5f, outline_c);
        }
        rings.emplace_back(.5f, clear);
    }
    else
    {
        rings.emplace_back(-.5f, fill_c);
        rings.emplace_back(.5f, clear);
    }

    auto const make_vertex = [&](float x, float y, color vc) noexcept {
        auto const p = transform * vector{x, y};
        return vertex{p.x + origin_x, p.y + origin_y, depth, rhw, 0, 0, vc};
    };

    std::vector<vertex> vertices;
    std::vector<std::uint16_t> indices;
    vertices.reserve(1 + rings.size() * n);
    indices.reserve(3 * n + 6 * n * rings.size());

    vertices.push_back(make_vertex(cx, cy, fill_c));
    for (auto const& [offset, ring_color] : rings)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            auto dx = mx.at(i) * offset;
            auto dy = my.at(i) * offset;
            if (offset < 0)
            {
                // Inward, a sharp corner of a small shape could pass the
                // center and fold the fill over: at most 80% of the way.
                auto const to_cx = cx - px.at(i);
                auto const to_cy = cy - py.at(i);
                auto const reach = .8f * std::sqrt(to_cx * to_cx + to_cy * to_cy);
                auto const moved = std::sqrt(dx * dx + dy * dy);
                if (moved > reach && moved > 0)
                {
                    dx = dx * reach / moved;
                    dy = dy * reach / moved;
                }
            }
            vertices.push_back(
                make_vertex(px.at(i) + dx, py.at(i) + dy, ring_color));
        }
    }

    auto const at = [n](std::size_t ring, std::size_t i) noexcept {
        return gsl::narrow_cast<std::uint16_t>(1 + ring * n + i % n);
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        indices.push_back(0);
        indices.push_back(at(0, i));
        indices.push_back(at(0, i + 1));
    }
    for (std::size_t r = 0; r + 1 < rings.size(); ++r)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            indices.push_back(at(r, i));
            indices.push_back(at(r + 1, i));
            indices.push_back(at(r + 1, i + 1));

            indices.push_back(at(r, i));
            indices.push_back(at(r + 1, i + 1));
            indices.push_back(at(r, i + 1));
        }
    }

    ctx.draw_triangle_list(
        std::span<vertex const>{vertices},
        std::span<std::uint16_t const>{indices}, determinant(transform) < 0);
}

void text(
    context& ctx, ui::rectangle const& bounds, std::u8string_view text,
    text_rasterization_options const& rasterization_options,
    text_layout_options const& layout_options) noexcept
{
    primitive::text(
        ctx, bounds.position(),
        layout_text(ctx, bounds.size(), text, layout_options),
        rasterization_options);
}

void text(
    context& ctx, vector const& position, text_layout const& layout,
    text_rasterization_options const& rasterization_options) noexcept
{
    auto const scale = ctx.scale_factor();

    auto& rasterizer = ctx.text_rasterizer();

    auto const count = rasterizer.chunk_count(ctx, layout);
    for (auto chunk = std::size_t{0}; chunk < count; ++chunk)
    {
        auto const texture =
            rasterizer.rasterize(ctx, layout, chunk, rasterization_options);

        auto const size = vector{texture.patch.bounds.size()} / scale;
        ctx.set_texture(texture.token);
        rectangle(ctx, {position, position + size}, texture.patch);
    }
}

text_layout layout_text(
    context& ctx, dimension const& size, std::u8string_view text,
    text_layout_options const& layout_options) noexcept
{
    return ctx.text_layout_engine().layout(ctx, text, size, layout_options);
}

}
