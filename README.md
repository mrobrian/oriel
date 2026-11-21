# Oriel

Oriel is an addon platform for Final Fantasy XI, forked from
[Fenestra][fenestra], the open-source core of Windower 5.

It focuses on the in-game UI: windows that clip their contents, smooth
scrolling, masked images, and correct layering of text and graphics.

> **Status:** early development. Oriel is not ready for general use and has no
> releases yet.

## Relationship to Fenestra and Windower

Oriel builds on Fenestra's code, which the Windower Dev Team publishes under
the MIT license. Oriel is an independent project. It is not affiliated with or
endorsed by the Windower Dev Team, and it does not use the Windower name or
artwork.

Upstream changes from Fenestra are merged in periodically.

## Building

Requirements:

- Visual Studio 2026 with the **Desktop development with C++** and
  **.NET desktop development** workloads
- Git

```
git clone --recurse-submodules https://github.com/mrobrian/oriel.git
cd oriel
msbuild fenestra.sln -t:restore;build -p:configuration=debug
```

You can also open `fenestra.sln` in Visual Studio and build from there. The
output is written to `build\bin\debug`.

## Contributing

Changes to `main` go through pull requests, which the maintainer reviews
before merging.

## License

Oriel is released under the [MIT license][license], the same license as
Fenestra. The original copyright notice for the Windower Dev Team is kept in
the license file and in the application, as the license requires.

FINAL FANTASY is a registered trademark of Square Enix Holdings Co., Ltd.

[fenestra]: https://github.com/Windower/Fenestra
[license]: ./LICENSE.md
