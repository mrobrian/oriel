# Oriel TODO

Ordered by priority: easy items first, because they let us test the core
with little code. Altana is last; it depends on new core UI features.

| # | Task | Size | What it tests |
|---|------|------|---------------|
| ✓ | Startup hardening + error log | Small, core C++ | One service failing at game start skipped all later services and init.lua (found 2026-11-22: items_service). Now each service starts on its own and init.lua always runs; failures show in chat. Every core error is also written to Saved Games\Oriel\logs\errors.log. Built in Debug; Release relink needs the game closed |
| 1c | Chat feedback for package commands | Small, core C++ | Loading, unloading and reloading packages (`/load`, `/unload`, `/reload`, `/pkg reload`, and the same from init.lua) say in chat what they did: "Loaded altana 1.2", "Unloaded probe", "altana is already loaded", "Reloaded package list (3 changed)". Today they're silent on success, so you can't tell whether anything happened (e.g. `/load` of an add-on that's still loaded does nothing) |
| 1b | Desktop shortcut per profile | Small, C# launcher | A "Create desktop shortcut" option for a profile: the shortcut starts Oriel and launches the game with that profile straight away (launcher command-line option for the profile) |
| ✓ | Fix Alt sticking after Alt+Tab | Tiny, core C++ | Keyboard and focus handling (fixed: DirectInput wrapper reports input lost while unfocused) |
| ✓ | DAT overlays (XIPivot replacement) | Medium, core C++ | Hooking the game's file opens (works, order verified) |
| ✓ | Load packages at startup | Tiny | Startup scripts (works) |
| 4 | Port Azure | Medium (2.2k lines) | Resources (spells), commands, settings |
| 5 | Fast POL login | Unknown; research first | Hooks inside PlayOnline (polcore.dll) |
| ✓ | Package browser in the launcher | Medium, C# | Built: "…" menu → Packages; install/update/remove + load at startup. Needs a UI check |
| 7 | Port MobFinder | Large (7.6k lines) | Entities, zone data, larger UI |
| 8 | In-game package browser (as a package) | Medium | Windows, lists, scrolling, buttons. Built: D:\oriel-packages\pkgbrowser + core.packages module; needs core rebuild + in-game test |
| 9 | Port Outfitter | Large (10k lines) | Equipment, items, big data tables |
| 10 | Core UI features: text layering, smooth scrolling, clipping, masked images | Core C++ | Needed by Altana |
| 12 | Windower 4 compatibility, built in | Large, mostly Lua | Unchanged W4 add-ons in `Saved Games\Oriel\addons` load with `//lua load`; `//cmd` routing; packet sending allowed for W4 add-ons; plugins unsupported (clear message). Plus a `w4compat` package for module-at-a-time ports |
| 11 | Port Altana | Very large (58k lines, 147 files) | Running via a Windower 4 compatibility layer (D:/oriel-packages/altana/compat); offline-tested, needs in-game test. Native rewrites of parts can come later |

## Notes

0. **Stuck Alt.** Oriel builds the game's keyboard state from window
   messages. An Alt press still queued when focus moves away was handled
   after the focus-loss reset, so Alt stayed held; the release went to the
   other window. First fix (in `binding_manager.cpp`): ignore key presses
   while the game isn't the foreground window, and reset all keys on
   regaining focus too. That wasn't enough: the game also reads keys through
   Windows' per-thread keyboard state (`GetKeyboardState`), which still had
   Alt down. Second fix: rebuild that state from the physical keyboard on
   focus changes, and keep doing so for 1 s after regaining focus.
   Needs an in-game test.
1. **DAT overlays.** Fenestra has no built-in overlay support, despite
   Windower 5's plans. Added `core/src/dat_overlays.cpp`: hooks `CreateFileA`
   and `FindFirstFileA`. Game paths of the form `...//ROM*/...` (the engine's
   forward-slash style; the updater uses backslashes and is left alone, same
   rule as XIPivot) are served from the first overlay folder that has the
   file. Overlay list: `Saved Games\Oriel\dat_overlays.txt`, one folder per
   line, highest priority first; a name under `Saved Games\Oriel\overlays\`
   or a full path. It currently points at the Windower 4 XIPivot folders
   (47 GB, not copied): Remapster, XIView, NextHD, AshenbubsHD-Prime,
   AshenbubsHD-Basic. No music/sound overlays (XIPivot's `\win\se\` and
   `\win\music\` handling not needed). Needs an in-game test. Later: a
   launcher setting and an in-game command to list active overlays.
   **Startup loading** (done): `Saved Games\Oriel\scripts\init.lua` runs at
   game start; `require('core.command').input('/load config')`.
2. **Omen / Skillchains.** Not ported: they stay Windower 4 add-ons and run
   through the built-in compatibility layer (12). Good early test cases for it.
4. **Azure.** Ours, and small. It's the first test of the resources library.
5. **Fast POL login.** Skip the PlayOnline screens after login and start FFXI.
   Oriel already passes `/game eAZcFcB`, which only adds a quick-start button.
   Windower 4's option and Ashita's `quicky` plugin are both closed source.
   Oriel's core is already loaded inside pol.exe, so the plan is:
   (a) find the polcore.dll function the "Play" button calls and call it
   once login finishes; fallback (b) press the buttons for the user
   (send Enter to the POL window) once the post-login screen appears.
   Option per profile, off by default. Only changes local behaviour; no
   network traffic is added.
6. **Launcher package browser.** Reads `packages.xml` from the package
   sources. That file has no descriptions, so we add our own or read each
   package's manifest.
7. **MobFinder.** Ours. Needs entity and zone data in Windower 5's libraries.
8. **In-game browser.** Builds on 6; a good first real UI package before Altana.
9. **Outfitter.** Ours; mostly data, so the port is mostly API swaps.
10. **Core UI work.** The clip stack and scroll panel already exist; the
    stencil mask commands (`begin_mask`/`apply_mask`/`end_mask`) exist but
    are unused and incomplete (nothing writes the stencil, and the game's
    depth buffer may have none through the D3D8 wrapper). Instead, added
    `ui.primitive.ellipse(x, y, w, h[, texture[, patch]][, color[, rotation]])`
    (2026-11-21): the image region drawn as a many-sided polygon, so a round
    minimap is one call, with optional rotation inside the circle. Built;
    in-game check with the `shapetest` package (`/load shapetest`). Next:
    soft (anti-aliased) edges if the polygon edge looks jagged; arbitrary
    mask shapes (a second texture stage) only if something needs them.
    Clipping for free drawing: `ui.push_clip`/`ui.pop_clip`/`ui.clip`
    (2026-11-21). First use: Altana's quest tracker (`native/rows.lua`),
    which scrolls by the pixel and glides. Then the info window's body, which
    shows the quest items list and quest/mission pages
    (`native/textview.lua`). Then the minimap's map as one round image
    (`native/minimap.lua`, drawn under the Windower 4 images through a new
    compat underlay hook). Offline-tested; need in-game test.
11. **Altana.** Last, once 10 is done.
12. **Windower 4 compatibility.** Offline survey (2026-11-21) of the 27
    Windower 4 add-ons installed: with a scratch copy of Altana's layer plus
    fixes, 26 load and run 300 frames with no errors, including GearSwap,
    Skillchains, Omen, MobFinder, Outfitter, Azure. InfoBar needs sqlite3
    (a C library; not provided). Fixes made in the scratch copy, to carry
    into `w4compat`: event handlers and scheduled functions can sleep (own
    scheduler); `...:method()` syntax; `loadstring`/`load` get the syntax
    fix and the add-on's globals; `../libs/` paths (logger.xml); the client
    version (`memory.account_info.version`); `get_items('equipment')` and
    empty slots; `get_mjob_data`/`get_sjob_data` from packet 0x044 (BLU,
    PUP, MON); `get_bag_info`; events `incoming text`, `action message`,
    `job change`, `level up/down`, `target change`, `time/day/weather
    change`, `gain/lose buff`, `hp/mp/tp change`; sending (`inject_outgoing`,
    `set_equip`, `cancel_buff`, `get_item`/`put_item`) and blocking or
    changing outgoing packets, for Windower 4 add-ons only; `play_sound`,
    `copy_to_clipboard`; `socket.gettime` and Base64 `mime` stand-ins.
    Still missing: `outgoing text` (needs a core hook on typed commands;
    GearSwap uses it), `add item`/`remove item`, recast timers (read as
    ready; need memory signatures), learned abilities, merits/job points.
    Not yet tried in game.
