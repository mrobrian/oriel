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

#include "addon/modules/packages.hpp"

#include "addon/addon.hpp"
#include "addon/addon_manager.hpp"
#include "addon/lua.hpp"
#include "addon/modules/packages.lua.hpp"
#include "addon/package_manager.hpp"
#include "core.hpp"

#include <gsl/gsl>

#include <string_view>
#include <vector>

namespace
{

std::u8string_view type_name(windower::package_type type) noexcept
{
    switch (type)
    {
    case windower::package_type::library: return u8"library";
    case windower::package_type::service: return u8"service";
    case windower::package_type::addon: break;
    }
    return u8"addon";
}

void set_field(
    windower::lua::stack_guard const& guard, std::u8string_view key,
    std::u8string_view value)
{
    windower::lua::push(guard, key);
    windower::lua::push(guard, value);
    windower::lua::raw_set(guard, -3);
}

void set_field(
    windower::lua::stack_guard const& guard, std::u8string_view key, bool value)
{
    windower::lua::push(guard, key);
    windower::lua::push(guard, value);
    windower::lua::raw_set(guard, -3);
}

// Sets table[key] = { names... } on the table at the top of the stack.
void set_list(
    windower::lua::stack_guard const& guard, std::u8string_view key,
    std::vector<std::u8string> const& names)
{
    windower::lua::push(guard, key);
    windower::lua::create_table(guard, names.size());
    auto index = 1;
    for (auto const& name : names)
    {
        windower::lua::push(guard, name);
        windower::lua::raw_set(guard, -2, index++);
    }
    windower::lua::raw_set(guard, -3);
}

// installed() -> { { name, version, type, path, is_local, dependencies }, ... }
int installed(windower::lua::state s)
{
    using namespace windower;

    lua::stack_guard guard{s};
    auto const& manager = core::instance().package_manager;
    auto const packages =
        manager ? manager->installed_packages()
                : std::vector<std::shared_ptr<package const>>{};

    lua::create_table(guard, packages.size());
    auto index = 1;
    for (auto const& p : packages)
    {
        std::vector<std::u8string> dependencies;
        for (auto const& d : p->dependencies())
        {
            dependencies.push_back(d.name());
        }

        lua::create_table(guard, 0, 6);
        set_field(guard, u8"name", p->name());
        set_field(guard, u8"version", to_u8string(p->version()));
        set_field(guard, u8"type", type_name(p->type()));
        set_field(guard, u8"path", p->path().u8string());
        set_field(guard, u8"is_local", !p->can_update());
        set_list(guard, u8"dependencies", dependencies);
        lua::raw_set(guard, -2, index++);
    }
    return guard.release();
}

// available() -> { { name, version, type, dependencies }, ... }
int available(windower::lua::state s)
{
    using namespace windower;

    lua::stack_guard guard{s};
    auto const& manager = core::instance().package_manager;
    auto const packages = manager ? manager->available_packages()
                                  : std::vector<available_package>{};

    lua::create_table(guard, packages.size());
    auto index = 1;
    for (auto const& p : packages)
    {
        lua::create_table(guard, 0, 4);
        set_field(guard, u8"name", p.name);
        set_field(guard, u8"version", to_u8string(p.version));
        set_field(guard, u8"type", p.type);
        set_list(guard, u8"dependencies", p.dependencies);
        lua::raw_set(guard, -2, index++);
    }
    return guard.release();
}

// loaded() -> { name, ... } (loaded add-ons)
int loaded(windower::lua::state s)
{
    using namespace windower;

    lua::stack_guard guard{s};
    std::vector<std::u8string> names;
    if (auto const& addons = core::instance().addon_manager)
    {
        for (auto const& a : addons->loaded())
        {
            if (auto const p = a->package())
            {
                names.push_back(p->name());
            }
        }
    }

    lua::create_table(guard, names.size());
    auto index = 1;
    for (auto const& name : names)
    {
        lua::push(guard, name);
        lua::raw_set(guard, -2, index++);
    }
    return guard.release();
}

}

int windower::load_packages_module(lua::state s)
{
    lua::stack_guard guard{s};

    lua::load(guard, lua_packages_source, u8"core.packages");
    lua::push(guard, ::installed);
    lua::push(guard, ::available);
    lua::push(guard, ::loaded);
    lua::call(guard, 3);

    return guard.release();
}
