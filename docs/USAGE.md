# Mosaic Planner user guide

This guide explains how to plan a mosaic with the Mosaic Planner plug-in in Stellarium and how to take the plan to your capture software.
For installation, see the [README](../README.md#installation-macos).

## Quick start

1. Set up your equipment in the **Oculars** plug-in: add at least one telescope and one sensor (and a lens such as a reducer if you use one).
2. Select your target in Stellarium, for example search for `M31` with *F3*.
3. Right-click the **Mosaic Planner** button in the bottom toolbar. The Mosaic Planner window opens and the mosaic is shown.
4. On the *Mosaic* tab, click **Selected object** to center the mosaic on the target.
5. Pick the telescope, sensor and lens. Then set the number of **columns × rows**, the **overlap** and the **position angle** until the mosaic covers the target.
6. On the *Panels* tab, click **Export CSV (Telescopius)…** and import the file in N.I.N.A., ASIAIR, StellarMate or a similar tool.

## Opening the plug-in

| What | How |
| --- | --- |
| Show or hide the mosaic | Left-click the toolbar button, or press `Ctrl+Alt+G` |
| Open the Mosaic Planner window | Right-click the toolbar button, or *Configuration (F2) → Plugins → Mosaic Planner → configure* |

The toolbar button is a disc with four overlapping panels. It's in the plug-in group of the bottom toolbar, which appears when you move the mouse to the bottom edge of the window.
The button is white while the mosaic is shown and grey while it is hidden. Opening the window also shows the mosaic.

All actions are listed under *Configuration → Keyboard shortcuts → Mosaic Planner*, where you can change the shortcuts or add new ones:

- Show mosaic plan (`Ctrl+Alt+G` by default)
- Mosaic Planner window
- Move/rotate mosaic with the mouse
- Center mosaic on selected object

## The mosaic on the sky

The mosaic is drawn as a grid of panels. Each panel is the field of view of your camera.

- Each panel shows its number. Panels are numbered row by row, starting at panel 1 in the top-left corner (with north up, east is on the left).
- A title along the top edge shows the target name and the total field size.
- A cross marks the mosaic center.
- The selected panel (see [Panels tab](#panels-tab)) is highlighted.

Coordinates are J2000. The mosaic stays fixed on the sky while you pan, zoom or change the time.

## Mosaic tab

### Target (J2000)

- **Name:** the name used in the title and in exported files. **Selected object** fills it in automatically.
- **Right ascension / Declination:** the mosaic center. You can type values directly.
- **Selected object:** centers the mosaic on the object selected in Stellarium.
- **View center:** centers the mosaic on the center of the screen.
- **Go to mosaic:** points the view at the mosaic and zooms so that the whole mosaic fits.

### Camera framing

The panel size is the field of view of your camera. It can come from two places:

- **From Oculars plug-in** (default): choose the **Telescope**, **Sensor** and **Lens** from the equipment you set up in Oculars. Changing the selection here also changes it in Oculars, and changes made in Oculars show up here.
  The field of view takes the lens multiplier and the sensor binning into account.
- **Manual:** enter the **focal length** and the **sensor width and height** in millimetres. Use this if you don't use Oculars, or for equipment you haven't added to Oculars.

The panel field of view is shown below the selection.

### Mosaic

- **Panels (columns × rows):** 1 to 50 in each direction.
- **Overlap (horizontal / vertical):** how much neighbouring panels overlap, in percent of the panel size. 10–20 % is common so that the panels can be stitched reliably.
- **Position angle (E of N):** the rotation of the mosaic. It is the direction of the sensor's "up" side (the short side for a landscape sensor), measured from north through east. This is the same convention as Telescopius and the Telescopius CSV.
- **Sync rotation with the Oculars sensor rotation:** links the position angle with the sensor rotation in Oculars in both directions. Turning it on takes over the rotation currently set in Oculars.
  Oculars measures its rotation the other way round, so a position angle of 35° is −35° in Oculars.
- **Same rotation for all panels:** by default each panel is rotated slightly so that it stays aligned with the mosaic grid. This matters for large mosaics and near the celestial poles, and is the same as "rotate panes to preserve alignment" in Telescopius.
  With this option, all panels use the same position angle instead. Use it if your capture software or mount can't change the rotation between panels.

A summary below shows the number of panels and the total field size.

## Panels tab

The table lists every panel with its row, column, RA, Dec and position angle (PA). These are the coordinates to send to the mount for each panel.

- **Click** a row to select the panel. It is highlighted on the sky.
- **Double-click** a row, or click **Go to panel**, to point the view at that panel.
  Tip: turn on the CCD frame in Oculars and set the same equipment. The Oculars frame then shows the exact framing of that panel, for example to check guide stars or bright stars at the edges.

### Export and plans

- **Export CSV (Telescopius)…:** writes a file in the Telescopius mosaic format. The format is: `Pane, RA, DEC, Position Angle (East), Pane width (arcmins), Pane height (arcmins), Overlap, Row, Column`. The first line is the mosaic center, followed by one line per panel.
  N.I.N.A. (Sequencer → import of a Telescopius mosaic), ASIAIR, StellarMate and many other tools can import it.
  The format has only one overlap value; the horizontal overlap is written there.
- **Export CSV (degrees)…:** writes one line per panel with RA and Dec in decimal degrees and in h:m:s / d:m:s, the position angle and the panel size. It is useful for spreadsheets and your own scripts.
- **Save plan… / Load plan…:** saves or loads the whole plan as a JSON file. The file contains the target, center, grid, overlap, rotation, framing source and, with Oculars, the names of the telescope, sensor and lens.
  When you load a plan, the same Oculars equipment is selected again by name if it exists.

## Display tab

- **Show mosaic:** shows or hides the mosaic, like the toolbar button.
- **Edit with the mouse:** lets you place the mosaic with the mouse directly on the sky:
  - **drag** the mosaic to move it,
  - **Shift+drag** (or Ctrl+drag) to rotate it around its center,
  - **click** a panel to select it (click it again to clear the selection).

  Dragging outside the mosaic pans the view as usual. While the option is on, clicks inside the mosaic go to the plug-in; turn it off to select stars inside the mosaic again.
- **Show panel numbers and title**
- **Fill panels:** draws a light fill inside the panels in addition to the outlines.
- **Color:** the color of the mosaic.

All settings are saved automatically and restored at the next start of Stellarium.

## Typical workflow

1. Frame a target that is larger than your field of view, for example M31 with a short refractor.
2. Center the mosaic on the object, then set the columns and rows so that the target and some sky around it are covered.
3. Turn on *Edit with the mouse* and drag or rotate the mosaic until the composition looks right. A rotation that follows the shape of the target (such as the long axis of M31) often saves panels.
4. Check the panels against bright stars or the horizon using *Go to panel* and the Oculars frame.
5. Save the plan, so you can change it later, and export the Telescopius CSV for your capture software.
6. When imaging, set your camera rotator, or rotate the camera by hand, to the position angle from the table.

## Scripting

Every setting is a property or slot of the `MosaicPlanner` module and can be used from Stellarium scripts (*F12* script console):

```javascript
MosaicPlanner.setTargetName("M31");
MosaicPlanner.setCenter(10.6847, 41.2689);   // RA, Dec in degrees (J2000)
MosaicPlanner.setColumns(3);
MosaicPlanner.setRows(2);
MosaicPlanner.setOverlapH(15);
MosaicPlanner.setOverlapV(15);
MosaicPlanner.setRotation(35);               // position angle in degrees
MosaicPlanner.setEnabled(true);
MosaicPlanner.gotoMosaic();
MosaicPlanner.exportTelescopiusCsv("/Users/me/Desktop/M31.csv");
```

Other slots: `centerOnSelectedObject()`, `centerOnView()`, `gotoPanel(index)` (index starts at 0), `exportDecimalCsv(path)`, `savePlan(path)`, `loadPlan(path)`, `setUseOculars(bool)`, `setManualFocalLength(mm)`, `setManualSensorWidth(mm)`, `setManualSensorHeight(mm)`, `setSyncRotation(bool)`, `setConstantRotation(bool)`, `setEditMode(bool)`, `setShowLabels(bool)`, `setFillPanels(bool)`, `setDialogVisible(bool)`, `restoreDefaultSettings()`.

## Questions

**The mosaic shows "The panel field of view is unknown".**
With *From Oculars plug-in*, the Oculars plug-in must be loaded and have at least one telescope and one sensor. Otherwise switch to *Manual*.

**The panels don't match the Oculars CCD frame.**
Check that the same telescope, sensor and lens are selected, and that the position angle matches (Oculars uses the negative angle). *Sync rotation* keeps both in step.

**My capture software rotates the panels differently.**
Turn on *Same rotation for all panels* if the software uses one rotation for the whole mosaic.

**The toolbar button is missing.**
Move the mouse to the bottom edge of the window to show the toolbar. If the button is still missing, see the troubleshooting section in the [README](../README.md#installation-macos).
