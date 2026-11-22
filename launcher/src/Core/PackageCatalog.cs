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
    using System.Globalization;
    using System.IO;
    using System.Linq;
    using System.Net;
    using System.Threading;
    using System.Threading.Tasks;
    using System.Xml.Linq;

    /// <summary>
    /// A package offered by a package source.
    /// </summary>
    public sealed class AvailablePackage
    {
        public AvailablePackage(string name, string version, string type, IReadOnlyList<string> dependencies,
            IReadOnlyList<string> files, string rootUrl)
        {
            Name = name;
            Version = version;
            Type = type;
            Dependencies = dependencies;
            Files = files;
            RootUrl = rootUrl;
        }

        public string Name { get; }

        public string Version { get; }

        public string Type { get; }

        public IReadOnlyList<string> Dependencies { get; }

        public IReadOnlyList<string> Files { get; }

        public string RootUrl { get; }
    }

    /// <summary>
    /// A package installed in the settings folder (or a local override folder).
    /// </summary>
    public sealed class InstalledPackage
    {
        public InstalledPackage(string name, string version, string type, IReadOnlyList<string> dependencies,
            string path, bool isLocal)
        {
            Name = name;
            Version = version;
            Type = type;
            Dependencies = dependencies;
            Path = path;
            IsLocal = isLocal;
        }

        public string Name { get; }

        public string Version { get; }

        public string Type { get; }

        public IReadOnlyList<string> Dependencies { get; }

        public string Path { get; }

        /// <summary>
        /// True for packages in a development override folder (overrides.xml); those are never updated or
        /// removed by the launcher.
        /// </summary>
        public bool IsLocal { get; }
    }

    /// <summary>
    /// Reads package sources and installs, updates and removes packages, using the same folders and formats as
    /// the core's package manager, so either one can manage the same installation.
    /// </summary>
    public class PackageCatalog
    {
        // Same built-in sources (and cache names) as the core's package_manager.
        private static readonly (string Url, string Guid)[] BuiltInSources =
        {
            ("https://files.windower.net/5/packages", "D103E5FA-AD95-4407-A1E3-2D3E83404E63"),
            ("https://windower.github.io/Resources", "0403E868-52FE-4329-A6D5-5AC9745C57B7"),
        };

        private readonly string settingsPath;

        public PackageCatalog(string settingsPath)
        {
            this.settingsPath = settingsPath ?? throw new ArgumentNullException(nameof(settingsPath));
        }

        public string PackagesPath => Path.Combine(settingsPath, "packages");

        private string UpdatesPath => Path.Combine(settingsPath, "updates");

        /// <summary>
        /// Downloads every source's package list (falling back to the copy the core cached last time) and
        /// returns the packages they offer. The first source to offer a name wins, as in the core.
        /// </summary>
        public async Task<IReadOnlyDictionary<string, AvailablePackage>> LoadAvailableAsync(
            CancellationToken token)
        {
            var result = new Dictionary<string, AvailablePackage>(StringComparer.Ordinal);
            foreach (var (url, guid) in GetSources())
            {
                token.ThrowIfCancellationRequested();
                XDocument document = null;
                try
                {
                    var text = await DownloadStringAsync(CombineUrl(url, "packages.xml"), token)
                        .ConfigureAwait(false);
                    document = XDocument.Parse(text);
                }
                catch (Exception e) when (e is WebException || e is System.Xml.XmlException || e is IOException)
                {
                    var cached = Path.Combine(UpdatesPath, guid);
                    if (File.Exists(cached))
                    {
                        try
                        {
                            document = XDocument.Load(cached);
                        }
                        catch (System.Xml.XmlException) { }
                    }
                }

                if (document?.Root == null)
                {
                    continue;
                }

                foreach (var element in document.Root.Elements("package"))
                {
                    var name = (string)element.Element("name");
                    if (string.IsNullOrEmpty(name) || result.ContainsKey(name))
                    {
                        continue;
                    }

                    var dependencies = element.Element("dependencies")?.Elements("dependency")
                        .Select(d => d.Value.Trim()).Where(d => d.Length > 0).ToList() ?? new List<string>();
                    var files = element.Element("files")?.Elements("file")
                        .Select(f => f.Value.Trim()).Where(f => f.Length > 0).ToList() ?? new List<string>();
                    var rootUrl = (string)element.Element("url") ?? url;

                    result.Add(name, new AvailablePackage(name, (string)element.Element("version") ?? "0",
                        (string)element.Element("type") ?? "addon", dependencies, files, rootUrl));
                }
            }
            return result;
        }

        /// <summary>
        /// Returns the installed packages. Local override folders come first and take precedence, as in the core.
        /// </summary>
        public IReadOnlyDictionary<string, InstalledPackage> LoadInstalled()
        {
            var result = new Dictionary<string, InstalledPackage>(StringComparer.Ordinal);
            foreach (var directory in GetOverrideDirectories())
            {
                AddInstalled(result, directory, true);
            }
            AddInstalled(result, PackagesPath, false);
            return result;
        }

        /// <summary>
        /// Installs or updates the named packages and any missing or outdated dependencies. Returns the names of
        /// the packages that were installed or updated.
        /// </summary>
        public async Task<IReadOnlyList<string>> InstallAsync(IEnumerable<string> names,
            IReadOnlyDictionary<string, AvailablePackage> available, IProgress<string> progress,
            CancellationToken token)
        {
            if (names == null)
            {
                throw new ArgumentNullException(nameof(names));
            }

            if (available == null)
            {
                throw new ArgumentNullException(nameof(available));
            }

            var installed = LoadInstalled();
            var plan = new List<AvailablePackage>();
            var visited = new HashSet<string>(StringComparer.Ordinal);
            var pending = new Stack<string>(names.Reverse());
            while (pending.Count > 0)
            {
                var name = pending.Pop();
                if (!visited.Add(name))
                {
                    continue;
                }

                if (!available.TryGetValue(name, out var package))
                {
                    if (!installed.ContainsKey(name))
                    {
                        throw new InvalidOperationException(string.Format(CultureInfo.CurrentCulture,
                            "Could not find the package or dependency \"{0}\".", name));
                    }
                    continue;
                }

                if (!installed.TryGetValue(name, out var current) ||
                    !current.IsLocal && CompareVersions(current.Version, package.Version) < 0)
                {
                    plan.Add(package);
                }

                foreach (var dependency in package.Dependencies)
                {
                    pending.Push(dependency);
                }
            }

            foreach (var package in plan)
            {
                token.ThrowIfCancellationRequested();
                progress?.Report(package.Name);
                await InstallPackageAsync(package, token).ConfigureAwait(false);
            }

            return plan.Select(p => p.Name).ToList();
        }

        /// <summary>
        /// The installed packages that depend on the named package.
        /// </summary>
        public IReadOnlyList<string> Dependents(string name) =>
            LoadInstalled().Values.Where(p => p.Dependencies.Contains(name)).Select(p => p.Name).ToList();

        /// <summary>
        /// Removes an installed package. Local override packages are never removed.
        /// </summary>
        public void Uninstall(string name)
        {
            if (LoadInstalled().TryGetValue(name, out var package) && !package.IsLocal)
            {
                Directory.Delete(package.Path, true);
            }
        }

        /// <summary>
        /// Compares dotted version strings numerically (e.g. "5.0.9720.12082.5"); missing parts count as 0.
        /// </summary>
        public static int CompareVersions(string a, string b)
        {
            var left = ParseVersion(a);
            var right = ParseVersion(b);
            for (var i = 0; i < Math.Max(left.Count, right.Count); i++)
            {
                var l = i < left.Count ? left[i] : 0;
                var r = i < right.Count ? right[i] : 0;
                if (l != r)
                {
                    return l.CompareTo(r);
                }
            }
            return 0;
        }

        private static List<long> ParseVersion(string version) =>
            (version ?? string.Empty).Split('.')
                .Select(part => long.TryParse(part, NumberStyles.None, CultureInfo.InvariantCulture, out var n) ? n : 0)
                .ToList();

        private static string CombineUrl(string root, string relative) =>
            root.TrimEnd('/') + "/" + relative.Replace('\\', '/').TrimStart('/');

        private static async Task<string> DownloadStringAsync(string url, CancellationToken token)
        {
            var request = WebRequest.CreateHttp(new Uri(url));
            request.UserAgent = "Oriel";
            using (token.Register(request.Abort))
            using (var response = await request.GetResponseAsync().ConfigureAwait(false))
            using (var reader = new StreamReader(response.GetResponseStream()))
            {
                return await reader.ReadToEndAsync().ConfigureAwait(false);
            }
        }

        private static async Task DownloadFileAsync(string url, string path, CancellationToken token)
        {
            var request = WebRequest.CreateHttp(new Uri(url));
            request.UserAgent = "Oriel";
            using (token.Register(request.Abort))
            using (var response = await request.GetResponseAsync().ConfigureAwait(false))
            using (var input = response.GetResponseStream())
            using (var output = File.Create(path))
            {
                await input.CopyToAsync(output, 81920, token).ConfigureAwait(false);
            }
        }

        private static List<string> ReadDependencies(XElement root) =>
            root?.Element("dependencies")?.Elements("dependency")
                .Select(d => d.Value.Trim()).Where(d => d.Length > 0).ToList() ?? new List<string>();

        private static void AddInstalled(Dictionary<string, InstalledPackage> result, string directory, bool isLocal)
        {
            if (!Directory.Exists(directory))
            {
                return;
            }

            foreach (var folder in Directory.EnumerateDirectories(directory))
            {
                var manifest = Path.Combine(folder, "manifest.xml");
                if (!File.Exists(manifest))
                {
                    continue;
                }

                try
                {
                    var root = XDocument.Load(manifest).Root;
                    var name = (string)root?.Element("name");
                    if (!string.IsNullOrEmpty(name) && !result.ContainsKey(name))
                    {
                        result.Add(name, new InstalledPackage(name, (string)root.Element("version") ?? "0",
                            (string)root.Element("type") ?? "addon", ReadDependencies(root), folder, isLocal));
                    }
                }
                catch (System.Xml.XmlException) { }
                catch (IOException) { }
            }
        }

        private IEnumerable<(string Url, string Guid)> GetSources()
        {
            foreach (var source in BuiltInSources)
            {
                yield return source;
            }

            var listPath = Path.Combine(UpdatesPath, "source_list");
            XDocument list = null;
            try
            {
                if (File.Exists(listPath))
                {
                    list = XDocument.Load(listPath);
                }
            }
            catch (System.Xml.XmlException) { }

            if (list?.Root != null)
            {
                foreach (var source in list.Root.Elements("source"))
                {
                    var url = (string)source.Attribute("url");
                    var guid = (string)source.Attribute("guid");
                    if (!string.IsNullOrEmpty(url) && !string.IsNullOrEmpty(guid))
                    {
                        yield return (url, guid);
                    }
                }
            }
        }

        private IEnumerable<string> GetOverrideDirectories()
        {
            var path = Path.Combine(settingsPath, "overrides.xml");
            if (!File.Exists(path))
            {
                return Enumerable.Empty<string>();
            }

            try
            {
                return XDocument.Load(path).Root?.Elements("path")
                    .Select(e => Environment.ExpandEnvironmentVariables(e.Value.Trim()))
                    .Where(p => p.Length > 0).ToList() ?? new List<string>();
            }
            catch (System.Xml.XmlException)
            {
                return Enumerable.Empty<string>();
            }
        }

        private async Task InstallPackageAsync(AvailablePackage package, CancellationToken token)
        {
            var packagesRoot = Path.GetFullPath(PackagesPath);
            var target = Path.GetFullPath(Path.Combine(packagesRoot, package.Name));
            var staging = Path.Combine(UpdatesPath, "launcher-staging", package.Name);

            // Every file must stay inside the package's own folder (package lists come from the network).
            var files = new List<(string Url, string Relative)>();
            foreach (var file in package.Files)
            {
                var relative = file.Replace('/', Path.DirectorySeparatorChar);
                var full = Path.GetFullPath(Path.Combine(packagesRoot, relative));
                if (!full.StartsWith(target + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
                {
                    throw new InvalidDataException(string.Format(CultureInfo.CurrentCulture,
                        "The package \"{0}\" lists a file outside its own folder: {1}", package.Name, file));
                }
                files.Add((CombineUrl(package.RootUrl, file), full.Substring(target.Length + 1)));
            }

            if (Directory.Exists(staging))
            {
                Directory.Delete(staging, true);
            }

            try
            {
                Directory.CreateDirectory(staging);
                foreach (var (url, relative) in files)
                {
                    var path = Path.Combine(staging, relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    await DownloadFileAsync(url, path, token).ConfigureAwait(false);
                }

                // All files downloaded: replace the installed copy.
                if (Directory.Exists(target))
                {
                    Directory.Delete(target, true);
                }
                Directory.CreateDirectory(packagesRoot);
                Directory.Move(staging, target);
            }
            finally
            {
                if (Directory.Exists(staging))
                {
                    Directory.Delete(staging, true);
                }
            }
        }
    }
}
