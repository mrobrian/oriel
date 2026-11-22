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
    using System.IO;
    using System.Linq;
    using System.Text;
    using System.Text.RegularExpressions;

    /// <summary>
    /// The packages loaded when the game starts. The core runs scripts\init.lua from the user folder at startup;
    /// the launcher keeps its list in a marked block in that file and leaves the rest of the file alone.
    /// </summary>
    public class StartupPackages
    {
        private const string BeginMarker = "-- BEGIN Oriel startup packages (managed by the launcher)";
        private const string EndMarker = "-- END Oriel startup packages";

        private static readonly Regex LoadLine = new Regex(
            @"^\s*(?:require\s*\(?\s*['""]core\.command['""]\s*\)?|command)\s*\.\s*input\s*\(\s*['""]/load\s+([A-Za-z0-9_]+)\s*['""]\s*\)\s*;?\s*$",
            RegexOptions.CultureInvariant);

        private static readonly Regex CommandLocal = new Regex(
            @"^\s*local\s+command\s*=\s*require\s*\(?\s*['""]core\.command['""]\s*\)?\s*;?\s*$",
            RegexOptions.CultureInvariant);

        private readonly string path;

        public StartupPackages(string userPath)
        {
            if (userPath == null)
            {
                throw new ArgumentNullException(nameof(userPath));
            }
            path = Path.Combine(userPath, "scripts", "init.lua");
        }

        /// <summary>
        /// The packages loaded at startup, in order: those in the managed block plus any plain
        /// "command.input('/load name')" lines elsewhere in the file.
        /// </summary>
        public IReadOnlyList<string> Load()
        {
            var names = new List<string>();
            foreach (var line in ReadLines())
            {
                var match = LoadLine.Match(line);
                if (match.Success && !names.Contains(match.Groups[1].Value))
                {
                    names.Add(match.Groups[1].Value);
                }
            }
            return names;
        }

        /// <summary>
        /// Replaces the startup list. Plain "/load" lines outside the managed block are moved into it.
        /// </summary>
        public void Save(IEnumerable<string> names)
        {
            if (names == null)
            {
                throw new ArgumentNullException(nameof(names));
            }

            var kept = new List<string>();
            var inBlock = false;
            foreach (var line in ReadLines())
            {
                if (line.Trim() == BeginMarker)
                {
                    inBlock = true;
                }
                else if (line.Trim() == EndMarker)
                {
                    inBlock = false;
                }
                else if (!inBlock && !LoadLine.IsMatch(line))
                {
                    kept.Add(line);
                }
            }

            // The hand-editing hint from the original init.lua no longer applies.
            kept.RemoveAll(l => l.Trim() == "-- Add a line for each package you want loaded automatically.");

            // Drop a "local command = require('core.command')" that nothing else uses any more.
            if (!kept.Any(l => !CommandLocal.IsMatch(l) && Regex.IsMatch(l, @"\bcommand\b")))
            {
                kept.RemoveAll(l => CommandLocal.IsMatch(l));
            }

            while (kept.Count > 0 && kept[kept.Count - 1].Trim().Length == 0)
            {
                kept.RemoveAt(kept.Count - 1);
            }

            if (kept.Count == 0)
            {
                kept.Add("-- Oriel runs this script once each time the game starts.");
            }

            var output = new StringBuilder();
            foreach (var line in kept)
            {
                output.Append(line).Append("\r\n");
            }
            output.Append("\r\n").Append(BeginMarker).Append("\r\n");
            foreach (var name in names.Distinct())
            {
                output.Append("require('core.command').input('/load ").Append(name).Append("')\r\n");
            }
            output.Append(EndMarker).Append("\r\n");

            Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path, output.ToString(), new UTF8Encoding(false));
        }

        private IEnumerable<string> ReadLines() =>
            File.Exists(path) ? File.ReadAllLines(path) : Enumerable.Empty<string>();
    }
}
