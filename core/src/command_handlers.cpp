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

#include "command_handlers.hpp"

#include "command_manager.hpp"
#include "core.hpp"
#include "errors/command_error.hpp"
#include "unicode.hpp"
#include "utilities/coroutine.hpp"
#include "utility.hpp"

#include "addon/addon.hpp"
#include "addon/addon_manager.hpp"
#include "addon/package_manager.hpp"

#include <algorithm>
#include <limits>
#include <map>

namespace
{
constexpr auto unlimited = std::numeric_limits<std::size_t>::max();

// Oriel: package commands say in chat what they did, wherever they're run
// from (typed, a binding, init.lua or an add-on).
void say(std::u8string_view text)
{
    windower::core::output(u8"Oriel", text, windower::command_source::user);
}

// A version as people write it: 1.2.0.0 is "1.2", 1.2.3.0 is "1.2.3".
std::u8string short_version(windower::package_version const& v)
{
    using windower::to_u8string;
    std::u8string result = to_u8string(v.major) + u8"." + to_u8string(v.minor);
    if (v.revision != 0 || v.build != 0)
    {
        result += u8"." + to_u8string(v.revision);
    }
    if (v.build != 0)
    {
        result += u8"." + to_u8string(v.build);
    }
    if (!v.tag.empty())
    {
        result += u8" " + v.tag;
    }
    return result;
}

bool is_loaded(std::u8string_view name)
{
    return windower::core::instance().addon_manager->get(name) != nullptr;
}

// "altana 1.2", or just the name if it's no longer installed.
std::u8string name_and_version(std::u8string const& name)
{
    auto const package =
        windower::core::instance().package_manager->get_package(name);
    return package ? name + u8" " + short_version(package->version()) : name;
}

// The names that are installed packages other than libraries; the rest are
// reported and left out.
std::vector<std::u8string> packages_named(std::vector<std::u8string> const& args)
{
    auto const& core = windower::core::instance();
    std::vector<std::u8string> names;
    for (auto const& name : args)
    {
        auto const package = core.package_manager->get_package(name);
        if (!package)
        {
            say(u8"No package named " + name + u8" is installed.");
        }
        else if (package->type() == windower::package_type::library)
        {
            say(name +
                u8" is a library: the add-ons that use it load it.");
        }
        else if (std::find(names.begin(), names.end(), name) == names.end())
        {
            names.push_back(name);
        }
    }
    return names;
}

std::size_t loaded_addon_count()
{
    auto const& loaded = windower::core::instance().addon_manager->loaded();
    return gsl::narrow_cast<std::size_t>(std::count_if(
        loaded.begin(), loaded.end(), [](auto const& a) {
            return a->package()->type() == windower::package_type::addon;
        }));
}

std::u8string plural(std::size_t n, std::u8string_view one, std::u8string_view many)
{
    return windower::to_u8string(n) + u8" " + std::u8string{n == 1 ? one : many};
}

void check_args(
    std::u8string_view command_name, std::vector<std::u8string> const& args,
    std::size_t min, std::size_t max)
{
    using namespace windower;

    auto const count = args.size();
    if (count < min)
    {
        std::u8string message;
        message.append(u8"Too few arguments; expected: ");
        message.append(to_u8string(min));
        message.append(u8", got: ");
        message.append(to_u8string(count));

        throw command_error{message, command_name};
    }
    else if (count > max)
    {
        std::u8string message;
        message.append(u8"Too many arguments; expected: ");
        message.append(to_u8string(min));
        message.append(u8", got: ");
        message.append(to_u8string(count));

        throw command_error{message, command_name};
    }
}

void check_args(
    std::u8string_view command_name, std::vector<std::u8string> const& args,
    std::size_t expected)
{
    check_args(command_name, args, expected, expected);
}

std::future<void> install_impl(std::vector<std::u8string> const& args)
{
    check_args(u8"/install", args, 1, unlimited);
    auto const& core = windower::core::instance();
    auto updated     = co_await core.package_manager->install(args);
    if (core.addon_manager)
    {
        core.addon_manager->reload(updated);
    }
}

std::future<void> update_impl(std::vector<std::u8string> const& args)
{
    check_args(u8"/update", args, 1, unlimited);
    auto const& core = windower::core::instance();
    auto updated     = co_await core.package_manager->update(args);
    if (core.addon_manager)
    {
        core.addon_manager->reload(updated);
    }
}

std::future<void> updateall_impl(std::vector<std::u8string> const& args)
{
    check_args(u8"/updateall", args, 0, 1);
    auto const& core = windower::core::instance();
    auto const force = !args.empty() && gsl::at(args, 0) == u8"force";
    auto updated     = co_await core.package_manager->update_all(force);
    if (core.addon_manager)
    {
        core.addon_manager->reload(updated);
    }
}
};

void windower::command_handlers::install(
    std::vector<std::u8string> const& args, windower::command_source)
{
    install_impl(args);
}

void windower::command_handlers::uninstall(
    std::vector<std::u8string> const& args, windower::command_source)
{
    check_args(u8"/uninstall", args, 1, unlimited);

    auto const& core = core::instance();
    auto packages    = core.package_manager->unload_order(args);
    std::vector<std::shared_ptr<windower::package const>> dependents;
    while (!packages.empty())
    {
        if (std::find(args.begin(), args.end(), packages.back()->name()) ==
            args.end())
        {
            dependents.push_back(packages.back());
        }
        packages.pop_back();
    }

    if (!dependents.empty())
    {
        std::u8string message = u8"Uninstall failed.\n The following ";
        message += dependents.size() == 1 ? u8"package depends on "
                                          : u8"packages depend on ";
        message += args.size() == 1 ? u8"this package:\n"
                                    : u8"one or more of these packages:\n";
        for (auto const& package : dependents)
        {
            message += u8"    ";
            message += package->name();
        }
        throw windower_error{message};
    }

    if (core.addon_manager)
    {
        core.addon_manager->unload(args);
        // Oriel: when an add-on runs /uninstall, the unload above is queued
        // until add-ons finish running; remove the files only after it.
        core.addon_manager->after_pending_changes(
            [args]() { core::instance().package_manager->uninstall(args); });
        return;
    }

    core.package_manager->uninstall(args);
}

void windower::command_handlers::update(
    std::vector<std::u8string> const& args, windower::command_source)
{
    update_impl(args);
}

void windower::command_handlers::updateall(
    std::vector<std::u8string> const& args, windower::command_source)
{
    updateall_impl(args);
}

void windower::command_handlers::load(
    std::vector<std::u8string> const& args, windower::command_source)
{
    check_args(u8"/load", args, 1, unlimited);
    auto const& core = core::instance();
    if (core.addon_manager)
    {
        std::vector<std::u8string> names;
        for (auto const& name : packages_named(args))
        {
            if (is_loaded(name))
            {
                say(name + u8" is already loaded (/reload " + name +
                    u8" to load it again).");
            }
            else
            {
                names.push_back(name);
            }
        }
        if (names.empty())
        {
            return;
        }
        core.addon_manager->load(names);
        // An add-on's /load waits until add-ons finish running.
        core.addon_manager->after_pending_changes([names]() {
            for (auto const& name : names)
            {
                say(is_loaded(name)
                        ? u8"Loaded " + name_and_version(name) + u8"."
                        : name + u8" didn't load.");
            }
        });
    }
    else
    {
        throw command_error{u8"Addon manager is not initialized", u8"/load"};
    }
}

void windower::command_handlers::unload(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/unload", args, 1, unlimited);
    auto const& core = core::instance();
    if (core.addon_manager)
    {
        std::vector<std::u8string> names;
        for (auto const& name : packages_named(args))
        {
            if (is_loaded(name))
            {
                names.push_back(name);
            }
            else
            {
                say(name + u8" isn't loaded.");
            }
        }
        if (names.empty())
        {
            return;
        }
        core.addon_manager->unload(names);
        core.addon_manager->after_pending_changes([names]() {
            for (auto const& name : names)
            {
                say(is_loaded(name) ? name + u8" is still loaded."
                                    : u8"Unloaded " + name + u8".");
            }
        });
    }
    else
    {
        throw command_error{u8"Addon manager is not initialized", u8"/unload"};
    }
}

void windower::command_handlers::reload(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/reload", args, 1, unlimited);
    auto const& core = core::instance();
    if (core.addon_manager)
    {
        auto const names = packages_named(args);
        if (names.empty())
        {
            return;
        }
        std::map<std::u8string, bool> was_loaded;
        for (auto const& name : names)
        {
            was_loaded[name] = is_loaded(name);
        }
        core.addon_manager->reload(names);
        core.addon_manager->after_pending_changes([names, was_loaded]() {
            for (auto const& name : names)
            {
                if (!is_loaded(name))
                {
                    say(name + u8" didn't load.");
                }
                else
                {
                    say((was_loaded.at(name) ? u8"Reloaded " : u8"Loaded ") +
                        name_and_version(name) + u8".");
                }
            }
        });
    }
    else
    {
        throw command_error{u8"Addon manager is not initialized", u8"/reload"};
    }
}

void windower::command_handlers::unloadall(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/unloadall", args, 0);
    auto const& core = core::instance();
    if (core.addon_manager)
    {
        auto const count = loaded_addon_count();
        core.addon_manager->unload_all();
        core.addon_manager->after_pending_changes([count]() {
            say(u8"Unloaded " + plural(count, u8"add-on", u8"add-ons") + u8".");
        });
    }
    else
    {
        throw command_error{
            u8"Addon manager is not initialized", u8"/unloadall"};
    }
}

void windower::command_handlers::reloadall(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/reloadall", args, 0);
    auto const& core = core::instance();
    if (core.addon_manager)
    {
        core.addon_manager->reload_all();
        core.addon_manager->after_pending_changes([]() {
            say(u8"Reloaded " +
                plural(loaded_addon_count(), u8"add-on", u8"add-ons") + u8".");
        });
    }
    else
    {
        throw command_error{
            u8"Addon manager is not initialized", u8"/reloadall"};
    }
}

void windower::command_handlers::alias(
    std::vector<std::u8string> const& args, command_source)
{
    if (args.at(0).length() > 6)
    {
        auto parsed = command_manager::get_arguments(args.at(0).substr(7), 1);
        check_args(u8"/alias", parsed, 2);
        std::u8string_view command = gsl::at(parsed, 1);
        auto index                 = std::size_t{};
        auto next_index            = index;
        while (is_whitespace(next_code_point(command, next_index)))
        {
            index = next_index;
        }
        command.remove_prefix(index);
        command_manager::instance().register_alias(gsl::at(parsed, 0), command);
    }
}

void windower::command_handlers::unalias(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/unalias", args, 1);
    command_manager::instance().unregister_alias(gsl::at(args, 0));
}

void windower::command_handlers::bind(
    std::vector<std::u8string> const& args, command_source)
{
    if (args.at(0).length() > 5)
    {
        core::instance().binding_manager.bind(gsl::at(args, 0).substr(6));
    }
}

void windower::command_handlers::unbind(
    std::vector<std::u8string> const& args, command_source)
{
    if (args.at(0).length() > 7)
    {
        core::instance().binding_manager.unbind(gsl::at(args, 0).substr(8));
    }
}

void windower::command_handlers::listbinds(
    std::vector<std::u8string> const& args, command_source source)
{
    check_args(u8"/listbinds", args, 0);
    auto const& bindings = core::instance().binding_manager.get_binds();
    if (bindings.empty())
    {
        core::output(u8"core", u8"No key bindings registered.", source);
        return;
    }
    core::output(
        u8"core",
        u8"Total bindings: " + windower::to_u8string(bindings.size()));
    for (auto const& binding : bindings)
    {
        core::output(u8"core", binding.first + u8": " + binding.second, source);
    }
}

void windower::command_handlers::exec(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/exec", args, 1);
    core::instance().script_environment.execute(gsl::at(args, 0));
}

void windower::command_handlers::eval(
    std::vector<std::u8string> const& args, command_source)
{
    auto arg = std::u8string_view{args.at(0)};
    if (arg.length() > 5)
    {
        arg.remove_prefix(6);
        core::instance().script_environment.evaluate(arg);
    }
}

void windower::command_handlers::reset(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/reset", args, 0);
    core::instance().script_environment.reset();
}

void windower::command_handlers::pkg(
    std::vector<std::u8string> const& args, command_source source)
{
    check_args(u8"/pkg", args, 1, unlimited);
    auto const& core = core::instance();
    if (gsl::at(args, 0) == u8"reload")
    {
        check_args(u8"/pkg", args, 1);
        // Oriel: say what changed (added, removed or a new version).
        std::map<std::u8string, std::u8string> before;
        for (auto const& p : core.package_manager->installed_packages())
        {
            before[p->name()] = to_u8string(p->version());
        }
        core.package_manager->reset();
        std::vector<std::u8string> changed;
        for (auto const& p : core.package_manager->installed_packages())
        {
            auto const it = before.find(p->name());
            if (it == before.end() || it->second != to_u8string(p->version()))
            {
                changed.push_back(p->name());
            }
            if (it != before.end())
            {
                before.erase(it);
            }
        }
        for (auto const& gone : before)
        {
            changed.push_back(gone.first);
        }
        std::sort(changed.begin(), changed.end());
        std::u8string message = u8"Reloaded the package list (";
        if (changed.empty())
        {
            message += u8"no changes";
        }
        else
        {
            message += plural(changed.size(), u8"changed", u8"changed");
            if (changed.size() <= 6)
            {
                message += u8": ";
                for (std::size_t i = 0; i < changed.size(); ++i)
                {
                    message += (i == 0 ? u8"" : u8", ") + gsl::at(changed, i);
                }
            }
        }
        say(message + u8").");
    }
    else if (gsl::at(args, 0) == u8"listsrc")
    {
        check_args(u8"/pkg", args, 1);
        core::output(u8"package manager", u8"listing sources...", source);
        for (auto const& s : core.package_manager->sources())
        {
            windower::core::output(u8"package manager", s, source);
        }
    }
    else if (gsl::at(args, 0) == u8"addsrc")
    {
        check_args(u8"/pkg", args, 2);
        core.package_manager->add_source(gsl::at(args, 1));
    }
    else if (gsl::at(args, 0) == u8"removesrc")
    {
        check_args(u8"/pkg", args, 2);
        core.package_manager->remove_source(gsl::at(args, 1));
    }
    else
    {
        std::u8string message;
        message.append(u8"Unrecognized package manager sub-command \"");
        message.append(gsl::at(args, 0));
        message.append(u8"\"");

        throw command_error{message, u8"/pkg"};
    }
}

void windower::command_handlers::nextwindow(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/nextwindow", args, 0);
    core::instance().ui.activate_next_window();
}

void windower::command_handlers::prevwindow(
    std::vector<std::u8string> const& args, command_source)
{
    check_args(u8"/prevwindow", args, 0);
    core::instance().ui.activate_previous_window();
}
