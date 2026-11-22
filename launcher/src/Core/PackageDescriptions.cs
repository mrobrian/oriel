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

namespace Windower.Core
{
    using System;
    using System.Collections.Generic;

    /// <summary>
    /// Short descriptions for packages. Package lists and manifests carry no descriptions, so these are written
    /// by hand from each package's code.
    /// </summary>
    public static class PackageDescriptions
    {
        private static readonly Dictionary<string, string> Descriptions =
            new Dictionary<string, string>(StringComparer.Ordinal)
            {
                ["burdometer"] = "Puppetmaster: shows your automaton's overload burden for each element and how fast it is cooling down.",
                ["config"] = "Game settings Windower 4's Config plugin covered: 60 fps, gamma, draw distance, aspect ratio, auto-disconnect and the language filter.",
                ["delay_me_not"] = "Windower 5 version of the DelayMeNot plugin: patches out some of the game's built-in waiting delays.",
                ["distance"] = "Shows the distance to your target in a small box at the top of the screen.",
                ["dress_up"] = "Changes how characters look on your screen only (race, face and gear models). Command: /du.",
                ["enternity"] = "Skips the Enter press after each line of NPC dialogue.",
                ["fps"] = "Shows the current frame rate.",
                ["ime"] = "Enables Japanese text input (IME) in the chat line.",
                ["logger"] = "Saves the chat log to a file per character and day in your user folder.",
                ["party_time"] = "Party invites: /pt invite and /pt request, with whitelist and blacklist for automatic answers.",
                ["paste"] = "/paste inserts the clipboard text into the chat line.",
                ["pouches"] = "/pouches <item> [count] uses a stack of a usable item (pouches, boxes) one after another.",
                ["pupswap"] = "Puppetmaster: save and equip automaton attachment sets. Command: /ps.",
                ["send"] = "/send <name> <command> runs a command in another of your game windows on this PC.",
                ["shortcuts"] = "Lets you type spells, abilities and items without the full command syntax.",
                ["speedometer"] = "Shows your movement speed bonus near the bottom of the screen.",
                ["target_info"] = "Shows your target's ID, hex index and movement speed.",
                ["terminate"] = "/terminate closes the game immediately.",
                ["timestamp"] = "Adds the time to each chat line. Format and color are configurable.",
                ["window_title"] = "Sets the game window's title to your character name.",
            };

        /// <summary>
        /// The description for a package, or a generic one based on its type.
        /// </summary>
        public static string Get(string name, string type)
        {
            if (name != null && Descriptions.TryGetValue(name, out var description))
            {
                return description;
            }

            switch (type)
            {
            case "library":
            case "service":
                return "Used by other packages; installed automatically when needed.";
            default:
                return null;
            }
        }
    }
}
