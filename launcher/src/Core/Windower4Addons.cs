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
    /// A Windower 4 add-on found on disk, run in game by the w4compat package.
    /// </summary>
    public sealed class Windower4Addon
    {
        public Windower4Addon(string name, string path, string author, string version,
            IReadOnlyList<string> commands)
        {
            Name = name;
            Path = path;
            Author = author;
            Version = version;
            Commands = commands;
        }

        public string Name { get; }

        public string Path { get; }

        public string Author { get; }

        public string Version { get; }

        public IReadOnlyList<string> Commands { get; }
    }

    /// <summary>
    /// The Windower 4 add-ons w4compat can load, and which of them it loads when the game starts. Mirrors
    /// w4compat's own w4\host.lua: the folders it looks in (folders.txt in its user folder, or the same
    /// defaults), an add-on being a folder in a folder's addons\ holding a main file of the same name, and
    /// the startup list in autoload.txt beside folders.txt.
    /// </summary>
    public class Windower4Addons
    {
        public const string ServiceName = "w4compat";

        private static readonly string[] DefaultInstalls =
        {
            @"C:\Program Files (x86)\Windower", @"C:\Program Files (x86)\Windower4", @"C:\Windower",
            @"C:\Windower4",
        };

        private static readonly Regex CommandList = new Regex(@"_addon\.commands\s*=\s*\{([^}]*)\}",
            RegexOptions.CultureInvariant);

        private static readonly Regex Quoted = new Regex(@"(['""])(.*?)\1", RegexOptions.CultureInvariant);

        private readonly string userPath;
        private readonly string servicePath;

        public Windower4Addons(string userPath)
        {
            this.userPath = userPath ?? throw new ArgumentNullException(nameof(userPath));
            servicePath = System.IO.Path.Combine(userPath, "addons", ServiceName);
        }

        public string AutoloadPath => System.IO.Path.Combine(servicePath, "autoload.txt");

        /// <summary>
        /// The Windower 4 folders, first to last.
        /// </summary>
        public IReadOnlyList<string> Folders()
        {
            var file = System.IO.Path.Combine(servicePath, "folders.txt");
            if (File.Exists(file))
            {
                return File.ReadAllLines(file)
                    .Select(l => l.Trim())
                    .Where(l => l.Length > 0 && !l.StartsWith("#", StringComparison.Ordinal))
                    .ToList();
            }

            // As w4compat makes folders.txt on its first start.
            var folders = new List<string> { System.IO.Path.Combine(userPath, "windower4") };
            folders.AddRange(DefaultInstalls.Where(p => Directory.Exists(System.IO.Path.Combine(p, "addons"))));
            return folders;
        }

        /// <summary>
        /// Every add-on in the folders (the first of each name, ignoring case), sorted by name.
        /// </summary>
        public IReadOnlyList<Windower4Addon> Find()
        {
            var result = new Dictionary<string, Windower4Addon>(StringComparer.OrdinalIgnoreCase);
            foreach (var folder in Folders())
            {
                var addons = System.IO.Path.Combine(folder, "addons");
                IEnumerable<string> directories;
                try
                {
                    directories = Directory.Exists(addons) ? Directory.GetDirectories(addons) : new string[0];
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
                {
                    continue;
                }

                foreach (var directory in directories)
                {
                    var name = System.IO.Path.GetFileName(directory);
                    if (string.Equals(name, "libs", StringComparison.OrdinalIgnoreCase) || result.ContainsKey(name))
                    {
                        continue;
                    }
                    var main = System.IO.Path.Combine(directory, name + ".lua");
                    if (File.Exists(main))
                    {
                        result[name] = Describe(name, directory, main);
                    }
                }
            }
            return result.Values.OrderBy(a => a.Name, StringComparer.OrdinalIgnoreCase).ToList();
        }

        /// <summary>
        /// The add-ons loaded at startup, in order.
        /// </summary>
        public IReadOnlyList<string> LoadAutoload()
        {
            if (!File.Exists(AutoloadPath))
            {
                return new string[0];
            }
            var names = new List<string>();
            foreach (var line in File.ReadAllLines(AutoloadPath).Select(l => l.Trim()))
            {
                if (line.Length > 0 && !line.StartsWith("#", StringComparison.Ordinal) &&
                    !names.Contains(line, StringComparer.OrdinalIgnoreCase))
                {
                    names.Add(line);
                }
            }
            return names;
        }

        /// <summary>
        /// Replaces the startup list.
        /// </summary>
        public void SaveAutoload(IEnumerable<string> names)
        {
            if (names == null)
            {
                throw new ArgumentNullException(nameof(names));
            }

            var output = new StringBuilder();
            output.Append("# Windower 4 add-ons loaded each time the game starts, in order, before init.txt runs.\n");
            output.Append("# One name a line. The launcher's package page and /pkgbrowser tick these too.\n");
            foreach (var name in names.Distinct(StringComparer.OrdinalIgnoreCase))
            {
                output.Append(name).Append('\n');
            }
            Directory.CreateDirectory(servicePath);
            File.WriteAllText(AutoloadPath, output.ToString(), new UTF8Encoding(false));
        }

        // What the add-on says about itself in its main file, read as text (w4compat's describe).
        private static Windower4Addon Describe(string name, string directory, string main)
        {
            string text;
            try
            {
                using (var reader = new StreamReader(main, Encoding.UTF8))
                {
                    var buffer = new char[256 * 1024];
                    text = new string(buffer, 0, reader.ReadBlock(buffer, 0, buffer.Length));
                }
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
            {
                return new Windower4Addon(name, directory, null, null, new string[0]);
            }

            string Field(string field)
            {
                var match = Regex.Match(text, @"_addon\." + field + @"\s*=\s*(['""])(.*?)\1",
                    RegexOptions.CultureInvariant);
                return match.Success ? match.Groups[2].Value : null;
            }

            var version = Field("version");
            if (version == null)
            {
                var number = Regex.Match(text, @"_addon\.version\s*=\s*([\d.]+)", RegexOptions.CultureInvariant);
                version = number.Success ? number.Groups[1].Value : null;
            }

            var commands = new List<string>();
            var list = CommandList.Match(text);
            if (list.Success)
            {
                commands.AddRange(Quoted.Matches(list.Groups[1].Value).Cast<Match>().Select(m => m.Groups[2].Value));
            }
            if (commands.Count == 0 && Field("command") is string command)
            {
                commands.Add(command);
            }

            return new Windower4Addon(name, directory, Field("author"), version, commands);
        }
    }
}
