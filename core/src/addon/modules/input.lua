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

-- core.input: raw mouse and keyboard events, delivered as they happen.
-- A handler returns true to keep the event from the game (and from later
-- handlers' "blocked" check), like Windower 4's 'mouse' and 'keyboard' events.
--
--   input.mouse(function(kind, x, y, delta, blocked) ... end)
--       kind: 0 move, 1/2 left down/up, 3 left double click, 4/5 right
--       down/up, 6 right double click, 7/8 middle down/up, 9 middle double
--       click, 10 wheel (delta: +1 per notch up, -1 down). x, y in UI pixels.
--   input.keyboard(function(dik, pressed, flags, blocked) ... end)
--       dik: DirectInput key code. flags: 1 for a key-repeat press.
--
-- Both return a function that removes the handler.

local registry, dispatch_key = ...

local handlers = {mouse = {}, keyboard = {}}

local remove = function(list, fn)
    return function()
        for i = #list, 1, -1 do
            if list[i] == fn then
                table.remove(list, i)
            end
        end
    end
end

registry[dispatch_key] = function(kind, ...)
    local list = handlers[kind]
    if not list or #list == 0 then
        return select(select('#', ...), ...)
    end
    local n = select('#', ...)
    local args = {...}
    local blocked = args[n]
    local snapshot = {}
    for i = 1, #list do
        snapshot[i] = list[i]
    end
    for i = 1, #snapshot do
        args[n] = blocked
        local ok, result = pcall(snapshot[i], unpack(args, 1, n))
        if not ok then
            print('input handler error: ' .. tostring(result))
        elseif result then
            blocked = true
        end
    end
    return blocked
end

local register = function(kind)
    return function(fn)
        assert(type(fn) == 'function', 'handler must be a function')
        local list = handlers[kind]
        list[#list + 1] = fn
        return remove(list, fn)
    end
end

return {
    mouse = register('mouse'),
    keyboard = register('keyboard'),
}
