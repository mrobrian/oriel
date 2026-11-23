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

namespace Windower.UI.Views
{
    using Core;
    using System;
    using System.Collections.Generic;
    using System.Collections.ObjectModel;
    using System.ComponentModel;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Linq;
    using System.Threading;
    using System.Windows.Data;
    using System.Windows.Input;

    /// <summary>
    /// One row in the package list.
    /// </summary>
    public class PackageItemViewModel : ViewModelBase
    {
        private readonly PackagesViewModel owner;
        private string installedVersion;
        private bool loadAtStartup;

        public PackageItemViewModel(PackagesViewModel owner, string name, string type, string availableVersion,
            string installedVersion, bool isLocal, bool loadAtStartup)
        {
            this.owner = owner;
            Name = name;
            Type = type;
            AvailableVersion = availableVersion;
            this.installedVersion = installedVersion;
            IsLocal = isLocal;
            this.loadAtStartup = loadAtStartup;
            Description = PackageDescriptions.Get(name, type);
        }

        /// <summary>
        /// A Windower 4 add-on on disk, which w4compat loads. It can't be installed, updated or removed here.
        /// </summary>
        public PackageItemViewModel(PackagesViewModel owner, Windower4Addon addon, bool loadAtStartup)
            : this(owner, addon?.Name, "addon", null, addon?.Version ?? string.Empty, true, loadAtStartup)
        {
            IsWindower4 = true;
            var description = new System.Text.StringBuilder("Windower 4 add-on");
            if (!string.IsNullOrEmpty(addon.Author))
            {
                description.Append(" by ").Append(addon.Author);
            }
            if (addon.Commands.Count > 0)
            {
                description.Append(addon.Commands.Count > 1 ? ". Commands: " : ". Command: ")
                    .Append(string.Join(", ", addon.Commands.Select(c => "/" + c)));
            }
            Description = description.Append(". In ").Append(addon.Path).Append('.').ToString();
        }

        public string Name { get; }

        public bool IsWindower4 { get; }

        public string LoadAtStartupToolTip => IsWindower4
            ? "Load this Windower 4 add-on each time the game starts (saved in autoload.txt in w4compat's user folder)."
            : "Load this add-on automatically each time the game starts (saved in scripts\\init.lua in your user folder).";

        public string Type { get; }

        public string Description { get; private set; }

        public bool HasDescription => !string.IsNullOrEmpty(Description);

        public string AvailableVersion { get; }

        public bool IsLocal { get; }

        public bool IsAddon => Type == "addon";

        public bool IsLibrary => !IsAddon;

        public string InstalledVersion
        {
            get => installedVersion;
            set
            {
                Set(ref installedVersion, value);
                OnPropertyChanged(nameof(IsInstalled));
                OnPropertyChanged(nameof(IsNotInstalled));
                OnPropertyChanged(nameof(HasUpdate));
                OnPropertyChanged(nameof(StatusText));
                OnPropertyChanged(nameof(CanLoadAtStartup));
            }
        }

        public bool IsInstalled => installedVersion != null;

        public bool IsNotInstalled => installedVersion == null;

        public bool HasUpdate => IsInstalled && !IsLocal && AvailableVersion != null &&
            PackageCatalog.CompareVersions(installedVersion, AvailableVersion) < 0;

        public bool CanLoadAtStartup => IsInstalled && IsAddon;

        public bool CanRemove => IsInstalled && !IsLocal;

        public string StatusText
        {
            get
            {
                if (IsWindower4)
                {
                    return string.IsNullOrEmpty(installedVersion)
                        ? "Windower 4"
                        : string.Format(CultureInfo.CurrentCulture, "Windower 4 · {0}", installedVersion);
                }
                if (IsLocal)
                {
                    return string.Format(CultureInfo.CurrentCulture, "Local copy {0}", installedVersion);
                }
                if (HasUpdate)
                {
                    return string.Format(CultureInfo.CurrentCulture, "{0} → {1}", installedVersion, AvailableVersion);
                }
                if (IsInstalled)
                {
                    return string.Format(CultureInfo.CurrentCulture, "Installed {0}", installedVersion);
                }
                return AvailableVersion;
            }
        }

        public bool LoadAtStartup
        {
            get => loadAtStartup;
            set
            {
                if (loadAtStartup != value)
                {
                    Set(ref loadAtStartup, value);
                    if (IsWindower4)
                    {
                        owner.SaveWindower4StartupList();
                    }
                    else
                    {
                        owner.SaveStartupList();
                    }
                }
            }
        }

        internal void SetLoadAtStartupSilently(bool value)
        {
            loadAtStartup = value;
            OnPropertyChanged(nameof(LoadAtStartup));
        }
    }

    /// <summary>
    /// Browse, install, update and remove packages, and choose which add-ons load when the game starts.
    /// </summary>
    public class PackagesViewModel : ViewModelBase
    {
        private readonly INavigationService navigation;
        private readonly PackageCatalog catalog;
        private readonly StartupPackages startup;
        private readonly Windower4Addons windower4;
        private IReadOnlyDictionary<string, AvailablePackage> available =
            new Dictionary<string, AvailablePackage>();
        private string searchText = string.Empty;
        private bool showLibraries;
        private bool showWindower4 = true;
        private bool hasWindower4;
        private bool isBusy;
        private string status;

        public PackagesViewModel(INavigationService navigation, string settingsPath, string userPath)
        {
            this.navigation = navigation ?? throw new ArgumentNullException(nameof(navigation));
            catalog = new PackageCatalog(settingsPath);
            startup = new StartupPackages(userPath);
            windower4 = new Windower4Addons(userPath);

            PackagesView = CollectionViewSource.GetDefaultView(Packages);
            PackagesView.Filter = Filter;
            // Oriel's packages, then the Windower 4 add-ons.
            PackagesView.SortDescriptions.Add(new SortDescription(nameof(PackageItemViewModel.IsWindower4),
                ListSortDirection.Ascending));
            PackagesView.SortDescriptions.Add(new SortDescription(nameof(PackageItemViewModel.Name),
                ListSortDirection.Ascending));

            Close = new DelegateCommand(_ => navigation.Close());
            Install = new DelegateCommand(ExecuteInstall, _ => !IsBusy).ObservesProperty(this, nameof(IsBusy));
            Remove = new DelegateCommand(ExecuteRemove, _ => !IsBusy).ObservesProperty(this, nameof(IsBusy));
            UpdateAll = new DelegateCommand(ExecuteUpdateAll, _ => !IsBusy && Packages.Any(p => p.HasUpdate))
                .ObservesProperty(this, nameof(IsBusy));
            Refresh = new DelegateCommand(_ => LoadAsync(), _ => !IsBusy).ObservesProperty(this, nameof(IsBusy));
        }

        public ObservableCollection<PackageItemViewModel> Packages { get; } =
            new ObservableCollection<PackageItemViewModel>();

        public ICollectionView PackagesView { get; }

        public ICommand Close { get; }

        public ICommand Install { get; }

        public ICommand Remove { get; }

        public ICommand UpdateAll { get; }

        public ICommand Refresh { get; }

        public string SearchText
        {
            get => searchText;
            set
            {
                Set(ref searchText, value ?? string.Empty);
                PackagesView.Refresh();
            }
        }

        public bool ShowLibraries
        {
            get => showLibraries;
            set
            {
                Set(ref showLibraries, value);
                PackagesView.Refresh();
            }
        }

        public bool ShowWindower4
        {
            get => showWindower4;
            set
            {
                Set(ref showWindower4, value);
                PackagesView.Refresh();
            }
        }

        /// <summary>
        /// Whether w4compat is installed, so Windower 4 add-ons are listed.
        /// </summary>
        public bool HasWindower4
        {
            get => hasWindower4;
            private set => Set(ref hasWindower4, value);
        }

        public bool IsBusy
        {
            get => isBusy;
            private set => Set(ref isBusy, value);
        }

        public string Status
        {
            get => status;
            private set
            {
                Set(ref status, value);
                OnPropertyChanged(nameof(HasStatus));
            }
        }

        public bool HasStatus => !string.IsNullOrEmpty(status);

        public static bool IsGameRunning => Process.GetProcessesByName("pol").Length > 0;

        public override void Opened() => LoadAsync();

        internal void SaveStartupList()
        {
            try
            {
                // Keep the existing order; add newly ticked packages at the end.
                var ticked = Packages.Where(p => !p.IsWindower4 && p.LoadAtStartup).ToList();
                var current = startup.Load().Where(n => ticked.Any(p => p.Name == n)).ToList();
                current.AddRange(ticked.Where(p => !current.Contains(p.Name))
                    .Select(p => p.Name).OrderBy(n => n, StringComparer.Ordinal));
                startup.Save(current);
                Status = IsGameRunning
                    ? "Saved. Takes effect the next time the game starts."
                    : null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
            {
                Status = "Could not save the startup list: " + e.Message;
            }
        }

        internal void SaveWindower4StartupList()
        {
            try
            {
                // Keep the existing order (names not found on disk too, as w4compat does); add newly ticked
                // add-ons at the end.
                var rows = Packages.Where(p => p.IsWindower4).ToList();
                var current = windower4.LoadAutoload()
                    .Where(n => !rows.Any(p => string.Equals(p.Name, n, StringComparison.OrdinalIgnoreCase)) ||
                        rows.Any(p => p.LoadAtStartup && string.Equals(p.Name, n, StringComparison.OrdinalIgnoreCase)))
                    .ToList();
                current.AddRange(rows.Where(p => p.LoadAtStartup &&
                        !current.Contains(p.Name, StringComparer.OrdinalIgnoreCase))
                    .Select(p => p.Name).OrderBy(n => n, StringComparer.OrdinalIgnoreCase));
                windower4.SaveAutoload(current);
                Status = IsGameRunning
                    ? "Saved. Takes effect the next time the game starts; in game, /lua load <name> loads it now."
                    : null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
            {
                Status = "Could not save the Windower 4 startup list: " + e.Message;
            }
        }

        private bool Filter(object item)
        {
            if (!(item is PackageItemViewModel package))
            {
                return false;
            }

            if (!showLibraries && package.IsLibrary)
            {
                return false;
            }

            if (!showWindower4 && package.IsWindower4)
            {
                return false;
            }

            var text = searchText.Trim();
            return text.Length == 0 ||
                package.Name.IndexOf(text, StringComparison.OrdinalIgnoreCase) >= 0 ||
                (package.Description?.IndexOf(text, StringComparison.OrdinalIgnoreCase) ?? -1) >= 0;
        }

        private async void LoadAsync()
        {
            IsBusy = true;
            Status = "Loading package list...";
            try
            {
                available = await catalog.LoadAvailableAsync(CancellationToken.None);
                RebuildList();
                Status = available.Count == 0
                    ? "Could not reach the package servers. Showing installed packages only."
                    : null;
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException ||
                e is InvalidOperationException)
            {
                Status = "Could not load packages: " + e.Message;
            }
            finally
            {
                IsBusy = false;
            }
        }

        private void RebuildList()
        {
            var installed = catalog.LoadInstalled();
            var atStartup = new HashSet<string>(startup.Load(), StringComparer.Ordinal);

            Packages.Clear();
            foreach (var name in available.Keys.Union(installed.Keys))
            {
                available.TryGetValue(name, out var offered);
                installed.TryGetValue(name, out var local);
                Packages.Add(new PackageItemViewModel(this, name, local?.Type ?? offered?.Type ?? "addon",
                    offered?.Version, local?.Version, local?.IsLocal ?? false, atStartup.Contains(name)));
            }

            // Windower 4 add-ons on disk, when w4compat is there to run them.
            HasWindower4 = installed.ContainsKey(Windower4Addons.ServiceName);
            if (HasWindower4)
            {
                try
                {
                    var autoload = new HashSet<string>(windower4.LoadAutoload(), StringComparer.OrdinalIgnoreCase);
                    foreach (var addon in windower4.Find())
                    {
                        Packages.Add(new PackageItemViewModel(this, addon, autoload.Contains(addon.Name)));
                    }
                }
                catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
                {
                    Status = "Could not list Windower 4 add-ons: " + e.Message;
                }
            }
            PackagesView.Refresh();
            (UpdateAll as DelegateCommand)?.RaiseCanExecuteChanged();
        }

        private async void ExecuteInstall(object arg)
        {
            if (!(arg is PackageItemViewModel package))
            {
                return;
            }

            await InstallAsync(new[] { package.Name }, package.IsAddon && package.IsNotInstalled ? package : null);
        }

        private async void ExecuteUpdateAll(object arg) =>
            await InstallAsync(Packages.Where(p => p.HasUpdate).Select(p => p.Name).ToList(), null);

        private async System.Threading.Tasks.Task InstallAsync(IReadOnlyList<string> names,
            PackageItemViewModel newAddon)
        {
            IsBusy = true;
            try
            {
                var progress = new Progress<string>(name => Status = "Installing " + name + "...");
                var changed = await catalog.InstallAsync(names, available, progress, CancellationToken.None);
                RebuildList();

                // A newly installed add-on loads at startup by default; untick it to opt out.
                if (newAddon != null)
                {
                    var item = Packages.FirstOrDefault(p => !p.IsWindower4 && p.Name == newAddon.Name);
                    if (item != null && !item.LoadAtStartup)
                    {
                        item.SetLoadAtStartupSilently(true);
                        SaveStartupList();
                    }
                }

                var summary = changed.Count == 0
                    ? "Everything is up to date."
                    : "Installed: " + string.Join(", ", changed) + ".";
                Status = IsGameRunning
                    ? summary + " The game is running: type /pkg reload in game, then /load <name>."
                    : summary;
            }
            catch (Exception e) when (e is System.Net.WebException || e is IOException ||
                e is UnauthorizedAccessException || e is InvalidOperationException ||
                e is InvalidDataException)
            {
                Status = "Install failed: " + e.Message;
                RebuildList();
            }
            finally
            {
                IsBusy = false;
                (UpdateAll as DelegateCommand)?.RaiseCanExecuteChanged();
            }
        }

        private void ExecuteRemove(object arg)
        {
            if (!(arg is PackageItemViewModel package))
            {
                return;
            }

            var dependents = catalog.Dependents(package.Name);
            if (dependents.Count > 0)
            {
                Status = string.Format(CultureInfo.CurrentCulture, "{0} is used by {1}. Remove those first.",
                    package.Name, string.Join(", ", dependents));
                return;
            }

            try
            {
                if (package.LoadAtStartup)
                {
                    package.SetLoadAtStartupSilently(false);
                    SaveStartupList();
                }
                catalog.Uninstall(package.Name);
                RebuildList();
                Status = IsGameRunning
                    ? "Removed " + package.Name + ". If it's loaded in game, /unload it first, then /pkg reload."
                    : "Removed " + package.Name + ".";
            }
            catch (Exception e) when (e is IOException || e is UnauthorizedAccessException)
            {
                Status = "Could not remove " + package.Name + ": " + e.Message;
            }
        }
    }
}
