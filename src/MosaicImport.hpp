/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef MOSAICIMPORT_HPP
#define MOSAICIMPORT_HPP

#include "MosaicGeometry.hpp"

#include <array>
#include <map>
#include <string>
#include <vector>

//! Reading of mosaic CSV files and FITS headers. Like MosaicGeometry it has
//! no dependency on Stellarium or Qt so it can be unit tested on its own.
namespace mosaic
{

//! One panel line of a mosaic CSV file.
struct ImportedPanel
{
	int row = 0;                //!< 0-based, 0 = top row
	int column = 0;             //!< 0-based, 0 = left column
	double raDeg = 0.;
	double decDeg = 0.;
	double paDeg = 0.;
	bool hasPa = false;
};

//! Contents of a mosaic CSV file (Telescopius format or the decimal degrees export).
struct ImportedMosaic
{
	std::vector<ImportedPanel> panels;
	bool hasCenter = false;     //!< a "Center" line was found (Telescopius format)
	double centerRaDeg = 0.;
	double centerDecDeg = 0.;
	double centerPaDeg = 0.;
	bool hasCenterPa = false;
	double fovXDeg = 0.;        //!< 0 if the file has no panel size
	double fovYDeg = 0.;
	double overlapPercent = -1.; //!< -1 if the file has no overlap
	std::string name;           //!< target name, empty if unknown
};

//! Parses a mosaic CSV. Columns are found by their header names, the delimiter may be ',', ';' or tab.
//! RA and Dec may be decimal degrees or sexagesimal ("20hr 16' 00\"", "20h16m00s", "+38°54'18\"").
bool parseMosaicCsv(const std::string& text, ImportedMosaic& mosaic, std::string& error);

//! Derives the mosaic parameters (center, PA, grid, overlap, rotation mode) from the imported panels.
//! The panel field of view is taken from the file; if the file has none, the one in p is kept.
bool fitMosaicParams(const ImportedMosaic& mosaic, MosaicParams& p, std::string& error);

//! Parses an angle. Sexagesimal values of RA are taken as hours, unless they have a degree sign.
//! Plain numbers are degrees.
bool parseAngle(const std::string& text, bool isRa, double& deg);

using FitsHeader = std::map<std::string, std::string>;

//! Parses FITS header cards (80 characters each) up to the END card.
//! String values are returned without quotes. Returns false if the data does not start like a FITS file.
bool parseFitsHeader(const std::string& data, FitsHeader& header);

//! The sky footprint of an image.
struct ReferenceFrame
{
	Vec3 center;
	Vec3 axisX;                 //!< sky direction of the image +x axis (along NAXIS1), tangent at center
	Vec3 axisY;                 //!< sky direction of the image +y axis (along NAXIS2), tangent at center
	double fovXDeg = 0.;        //!< field of view along the image x axis
	double fovYDeg = 0.;
	double pixelScaleArcsec = 0.;
	int width = 0;              //!< pixels
	int height = 0;
	//! False if the header has no WCS: the position comes from the pointing coordinates
	//! and the rotation is unknown (axisY points north).
	bool hasWcs = false;
	std::array<Vec3, 4> corners;
	std::string object;
	std::string dateObs;
};

//! Reads the footprint from the WCS keywords (CRVAL, CRPIX, CD or PC/CDELT or CROTA, TAN projection).
//! Without a WCS falls back to RA/DEC (or OBJCTRA/OBJCTDEC), FOCALLEN and XPIXSZ/YPIXSZ.
bool referenceFromFits(const FitsHeader& header, ReferenceFrame& ref, std::string& error);

//! The frame a panel would need to coincide with the reference image: its center, and the image axis
//! that corresponds to the panel's height (the image may be portrait while the panel is landscape),
//! pointing to the side closest to the panel's up direction. Without a WCS the panel's rotation is kept.
Frame referenceTargetFrame(const ReferenceFrame& ref, const Panel& panel, double fovXDeg, double fovYDeg);

} // namespace mosaic

#endif // MOSAICIMPORT_HPP
