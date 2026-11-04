/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "MosaicImport.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <locale>
#include <sstream>

namespace mosaic
{

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDegToRad = kPi / 180.;
static constexpr double kRadToDeg = 180. / kPi;

namespace
{
std::string trim(const std::string& s)
{
	const size_t begin = s.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
		return std::string();
	const size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(begin, end - begin + 1);
}

std::string lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool startsWith(const std::string& s, const std::string& prefix)
{
	return s.compare(0, prefix.size(), prefix) == 0;
}

// Locale independent: Stellarium runs with the user's locale, where strtod may expect a decimal comma
bool toDouble(std::string s, double& value)
{
	s = trim(s);
	if (s.empty())
		return false;
	// FORTRAN style exponents are allowed in FITS
	std::replace(s.begin(), s.end(), 'D', 'E');
	std::replace(s.begin(), s.end(), 'd', 'e');
	std::istringstream in(s);
	in.imbue(std::locale::classic());
	in >> value;
	if (in.fail())
		return false;
	in >> std::ws;
	return in.eof() && std::isfinite(value);
}

bool toInt(const std::string& s, int& value)
{
	double d;
	if (!toDouble(s, d) || d != std::floor(d) || std::fabs(d) > 1e9)
		return false;
	value = static_cast<int>(d);
	return true;
}

// Difference a-b of two angles in degrees, in -180..180
double angleDiff(double a, double b)
{
	const double d = normalizeDeg360(a - b);
	return d > 180. ? d - 360. : d;
}

Vec3 tangentAt(const Vec3& center, const Vec3& v)
{
	return (v - center * center.dot(v)).normalized();
}

// Splits a CSV line, honouring quoted fields ("" inside quotes is a quote)
std::vector<std::string> splitCsvLine(const std::string& line, char delimiter)
{
	std::vector<std::string> fields;
	std::string field;
	bool quoted = false;
	for (size_t i = 0; i < line.size(); ++i)
	{
		const char c = line[i];
		if (quoted)
		{
			if (c == '"' && i + 1 < line.size() && line[i + 1] == '"')
			{
				field += '"';
				++i;
			}
			else if (c == '"')
				quoted = false;
			else
				field += c;
		}
		else if (c == '"' && trim(field).empty())
			quoted = true; // only at the start of a field: Telescopius writes 00" for arcseconds
		else if (c == delimiter)
		{
			fields.push_back(trim(field));
			field.clear();
		}
		else
			field += c;
	}
	fields.push_back(trim(field));
	return fields;
}

char detectDelimiter(const std::string& header)
{
	const char candidates[] = {',', ';', '\t'};
	char best = ',';
	long bestCount = 0;
	for (char c : candidates)
	{
		const long count = std::count(header.begin(), header.end(), c);
		if (count > bestCount)
		{
			best = c;
			bestCount = count;
		}
	}
	return best;
}

// Header name reduced to lower case letters and digits: "Pane width (arcmins)" -> "panewidtharcmins"
std::string headerKey(const std::string& s)
{
	std::string key;
	for (unsigned char c : s)
		if (std::isalnum(c))
			key += static_cast<char>(std::tolower(c));
	return key;
}

// A number in a CSV field, which may have a decimal comma (the fields are already split)
bool csvNumber(std::string s, double& value)
{
	std::replace(s.begin(), s.end(), ',', '.');
	return toDouble(s, value);
}

// Size in degrees; the unit is taken from the header, arcminutes by default
bool parseSize(const std::string& text, const std::string& key, double& deg)
{
	double v;
	if (!csvNumber(text, v) || v <= 0.)
		return false;
	if (key.find("arcsec") != std::string::npos)
		deg = v / 3600.;
	else if (key.find("deg") != std::string::npos)
		deg = v;
	else
		deg = v / 60.;
	return true;
}
} // namespace

bool parseAngle(const std::string& text, bool isRa, double& deg)
{
	std::string s = trim(text);
	if (s.empty())
		return false;
	// A comma can only be a decimal separator here, the fields are already split
	std::replace(s.begin(), s.end(), ',', '.');

	double numbers[3] = {0., 0., 0.};
	int count = 0;
	bool negative = false;
	bool hasDegreeSign = false;
	bool hasHourSign = false;
	for (size_t i = 0; i < s.size();)
	{
		const unsigned char c = static_cast<unsigned char>(s[i]);
		if (std::isdigit(c) || (c == '.' && i + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[i + 1]))))
		{
			size_t end = i;
			while (end < s.size() && (std::isdigit(static_cast<unsigned char>(s[end])) || s[end] == '.'))
				++end;
			if (count == 3)
				return false;
			if (!toDouble(s.substr(i, end - i), numbers[count]))
				return false;
			++count;
			i = end;
			continue;
		}
		if (count == 0 && (c == '-' || s.compare(i, 3, "\xE2\x88\x92") == 0))
			negative = true;
		else if (s.compare(i, 2, "\xC2\xB0") == 0 || s.compare(i, 2, "\xC2\xBA") == 0 || c == 'd' || c == 'D')
			hasDegreeSign = true;
		else if (c == 'h' || c == 'H')
			hasHourSign = true;
		++i;
	}
	if (count == 0)
		return false;
	const bool sexagesimal = count > 1 || hasDegreeSign || hasHourSign;
	deg = numbers[0] + numbers[1] / 60. + numbers[2] / 3600.;
	if (isRa && sexagesimal && !hasDegreeSign)
		deg *= 15.;
	if (negative)
		deg = -deg;
	return true;
}

bool parseMosaicCsv(const std::string& input, ImportedMosaic& mosaic, std::string& error)
{
	mosaic = ImportedMosaic();
	std::string text = input;
	if (startsWith(text, "\xEF\xBB\xBF"))
		text.erase(0, 3);

	std::vector<std::string> lines;
	std::istringstream in(text);
	std::string line;
	while (std::getline(in, line))
		if (!trim(line).empty())
			lines.push_back(line);
	if (lines.size() < 2)
	{
		error = "The file has no panels.";
		return false;
	}

	const char delimiter = detectDelimiter(lines.front());
	const std::vector<std::string> header = splitCsvLine(lines.front(), delimiter);
	int nameCol = -1, rowCol = -1, columnCol = -1, raCol = -1, decCol = -1, paCol = -1;
	int widthCol = -1, heightCol = -1, overlapCol = -1;
	bool raInDegrees = false, decInDegrees = false;
	std::vector<std::string> keys;
	for (int i = 0; i < static_cast<int>(header.size()); ++i)
	{
		const std::string k = headerKey(header[static_cast<size_t>(i)]);
		keys.push_back(k);
		const bool degrees = k.find("deg") != std::string::npos;
		if (k == "pane" || k == "name" || k == "target")
			nameCol = i;
		else if (k == "row")
			rowCol = i;
		else if (k == "column" || k == "col")
			columnCol = i;
		else if (startsWith(k, "ra") && !startsWith(k, "rate") && (raCol < 0 || (degrees && !raInDegrees)))
		{
			raCol = i;
			raInDegrees = degrees;
		}
		else if (startsWith(k, "dec") && (decCol < 0 || (degrees && !decInDegrees)))
		{
			decCol = i;
			decInDegrees = degrees;
		}
		else if (startsWith(k, "positionangle") || k == "pa" || k == "padeg" || startsWith(k, "rotation") || k == "angle")
			paCol = i;
		else if (startsWith(k, "panewidth") || startsWith(k, "panelwidth") || startsWith(k, "width") || startsWith(k, "fovx"))
			widthCol = i;
		else if (startsWith(k, "paneheight") || startsWith(k, "panelheight") || startsWith(k, "height") || startsWith(k, "fovy"))
			heightCol = i;
		else if (startsWith(k, "overlap"))
			overlapCol = i;
	}
	if (raCol < 0 || decCol < 0)
	{
		error = "The file has no RA and Dec columns.";
		return false;
	}
	if (rowCol < 0 || columnCol < 0)
	{
		error = "The file has no Row and Column columns.";
		return false;
	}

	for (size_t n = 1; n < lines.size(); ++n)
	{
		const std::vector<std::string> f = splitCsvLine(lines[n], delimiter);
		auto field = [&f](int col) { return col >= 0 && col < static_cast<int>(f.size()) ? f[static_cast<size_t>(col)] : std::string(); };
		const std::string lineNo = std::to_string(n + 1);

		double ra, dec;
		if (!parseAngle(field(raCol), true, ra) || !parseAngle(field(decCol), false, dec) || std::fabs(dec) > 90.)
		{
			error = "Invalid coordinates in line " + lineNo + ".";
			return false;
		}
		double pa = 0.;
		const bool hasPa = csvNumber(field(paCol), pa);

		double size;
		if (mosaic.fovXDeg <= 0. && widthCol >= 0 && parseSize(field(widthCol), keys[static_cast<size_t>(widthCol)], size))
			mosaic.fovXDeg = size;
		if (mosaic.fovYDeg <= 0. && heightCol >= 0 && parseSize(field(heightCol), keys[static_cast<size_t>(heightCol)], size))
			mosaic.fovYDeg = size;
		std::string overlap = field(overlapCol);
		overlap.erase(std::remove(overlap.begin(), overlap.end(), '%'), overlap.end());
		double overlapValue;
		if (mosaic.overlapPercent < 0. && csvNumber(overlap, overlapValue) && overlapValue >= 0. && overlapValue < 100.)
			mosaic.overlapPercent = overlapValue;

		const std::string name = field(nameCol);
		int row, column;
		const bool hasGridPosition = toInt(field(rowCol), row) && toInt(field(columnCol), column);
		if (lower(name) == "center" || lower(name) == "centre" || (!hasGridPosition && !mosaic.hasCenter))
		{
			// Telescopius writes the mosaic center as the first line, without row and column
			mosaic.hasCenter = true;
			mosaic.centerRaDeg = normalizeDeg360(ra);
			mosaic.centerDecDeg = dec;
			mosaic.centerPaDeg = normalizeDeg360(pa);
			mosaic.hasCenterPa = hasPa;
			continue;
		}
		if (!hasGridPosition || row < 1 || column < 1)
		{
			error = "Invalid row or column in line " + lineNo + ".";
			return false;
		}
		if (mosaic.name.empty() && nameCol >= 0 && keys[static_cast<size_t>(nameCol)] != "pane")
		{
			// The degrees export names the panels "<target> P<number>"
			std::string target = name;
			const size_t p = target.rfind(" P");
			if (p != std::string::npos && p + 2 < target.size()
			    && std::all_of(target.begin() + static_cast<long>(p) + 2, target.end(), [](unsigned char c) { return std::isdigit(c); }))
				target.erase(p);
			mosaic.name = trim(target);
		}

		ImportedPanel panel;
		panel.row = row - 1;
		panel.column = column - 1;
		panel.raDeg = normalizeDeg360(ra);
		panel.decDeg = dec;
		panel.paDeg = normalizeDeg360(pa);
		panel.hasPa = hasPa;
		mosaic.panels.push_back(panel);
	}
	if (mosaic.panels.empty())
	{
		error = "The file has no panels.";
		return false;
	}
	return true;
}

bool fitMosaicParams(const ImportedMosaic& mosaic, MosaicParams& p, std::string& error)
{
	int columns = 0, rows = 0;
	for (const ImportedPanel& panel : mosaic.panels)
	{
		columns = std::max(columns, panel.column + 1);
		rows = std::max(rows, panel.row + 1);
	}
	if (columns < 1 || rows < 1 || columns > 50 || rows > 50)
	{
		error = "The mosaic must have 1 to 50 rows and columns.";
		return false;
	}
	// Grid of panel indices, row-major
	std::vector<int> grid(static_cast<size_t>(columns * rows), -1);
	for (size_t i = 0; i < mosaic.panels.size(); ++i)
	{
		int& cell = grid[static_cast<size_t>(mosaic.panels[i].row * columns + mosaic.panels[i].column)];
		if (cell >= 0)
		{
			error = "The file has the same row and column twice.";
			return false;
		}
		cell = static_cast<int>(i);
	}
	if (std::find(grid.begin(), grid.end(), -1) != grid.end())
	{
		error = "The panels do not form a complete grid.";
		return false;
	}
	auto at = [&](int row, int column) {
		const ImportedPanel& panel = mosaic.panels[static_cast<size_t>(grid[static_cast<size_t>(row * columns + column)])];
		return raDecToVec(panel.raDeg, panel.decDeg);
	};

	MosaicParams q = p;
	q.columns = columns;
	q.rows = rows;
	if (mosaic.fovXDeg > 0. && mosaic.fovYDeg > 0.)
	{
		q.fovXDeg = mosaic.fovXDeg;
		q.fovYDeg = mosaic.fovYDeg;
	}
	if (q.fovXDeg <= 0. || q.fovYDeg <= 0.)
	{
		error = "The file has no panel size and the panel field of view is unknown.";
		return false;
	}

	// The panels are symmetric around the center, so their mean direction is the center
	Vec3 center;
	if (mosaic.hasCenter)
		center = raDecToVec(mosaic.centerRaDeg, mosaic.centerDecDeg);
	else
	{
		for (const ImportedPanel& panel : mosaic.panels)
			center = center + raDecToVec(panel.raDeg, panel.decDeg);
		center = center.normalized();
	}
	vecToRaDec(center, q.raDeg, q.decDeg);

	// The mosaic "up" direction runs from the bottom row to the top row
	Vec3 up;
	if (rows > 1)
	{
		for (int c = 0; c < columns; ++c)
			up = up + at(0, c) - at(rows - 1, c);
		up = tangentAt(center, up);
	}
	else if (columns > 1)
	{
		// right = center x up, so up = right x center
		up = tangentAt(center, at(0, columns - 1) - at(0, 0)).cross(center);
	}
	if (up.length() > 0.5)
	{
		q.paDeg = positionAngleOf(center, up);
		// Coordinates rounded to seconds give a less precise angle than the one in the file
		if (mosaic.hasCenterPa && std::fabs(angleDiff(mosaic.centerPaDeg, q.paDeg)) < 2.)
			q.paDeg = mosaic.centerPaDeg;
	}
	else if (mosaic.hasCenterPa)
		q.paDeg = mosaic.centerPaDeg;
	else
		q.paDeg = mosaic.panels.front().hasPa ? mosaic.panels.front().paDeg : 0.;

	// Overlap from the panel spacing on the tangent plane of the center
	const Frame frame = frameAt(center, q.paDeg);
	auto fitOverlap = [&](bool alongColumns, double fovDeg, double& overlap) {
		const int count = alongColumns ? columns : rows;
		if (count < 2)
			return true;
		double first = 0., last = 0.;
		const int others = alongColumns ? rows : columns;
		for (int k = 0; k < others; ++k)
		{
			double x0, y0, x1, y1;
			const Vec3 a = alongColumns ? at(k, 0) : at(0, k);
			const Vec3 b = alongColumns ? at(k, columns - 1) : at(rows - 1, k);
			if (!projectToFrame(frame, a, x0, y0) || !projectToFrame(frame, b, x1, y1))
				return false;
			first += alongColumns ? x0 : y0;
			last += alongColumns ? x1 : y1;
		}
		// x grows to the right, y grows upwards (row 0 is the top row)
		const double step = (alongColumns ? last - first : first - last) / others / (count - 1);
		overlap = 1. - step / tangentSize(fovDeg);
		if (mosaic.overlapPercent >= 0. && std::fabs(overlap * 100. - mosaic.overlapPercent) < 1.5)
			overlap = mosaic.overlapPercent / 100.;
		else
			overlap = std::round(overlap * 1000.) / 1000.;
		return overlap >= 0. && overlap < 0.9;
	};
	double overlapX = -1., overlapY = -1.;
	if (!fitOverlap(true, q.fovXDeg, overlapX) || !fitOverlap(false, q.fovYDeg, overlapY))
	{
		error = "The panel spacing does not match the panel size.";
		return false;
	}
	// A direction with a single panel has no spacing: use the overlap of the file or the other direction
	const double fallback = mosaic.overlapPercent >= 0. ? mosaic.overlapPercent / 100. : p.overlapX;
	q.overlapX = overlapX >= 0. ? overlapX : (overlapY >= 0. ? overlapY : fallback);
	q.overlapY = overlapY >= 0. ? overlapY : (overlapX >= 0. ? overlapX : fallback);

	// Rotation mode: whichever reproduces the panel angles of the file better
	q.constantRotation = false;
	const bool allPa = std::all_of(mosaic.panels.begin(), mosaic.panels.end(), [](const ImportedPanel& panel) { return panel.hasPa; });
	if (allPa && mosaic.panels.size() > 1)
	{
		double error2[2] = {0., 0.};
		for (int mode = 0; mode < 2; ++mode)
		{
			MosaicParams t = q;
			t.constantRotation = mode == 1;
			const std::vector<Panel> panels = computePanels(t);
			for (const Panel& panel : panels)
			{
				const double d = angleDiff(panel.paDeg, mosaic.panels[static_cast<size_t>(grid[static_cast<size_t>(panel.number - 1)])].paDeg);
				error2[mode] += d * d;
			}
		}
		q.constantRotation = error2[1] < error2[0];
	}
	p = q;
	return true;
}

bool parseFitsHeader(const std::string& data, FitsHeader& header)
{
	header.clear();
	if (data.size() < 80 || (!startsWith(data, "SIMPLE  =") && !startsWith(data, "XTENSION=")))
		return false;
	for (size_t pos = 0; pos + 80 <= data.size(); pos += 80)
	{
		const std::string card = data.substr(pos, 80);
		const std::string key = trim(card.substr(0, 8));
		if (key == "END")
			break;
		if (key.empty() || card.compare(8, 2, "= ") != 0)
			continue;
		std::string rest = card.substr(10);
		std::string value;
		const size_t start = rest.find_first_not_of(' ');
		if (start != std::string::npos && rest[start] == '\'')
		{
			for (size_t i = start + 1; i < rest.size(); ++i)
			{
				if (rest[i] == '\'')
				{
					if (i + 1 < rest.size() && rest[i + 1] == '\'')
					{
						value += '\'';
						++i;
						continue;
					}
					break;
				}
				value += rest[i];
			}
			// Trailing blanks in FITS strings are not significant
			const size_t end = value.find_last_not_of(' ');
			value = end == std::string::npos ? std::string() : value.substr(0, end + 1);
		}
		else
			value = trim(rest.substr(0, rest.find('/')));
		header.emplace(key, value);
	}
	return true;
}

bool referenceFromFits(const FitsHeader& header, ReferenceFrame& ref, std::string& error)
{
	ref = ReferenceFrame();
	auto text = [&header](const char* key) {
		const auto it = header.find(key);
		return it == header.end() ? std::string() : it->second;
	};
	auto number = [&](const char* key, double& value) { return toDouble(text(key), value); };

	double w = 0., h = 0.;
	// Astrometry.net .wcs files have no data, but give the size of the solved image
	if (!(number("NAXIS1", w) && number("NAXIS2", h)) && !(number("IMAGEW", w) && number("IMAGEH", h)))
		w = h = 0.;
	if (w < 1. || h < 1.)
	{
		error = "The header does not give the image size.";
		return false;
	}
	ref.width = static_cast<int>(w);
	ref.height = static_cast<int>(h);
	ref.object = text("OBJECT");
	ref.dateObs = text("DATE-OBS");

	double crval1, crval2, crpix1, crpix2;
	if (number("CRVAL1", crval1) && number("CRVAL2", crval2) && number("CRPIX1", crpix1) && number("CRPIX2", crpix2))
	{
		if (!startsWith(text("CTYPE1"), "RA") || !startsWith(text("CTYPE2"), "DEC"))
		{
			error = "The WCS is not in RA and Dec.";
			return false;
		}
		// Linear part of the WCS, in degrees per pixel
		double cd11 = 0., cd12 = 0., cd21 = 0., cd22 = 0.;
		double cdelt1 = 1., cdelt2 = 1.;
		// Missing CD elements are 0 (no short-circuit: read all of them)
		const bool hasCd = number("CD1_1", cd11) | number("CD1_2", cd12) | number("CD2_1", cd21) | number("CD2_2", cd22);
		if (!hasCd && number("CDELT1", cdelt1) && number("CDELT2", cdelt2))
		{
			double pc11 = 1., pc12 = 0., pc21 = 0., pc22 = 1., crota = 0.;
			const bool hasPc = number("PC1_1", pc11) | number("PC1_2", pc12) | number("PC2_1", pc21) | number("PC2_2", pc22);
			if (hasPc)
			{
				cd11 = cdelt1 * pc11;
				cd12 = cdelt1 * pc12;
				cd21 = cdelt2 * pc21;
				cd22 = cdelt2 * pc22;
			}
			else
			{
				if (!number("CROTA2", crota))
					number("CROTA1", crota);
				const double c = std::cos(crota * kDegToRad), s = std::sin(crota * kDegToRad);
				cd11 = cdelt1 * c;
				cd12 = -cdelt2 * s;
				cd21 = cdelt1 * s;
				cd22 = cdelt2 * c;
			}
		}
		const double det = cd11 * cd22 - cd12 * cd21;
		if (std::fabs(det) < 1e-20)
		{
			error = "The WCS has no valid pixel scale.";
			return false;
		}

		// Gnomonic (TAN) projection; SIP distortions are ignored, they are tiny at the scale of a panel
		const Vec3 c0 = raDecToVec(crval1, crval2);
		Vec3 north, east;
		localNorthEast(c0, north, east);
		auto sky = [&](double x, double y) {
			const double xi = (cd11 * (x - crpix1) + cd12 * (y - crpix2)) * kDegToRad;
			const double eta = (cd21 * (x - crpix1) + cd22 * (y - crpix2)) * kDegToRad;
			return (c0 + east * xi + north * eta).normalized();
		};
		const double cx = 0.5 * (w + 1.), cy = 0.5 * (h + 1.);
		ref.center = sky(cx, cy);
		ref.axisX = tangentAt(ref.center, sky(cx + 1., cy) - ref.center);
		ref.axisY = tangentAt(ref.center, sky(cx, cy + 1.) - ref.center);
		ref.corners = {sky(0.5, h + 0.5), sky(0.5, 0.5), sky(w + 0.5, 0.5), sky(w + 0.5, h + 0.5)};
		const double scaleX = std::sqrt(cd11 * cd11 + cd21 * cd21) * kDegToRad;
		const double scaleY = std::sqrt(cd12 * cd12 + cd22 * cd22) * kDegToRad;
		ref.fovXDeg = 2. * std::atan(0.5 * w * scaleX) * kRadToDeg;
		ref.fovYDeg = 2. * std::atan(0.5 * h * scaleY) * kRadToDeg;
		ref.pixelScaleArcsec = std::sqrt(std::fabs(det)) * 3600.;
		ref.hasWcs = true;
	}
	else
	{
		// Not plate solved: use the pointing of the mount and the optics
		double ra, dec, focal, pixelX, pixelY;
		const bool hasRa = parseAngle(text("RA"), true, ra) || parseAngle(text("OBJCTRA"), true, ra);
		const bool hasDec = parseAngle(text("DEC"), false, dec) || parseAngle(text("OBJCTDEC"), false, dec);
		if (!hasRa || !hasDec || !number("FOCALLEN", focal) || focal <= 0. || !number("XPIXSZ", pixelX) || pixelX <= 0.)
		{
			error = "The header has no WCS (plate solve the image first) and no pointing and focal length.";
			return false;
		}
		if (!number("YPIXSZ", pixelY) || pixelY <= 0.)
			pixelY = pixelX;
		const double scaleX = pixelX * 1e-3 / focal;
		const double scaleY = pixelY * 1e-3 / focal;
		ref.fovXDeg = 2. * std::atan(0.5 * w * scaleX) * kRadToDeg;
		ref.fovYDeg = 2. * std::atan(0.5 * h * scaleY) * kRadToDeg;
		ref.pixelScaleArcsec = std::sqrt(scaleX * scaleY) * kRadToDeg * 3600.;
		const Frame f = frameAt(raDecToVec(ra, dec), 0.);
		ref.center = f.center;
		ref.axisY = f.up;
		ref.axisX = f.right;
		const double dx = 0.5 * tangentSize(ref.fovXDeg), dy = 0.5 * tangentSize(ref.fovYDeg);
		ref.corners = {(f.center - f.right * dx + f.up * dy).normalized(), (f.center - f.right * dx - f.up * dy).normalized(),
		               (f.center + f.right * dx - f.up * dy).normalized(), (f.center + f.right * dx + f.up * dy).normalized()};
	}

	// Mirrored images have the opposite winding: reorder to the winding of the panel corners
	const std::array<Vec3, 4>& c = ref.corners;
	if ((c[1] - c[0]).cross(c[2] - c[1]).dot(ref.center) > 0.)
		std::swap(ref.corners[1], ref.corners[3]);
	return true;
}

Frame referenceTargetFrame(const ReferenceFrame& ref, const Panel& panel, double fovXDeg, double fovYDeg)
{
	Frame f;
	f.center = ref.center.normalized();
	const Vec3 panelUp = tangentAt(f.center, panel.up);
	Vec3 up = panelUp;
	if (ref.hasWcs)
	{
		// Candidate image axes for the panel height: the one with the same aspect, or both if the image is square
		std::vector<Vec3> axes;
		const double aspect = ref.fovXDeg / ref.fovYDeg;
		if (std::fabs(aspect - 1.) < 0.02)
			axes = {ref.axisX, ref.axisY};
		else
			axes = {(ref.fovXDeg > ref.fovYDeg) == (fovXDeg >= fovYDeg) ? ref.axisY : ref.axisX};
		double best = -2.;
		for (const Vec3& axis : axes)
		{
			for (double sign : {1., -1.})
			{
				const double d = (axis * sign).dot(panelUp);
				if (d > best)
				{
					best = d;
					up = axis * sign;
				}
			}
		}
	}
	f.up = tangentAt(f.center, up);
	f.right = f.center.cross(f.up).normalized();
	return f;
}

} // namespace mosaic
