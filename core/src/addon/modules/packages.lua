--[[
Copyright © Oriel contributors

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation files
(the "Software"),to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
]]

-- core.packages: what packages are installed, available and loaded, and
-- functions to install, update, remove, load and unload them.
--
--   packages.installed() -> { { name, version, type, path, is_local, dependencies }, ... }
--   packages.available() -> { { name, version, type, dependencies }, ... }
--   packages.loaded()    -> { name, ... }  (loaded add-ons)
--
-- type is 'addon', 'library' or 'service'. is_local is true for packages in a
-- development override folder (overrides.xml), which are never updated.
--
-- The actions run the matching core commands (/install, /load, ...), so they
-- behave exactly like typing them. Installs and updates finish in the
-- background; call installed() again later to see the result.

local command = require('core.command')

local installed, available, loaded = ...

local check_name = function(name)
    if type(name) ~= 'string' or not name:match('^[%w_]+$') then
        error('invalid package name: ' .. tostring(name), 3)
    end
    return name
end

local run = function(verb)
    return function(name)
        command.input('/' .. verb .. ' ' .. check_name(name))
    end
end

local packages = {
    installed = installed,
    available = available,
    loaded = loaded,

    install = run('install'),
    update = run('update'),
    uninstall = run('uninstall'),
    load = run('load'),
    unload = run('unload'),
    reload = run('reload'),
}

packages.update_all = function()
    command.input('/updateall')
end

return packages
