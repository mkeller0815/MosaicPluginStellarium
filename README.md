# Mosaic Planner for Stellarium

A Stellarium plug-in for planning multi-panel astrophotography mosaics directly on the sky, similar to the Telescopius mosaic planner.
The panel size and rotation come from the **telescope, sensor and lens selected in the Oculars plug-in**. Change the equipment in Oculars and the mosaic updates.

## Features

- Mosaic overlay on the sky: columns × rows, horizontal and vertical overlap, position angle, numbered panels and a title with the total field size.
- Camera framing taken live from Oculars (telescope + sensor + lens), or entered manually (focal length and sensor size).
- Optional two-way rotation sync with the Oculars sensor rotation.
- Panels can be rotated to preserve alignment (Telescopius default) or all use the same rotation.
- Center the mosaic on the selected object or on the view center, or type RA/Dec.
- Edit mode: drag the mosaic to move it, Shift+drag to rotate it, click a panel to select it.
- A panel table with RA/Dec/PA for each panel. Double-click a panel to go to it. With the Oculars sensor frame on, you see that panel's exact framing.
- Export as **Telescopius CSV**, which N.I.N.A., ASIAIR, StellarMate and similar tools can import. You can also export a CSV in decimal degrees.
- Save and load plans as JSON. Loading a plan reselects the same Oculars equipment by name.
- Scriptable: every setting is a property or slot of the `MosaicPlanner` module (for example `MosaicPlanner.setColumns(3)`).

Conventions: coordinates are J2000. The position angle is the direction of the sensor's "up" axis (the short side for a landscape sensor), measured from north through east. This is the same convention as the Telescopius CSV. Oculars' rotation angle is the negative of this.

## Usage

- **Toolbar button** (bottom bar): left-click shows or hides the mosaic, right-click opens the Mosaic Planner window.
- **Keyboard:** `Ctrl+Alt+G` shows or hides the mosaic. A shortcut for the window can be assigned under *Configuration → Keyboard shortcuts → Mosaic Planner*.
- The window can also be opened from *Configuration → Plugins → Mosaic Planner → configure*.

See the **[user guide](docs/USAGE.md)** for a step-by-step walkthrough of every option, exporting to capture software, and scripting.

## Installation (macOS)

Requirements: Stellarium **24.1 or newer** on macOS (Apple Silicon or Intel). The plug-in must match your Stellarium version exactly. Releases contain one zip per Stellarium version; check yours under *Help → About*. To use the Oculars framing, enable the Oculars plug-in and set up at least one telescope and one sensor there.

1. Download `MosaicPlanner-<plug-in version>-Stellarium-<your Stellarium version>-macOS.zip` from the [Releases](https://github.com/mkeller0815/MosaicPluginStellarium/releases) page.
2. Quit Stellarium.
3. Unpack the zip and install it in Terminal:

   ```sh
   mkdir -p ~/Library/Application\ Support/Stellarium/modules
   cd ~/Library/Application\ Support/Stellarium/modules
   unzip -o ~/Downloads/MosaicPlanner-*-Stellarium-<your Stellarium version>-macOS.zip
   xattr -dr com.apple.quarantine MosaicPlanner
   ```

   The result must be `~/Library/Application Support/Stellarium/modules/MosaicPlanner/libMosaicPlanner.dylib`.

   The `xattr` step is required. The plug-in is not notarized by Apple, so macOS refuses to load a downloaded copy until the download ("quarantine") flag is removed.
   If you prefer Finder: open the folder with *Go → Go to Folder…*, paste `~/Library/Application Support/Stellarium/modules`, drag the unpacked `MosaicPlanner` folder into it, then run only the `xattr` line above with the full path.
4. Start Stellarium. The Mosaic Planner button (a disc with four overlapping panels) appears in the bottom toolbar, which shows up when you move the mouse to the bottom edge:
   **left-click** shows or hides the mosaic, **right-click** opens the Mosaic Planner window.
   If it doesn't appear, enable *Load at startup* for Mosaic Planner under *Configuration → Plugins* and restart.

**Troubleshooting:** if the plug-in doesn't load, open `~/Library/Application Support/Stellarium/log.txt` and search for `MosaicPlanner`.
"library load disallowed by system policy" means the `xattr` step was skipped.
"This build of the plug-in is for Stellarium X" or "uses incompatible Qt library" means the zip doesn't match your Stellarium version: install the zip for your version. This also applies after updating Stellarium.

**Uninstall:** quit Stellarium and delete the `modules/MosaicPlanner` folder.

## Building (macOS)

You need:

- A Stellarium source tree for **exactly** the version you run (the example below uses 25.3). Only its headers are used.
- Qt 6 headers that are **not newer** than the Qt inside Stellarium.app (Stellarium 25.3 ships Qt 6.9.2).

```sh
git clone --depth 1 --branch v25.3 https://github.com/Stellarium/stellarium.git _deps/stellarium-src
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local   # path to your Qt 6
cmake --build build
ctest --test-dir build          # geometry tests
cmake --install build           # -> ~/Library/Application Support/Stellarium/modules/MosaicPlanner/
```

The plug-in links against the Stellarium executable (`-bundle_loader`) and the Qt frameworks inside `Stellarium.app` (`-DSTELLARIUM_APP=...`). It is built as a universal binary (arm64 + x86_64).

After installing, restart Stellarium. The plug-in loads by default.

## Automated builds and releases

The [Build workflow](.github/workflows/build.yml) builds the plug-in for every Stellarium version in `DEFAULT_STELLARIUM_VERSIONS` (currently 24.1 to 26.3). For each version it downloads the official macOS app, checks out the matching sources and installs the exact Qt version bundled with that app.

- **Push or pull request:** builds all versions as a compatibility check. The zips are available as run artifacts.
- **Manual run** (*Actions → Build → Run workflow*): builds only the Stellarium versions you enter, e.g. `26.3`.
- **Version tag:** builds all versions and attaches the zips and `SHA256SUMS.txt` to the GitHub release. The release is only published if every version built successfully.

To publish a new release:

1. Set the new version in `project(MosaicPlanner VERSION x.y.z ...)` in `CMakeLists.txt` and commit.
2. Tag and push: `git tag -a vx.y.z -m "Mosaic Planner x.y.z" && git push origin main vx.y.z`. The tag must match the version in `CMakeLists.txt`, otherwise the workflow stops.

When a new Stellarium version comes out, add it to `DEFAULT_STELLARIUM_VERSIONS` and publish a new release.

## Limitations

- Like every external Stellarium plug-in, it must be rebuilt for each Stellarium version. Since 0.1.1 the plug-in checks the Stellarium version at startup and refuses to load into a different version instead of crashing. Version 0.1.0 has no such check.
- The Oculars device lists are private. They are read through a standard C++ accessor and the headers of the matching Oculars version, so a header mismatch would break the build or the plug-in.
- The Telescopius CSV has one overlap value; the horizontal overlap is written there.
- Only macOS has been built and tested. Linux should work with the same CMake file. Windows would need an import library for Stellarium.

## License

Copyright (C) 2026 Mario Keller

GNU GPL v2 or later (the same as Stellarium). See [COPYING](COPYING) for the full license text.
