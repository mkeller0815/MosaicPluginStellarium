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

## Building (macOS)

You need:

- A Stellarium source tree for **exactly** the version you run (here 25.3). Only its headers are used.
- Qt 6 headers that are **not newer** than the Qt inside Stellarium.app (Stellarium 25.3 ships Qt 6.9.2).

```sh
git clone --depth 1 --branch v25.3 https://github.com/Stellarium/stellarium.git _deps/stellarium-src
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local   # path to your Qt 6
cmake --build build
ctest --test-dir build          # geometry tests
cmake --install build           # -> ~/Library/Application Support/Stellarium/modules/MosaicPlanner/
```

The plug-in links against the Stellarium executable (`-bundle_loader`) and the Qt frameworks inside `Stellarium.app` (`-DSTELLARIUM_APP=...`). It is built as a universal binary (arm64 + x86_64).

After installing, restart Stellarium. The plug-in loads by default. Open it with the toolbar button: left-click opens the window, right-click toggles the overlay. It also appears under Configuration → Plugins.

## Limitations

- Like every external Stellarium plug-in, it must be rebuilt for each Stellarium version.
- The Oculars device lists are private. They are read through a standard C++ accessor and the headers of the matching Oculars version, so a header mismatch would break the build or the plug-in.
- The Telescopius CSV has one overlap value; the horizontal overlap is written there.
- Only macOS has been built and tested. Linux should work with the same CMake file. Windows would need an import library for Stellarium.

## License

Copyright (C) 2026 Mario Keller

GNU GPL v2 or later (the same as Stellarium). See [COPYING](COPYING) for the full license text.
