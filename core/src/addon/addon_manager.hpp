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

#ifndef WINDOWER_ADDON_ADDON_MANAGER_HPP
#define WINDOWER_ADDON_ADDON_MANAGER_HPP

#include "addon/addon.hpp"
#include "addon/scheduler.hpp"

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace windower
{

class package;

class addon_manager
{
public:
    addon_manager()                     = default;
    addon_manager(addon_manager const&) = delete;
    addon_manager(addon_manager&&)      = delete;

    ~addon_manager() noexcept;

    addon_manager& operator=(addon_manager const&) = delete;
    addon_manager& operator=(addon_manager&&)      = delete;

    addon const* get(std::u8string_view) const;
    std::vector<std::unique_ptr<addon>> const& loaded() const noexcept;

    void load(std::vector<std::u8string> const&);
    void unload(std::vector<std::u8string> const&);
    void reload(std::vector<std::u8string> const&);
    void unload_all() noexcept;
    void reload_all();

    void run_until_idle();

    // Oriel: runs the action now, or, if add-ons are being run (so loads and
    // unloads are being queued), after those queued changes.
    void after_pending_changes(std::function<void()> action);

    // Oriel: while held, loads and unloads are queued; the last release runs
    // them. Hold while calling into add-ons.
    void hold_changes() noexcept;
    void release_changes();

    void raise_error(gsl::not_null<package const*>, std::exception_ptr);

private:
    std::mutex m_mutex;
    std::vector<std::unique_ptr<addon>> m_loaded_addons;

    // Oriel: loads and unloads requested while add-ons are being run (an
    // add-on running /load or /unload, or an add-on being unloaded after an
    // error) are queued and run afterwards; changing m_loaded_addons while it
    // was being iterated left the loop using a destroyed add-on.
    int m_hold = 0;
    std::vector<std::function<void()>> m_deferred;

    void load(std::vector<std::shared_ptr<package const>> const&);
    void unload(std::vector<std::shared_ptr<package const>> const&);
};

}

#endif
