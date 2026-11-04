# Changelog

## Unreleased

- New **Import CSV…** on the *Panels* tab: reads a Telescopius mosaic CSV or the degrees CSV export and rebuilds the grid, overlap, position angle, rotation mode and panel size from the panels. If the panel size differs from the current camera framing, the framing is switched to manual with that size.
- New *Reference* tab: load the header of a plate-solved FITS file (or an astrometry.net `.wcs` file) of a captured panel. The image frame is drawn on the sky, the closest panel is selected and its offset and rotation are shown. **Align mosaic to reference** moves and rotates the whole mosaic so that the chosen panel coincides with the image. Unsolved files with pointing coordinates and focal length are accepted too; then only the position is used.
- New script slots `importCsv(path)`, `loadReferenceImage(path)`, `clearReferenceImage()`, `alignToReference(index)` and the property `showReference`.

## 0.1.2

- New internal mosaic scale (1.00× to 2.00×, separately for X and Y, optionally synced) in the camera framing settings, for cameras with a built-in mosaic mode. The panel field of view grows accordingly; the scale is saved in the settings and in plans.

## 0.1.1

- Builds for every Stellarium release from 24.1 to 26.3 (one zip per version).
- The plug-in checks the Stellarium version at startup and refuses to load into a different version instead of crashing Stellarium.
- New toolbar button in Stellarium's style: left-click shows or hides the mosaic, right-click opens the Mosaic Planner window.
- New shortcut `Ctrl+Alt+G` to show or hide the mosaic.
- New [user guide](docs/USAGE.md).

## 0.1.0

- First release (Stellarium 25.3 only).
