// Standalone tests for MosaicImport (no Stellarium/Qt needed).
// Copyright (C) 2026 Mario Keller, GNU GPL v2 or later.
#include "MosaicImport.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace mosaic;

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); if (std::fabs(_a - _b) > (eps)) { \
	std::printf("FAIL %s:%d: %s = %.9f, expected %.9f\n", __FILE__, __LINE__, #a, _a, _b); ++failures; } } while (0)

static double angDistDeg(const Vec3& a, const Vec3& b)
{
	return std::acos(std::fmax(-1., std::fmin(1., a.normalized().dot(b.normalized())))) * 180. / M_PI;
}

static double paDiff(double a, double b)
{
	const double d = normalizeDeg360(a - b);
	return d > 180. ? 360. - d : d;
}

// Same layout as MosaicPlanner::exportTelescopiusCsv()
static std::string telescopiusCsv(const MosaicParams& p, int overlapPercent)
{
	char buf[256];
	std::string s = "Pane, RA, DEC, Position Angle (East), Pane width (arcmins), Pane height (arcmins), Overlap, Row, Column\n";
	std::snprintf(buf, sizeof(buf), "Center, %s, %s, %.2f, %.2f, %.2f, %d%%, -, -\n", formatRaTelescopius(p.raDeg).c_str(),
	              formatDecTelescopius(p.decDeg).c_str(), p.paDeg, p.fovXDeg * 60., p.fovYDeg * 60., overlapPercent);
	s += buf;
	for (const Panel& panel : computePanels(p))
	{
		std::snprintf(buf, sizeof(buf), "Pane %d, %s, %s, %.2f, %.2f, %.2f, %d%%, %d, %d\n", panel.number,
		              formatRaTelescopius(panel.raDeg).c_str(), formatDecTelescopius(panel.decDeg).c_str(), panel.paDeg,
		              p.fovXDeg * 60., p.fovYDeg * 60., overlapPercent, panel.row + 1, panel.column + 1);
		s += buf;
	}
	return s;
}

// Same layout as MosaicPlanner::exportDecimalCsv(), in the European flavour with ';' and decimal commas
static std::string decimalCsv(const MosaicParams& p, bool european)
{
	char buf[256];
	std::string s = "name,panel,row,column,ra_deg,dec_deg,ra_hms,dec_dms,position_angle_deg,fov_x_arcmin,fov_y_arcmin\r\n";
	for (const Panel& panel : computePanels(p))
	{
		std::snprintf(buf, sizeof(buf), "\"M 31, \"\"Andromeda\"\" P%d\",%d,%d,%d,%.6f,%.6f,0h00m00s,+0d00m00s,%.2f,%.2f,%.2f\r\n",
		              panel.number, panel.number, panel.row + 1, panel.column + 1, panel.raDeg, panel.decDeg, panel.paDeg,
		              p.fovXDeg * 60., p.fovYDeg * 60.);
		s += buf;
	}
	if (european)
	{
		// Swap ',' and '.' outside of the quoted name
		std::string out;
		bool quoted = false;
		for (char c : s)
		{
			if (c == '"')
				quoted = !quoted;
			out += quoted ? c : (c == ',' ? ';' : (c == '.' ? ',' : c));
		}
		s = out;
	}
	return s;
}

static void testParseAngle()
{
	double d;
	CHECK(parseAngle("20hr 16' 00\"", true, d));
	CHECK_NEAR(d, 304., 1e-9);
	CHECK(parseAngle("5h35m17.3s", true, d));
	CHECK_NEAR(d, 15. * (5. + 35. / 60. + 17.3 / 3600.), 1e-9);
	CHECK(parseAngle("83.8221", true, d));
	CHECK_NEAR(d, 83.8221, 1e-12);
	CHECK(parseAngle("05 35 17.3", true, d));
	CHECK_NEAR(d, 15. * (5. + 35. / 60. + 17.3 / 3600.), 1e-9);
	CHECK(parseAngle("38\xC2\xBA 54' 18\"", false, d));
	CHECK_NEAR(d, 38. + 54. / 60. + 18. / 3600., 1e-9);
	CHECK(parseAngle("-0\xC2\xB0 30'", false, d));
	CHECK_NEAR(d, -0.5, 1e-12);
	CHECK(parseAngle("\xE2\x88\x92" "5:23:28", false, d));
	CHECK_NEAR(d, -(5. + 23. / 60. + 28. / 3600.), 1e-9);
	CHECK(parseAngle("+41d16m09s", false, d));
	CHECK_NEAR(d, 41. + 16. / 60. + 9. / 3600., 1e-9);
	CHECK(parseAngle("-5,3911", false, d));
	CHECK_NEAR(d, -5.3911, 1e-12);
	CHECK(!parseAngle("", true, d));
	CHECK(!parseAngle("-", true, d));
}

static void checkFit(const MosaicParams& expected, const MosaicParams& got, double posEps, double paEps, double overlapEps)
{
	CHECK(got.columns == expected.columns);
	CHECK(got.rows == expected.rows);
	CHECK_NEAR(angDistDeg(raDecToVec(got.raDeg, got.decDeg), raDecToVec(expected.raDeg, expected.decDeg)), 0., posEps);
	CHECK_NEAR(paDiff(got.paDeg, expected.paDeg), 0., paEps);
	CHECK_NEAR(got.overlapX, expected.overlapX, overlapEps);
	CHECK_NEAR(got.overlapY, expected.overlapY, overlapEps);
	CHECK_NEAR(got.fovXDeg, expected.fovXDeg, 1e-3);
	CHECK_NEAR(got.fovYDeg, expected.fovYDeg, 1e-3);
	CHECK(got.constantRotation == expected.constantRotation);
}

static void testTelescopiusRoundTrip()
{
	MosaicParams p;
	p.raDeg = 10.6847; p.decDeg = 41.2689; p.paDeg = 35.;
	p.columns = 3; p.rows = 2;
	p.overlapX = p.overlapY = 0.15;
	p.fovXDeg = 1.8; p.fovYDeg = 1.2;
	for (bool constant : {false, true})
	{
		p.constantRotation = constant;
		ImportedMosaic m;
		std::string error;
		CHECK(parseMosaicCsv(telescopiusCsv(p, 15), m, error));
		CHECK(m.hasCenter && m.hasCenterPa);
		CHECK(m.panels.size() == 6);
		CHECK_NEAR(m.overlapPercent, 15., 1e-9);
		MosaicParams fit;
		CHECK(fitMosaicParams(m, fit, error));
		// Telescopius rounds RA to whole seconds of time
		checkFit(p, fit, 0.005, 1e-6, 1e-9);
	}
	// Different overlaps: the file has only one value, the other one comes from the spacing
	p.constantRotation = false;
	p.overlapY = 0.25;
	ImportedMosaic m;
	std::string error;
	CHECK(parseMosaicCsv(telescopiusCsv(p, 15), m, error));
	MosaicParams fit;
	CHECK(fitMosaicParams(m, fit, error));
	checkFit(p, fit, 0.005, 1e-6, 0.002);
}

static void testDecimalRoundTrip()
{
	MosaicParams p;
	p.raDeg = 359.5; p.decDeg = 75.; p.paDeg = 300.;
	p.columns = 4; p.rows = 3;
	p.overlapX = 0.1; p.overlapY = 0.2;
	p.fovXDeg = 2.5; p.fovYDeg = 1.7;
	for (bool european : {false, true})
	{
		for (bool constant : {false, true})
		{
			p.constantRotation = constant;
			ImportedMosaic m;
			std::string error;
			CHECK(parseMosaicCsv(decimalCsv(p, european), m, error));
			CHECK(!m.hasCenter);
			CHECK(m.name == "M 31, \"Andromeda\"");
			CHECK(m.panels.size() == 12);
			MosaicParams fit;
			CHECK(fitMosaicParams(m, fit, error));
			checkFit(p, fit, 1e-5, 0.01, 1e-9);
		}
	}
	// Single row: the angle comes from the columns
	p.rows = 1;
	p.constantRotation = false;
	ImportedMosaic m;
	std::string error;
	CHECK(parseMosaicCsv(decimalCsv(p, false), m, error));
	MosaicParams fit;
	fit.overlapX = 0.3;
	CHECK(fitMosaicParams(m, fit, error));
	CHECK_NEAR(paDiff(fit.paDeg, p.paDeg), 0., 0.01);
	CHECK_NEAR(fit.overlapX, 0.1, 1e-9);
	CHECK_NEAR(fit.overlapY, 0.1, 1e-9);
}

static void testCsvErrors()
{
	ImportedMosaic m;
	MosaicParams fit;
	std::string error;
	CHECK(!parseMosaicCsv("", m, error));
	CHECK(!parseMosaicCsv("a,b,c\n1,2,3\n", m, error));
	CHECK(!parseMosaicCsv("ra,dec\n10,20\n", m, error));
	CHECK(!parseMosaicCsv("row,column,ra,dec\n1,1,10,95\n", m, error));
	// Incomplete grid
	CHECK(parseMosaicCsv("row,column,ra,dec\n1,1,10,20\n2,2,11,21\n", m, error));
	CHECK(!fitMosaicParams(m, fit, error));
	// No panel size in the file and none known
	CHECK(parseMosaicCsv("row,column,ra,dec\n1,1,10,20\n", m, error));
	fit.fovXDeg = fit.fovYDeg = 0.;
	CHECK(!fitMosaicParams(m, fit, error));
	fit.fovXDeg = 1.; fit.fovYDeg = 0.5;
	CHECK(fitMosaicParams(m, fit, error));
	CHECK_NEAR(fit.raDeg, 10., 1e-9);
}

static std::string card(const std::string& text)
{
	std::string c = text;
	c.resize(80, ' ');
	return c;
}

static std::string fitsHeader(double ra, double dec, double paDeg, double scaleDeg, int w, int h, bool mirrored)
{
	const double t = paDeg * M_PI / 180.;
	const double m = mirrored ? -1. : 1.;
	char buf[96];
	std::string s = card("SIMPLE  =                    T / conforms to FITS standard");
	s += card("BITPIX  =                   16");
	s += card("NAXIS   =                    2");
	std::snprintf(buf, sizeof(buf), "NAXIS1  = %20d", w); s += card(buf);
	std::snprintf(buf, sizeof(buf), "NAXIS2  = %20d", h); s += card(buf);
	s += card("OBJECT  = 'M 31 Panel 2'       / object name");
	s += card("CTYPE1  = 'RA---TAN-SIP'");
	s += card("CTYPE2  = 'DEC--TAN-SIP'");
	std::snprintf(buf, sizeof(buf), "CRVAL1  = %20.10E", ra); s += card(buf);
	std::snprintf(buf, sizeof(buf), "CRVAL2  = %20.10E", dec); s += card(buf);
	std::snprintf(buf, sizeof(buf), "CRPIX1  = %20.3f", 0.5 * (w + 1)); s += card(buf);
	std::snprintf(buf, sizeof(buf), "CRPIX2  = %20.3f", 0.5 * (h + 1)); s += card(buf);
	// +x along the panel "right", +y along the panel "up" (see the conventions in MosaicGeometry.hpp)
	std::snprintf(buf, sizeof(buf), "CD1_1   = %20.12E", -scaleDeg * std::cos(t) * m); s += card(buf);
	std::snprintf(buf, sizeof(buf), "CD1_2   = %20.12E", scaleDeg * std::sin(t)); s += card(buf);
	std::snprintf(buf, sizeof(buf), "CD2_1   = %20.12E", scaleDeg * std::sin(t) * m); s += card(buf);
	std::snprintf(buf, sizeof(buf), "CD2_2   = %20.12E", scaleDeg * std::cos(t)); s += card(buf);
	s += card("END");
	s.resize(2880, ' ');
	return s;
}

static void testFits()
{
	FitsHeader h;
	CHECK(!parseFitsHeader("not a fits file", h));
	CHECK(parseFitsHeader(fitsHeader(10.68, 41.27, 30., 1. / 3600., 6000, 4000, false), h));
	CHECK(h["OBJECT"] == "M 31 Panel 2");
	CHECK(h["NAXIS1"] == "6000");

	for (bool mirrored : {false, true})
	{
		CHECK(parseFitsHeader(fitsHeader(10.68, 41.27, 30., 1. / 3600., 6000, 4000, mirrored), h));
		ReferenceFrame ref;
		std::string error;
		CHECK(referenceFromFits(h, ref, error));
		CHECK(ref.hasWcs);
		CHECK(ref.width == 6000 && ref.height == 4000);
		CHECK(ref.object == "M 31 Panel 2");
		CHECK_NEAR(angDistDeg(ref.center, raDecToVec(10.68, 41.27)), 0., 1e-9);
		CHECK_NEAR(positionAngleOf(ref.center, ref.axisY), 30., 1e-6);
		CHECK_NEAR(ref.pixelScaleArcsec, 1., 1e-9);
		CHECK_NEAR(ref.fovXDeg, 2. * std::atan(0.5 * 6000. / 3600. * M_PI / 180.) * 180. / M_PI, 1e-9);
		CHECK_NEAR(ref.fovYDeg, 2. * std::atan(0.5 * 4000. / 3600. * M_PI / 180.) * 180. / M_PI, 1e-9);

		// The footprint is the panel of the same size, PA and center, with the corners wound the same way.
		// Mirroring swaps left and right, so the first corner may differ.
		MosaicParams p;
		p.raDeg = 10.68; p.decDeg = 41.27; p.paDeg = 30.;
		p.columns = p.rows = 1;
		p.fovXDeg = ref.fovXDeg; p.fovYDeg = ref.fovYDeg;
		const Panel panel = computePanels(p).front();
		const int shift = mirrored ? 3 : 0;
		for (int i = 0; i < 4; ++i)
			CHECK_NEAR(angDistDeg(ref.corners[static_cast<size_t>(i)], panel.corners[static_cast<size_t>((i + shift) % 4)]), 0., 1e-6);

		// A panel rotated by 180°, or in portrait orientation, uses the matching image axis
		const Frame same = referenceTargetFrame(ref, panel, p.fovXDeg, p.fovYDeg);
		CHECK_NEAR(positionAngleOf(same.center, same.up), 30., 1e-6);
		p.paDeg = 200.;
		const Frame flipped = referenceTargetFrame(ref, computePanels(p).front(), p.fovXDeg, p.fovYDeg);
		CHECK_NEAR(positionAngleOf(flipped.center, flipped.up), 210., 1e-6);
		p.paDeg = 100.;
		const Frame portrait = referenceTargetFrame(ref, computePanels(p).front(), p.fovYDeg, p.fovXDeg);
		CHECK_NEAR(paDiff(positionAngleOf(portrait.center, portrait.up), 120.), 0., 1e-6);
	}

	// CDELT/CROTA2 form, same orientation
	{
		std::string s = card("SIMPLE  = T") + card("NAXIS1  = 3000") + card("NAXIS2  = 2000") + card("CTYPE1  = 'RA---TAN'")
		                + card("CTYPE2  = 'DEC--TAN'") + card("CRVAL1  = 200.0") + card("CRVAL2  = -30.0")
		                + card("CRPIX1  = 1500.5") + card("CRPIX2  = 1000.5") + card("CDELT1  = -5.555555556D-4")
		                + card("CDELT2  = 5.555555556D-4") + card("CROTA2  = -20.0") + card("END");
		CHECK(parseFitsHeader(s, h));
		ReferenceFrame ref;
		std::string error;
		CHECK(referenceFromFits(h, ref, error));
		CHECK_NEAR(ref.pixelScaleArcsec, 2., 1e-6);
		// CROTA2 is the angle from north to the +y axis, counted towards the -x axis (east for CDELT1 < 0)
		CHECK_NEAR(positionAngleOf(ref.center, ref.axisY), 20., 1e-6);
	}

	// No WCS: pointing and optics
	{
		std::string s = card("SIMPLE  = T") + card("NAXIS1  = 6248") + card("NAXIS2  = 4176")
		                + card("OBJCTRA = '00 42 44.3'") + card("OBJCTDEC= '+41 16 09'") + card("FOCALLEN= 400.0")
		                + card("XPIXSZ  = 3.76") + card("END");
		CHECK(parseFitsHeader(s, h));
		ReferenceFrame ref;
		std::string error;
		CHECK(referenceFromFits(h, ref, error));
		CHECK(!ref.hasWcs);
		CHECK_NEAR(angDistDeg(ref.center, raDecToVec(15. * (42. / 60. + 44.3 / 3600.), 41. + 16. / 60. + 9. / 3600.)), 0., 1e-9);
		CHECK_NEAR(ref.pixelScaleArcsec, 3.76e-3 / 400. * 180. / M_PI * 3600., 1e-9);
		CHECK_NEAR(ref.fovXDeg, 2. * std::atan(0.5 * 6248. * 3.76e-3 / 400.) * 180. / M_PI, 1e-9);
	}

	// Neither
	{
		CHECK(parseFitsHeader(card("SIMPLE  = T") + card("NAXIS1  = 100") + card("NAXIS2  = 100") + card("END"), h));
		ReferenceFrame ref;
		std::string error;
		CHECK(!referenceFromFits(h, ref, error));
		CHECK(!error.empty());
	}
}

static void testAlign()
{
	MosaicParams p;
	p.raDeg = 83.82; p.decDeg = -5.39; p.paDeg = 10.;
	p.columns = 3; p.rows = 3;
	p.overlapX = p.overlapY = 0.1;
	p.fovXDeg = 2.; p.fovYDeg = 1.3;
	for (bool constant : {false, true})
	{
		for (double dec : {-5., 60., 85.})
		{
			p.constantRotation = constant;
			const Frame target = frameAt(raDecToVec(84.5, dec), 25.);
			const MosaicParams q = alignPanelTo(p, 5, target);
			const std::vector<Panel> panels = computePanels(q);
			CHECK_NEAR(angDistDeg(panels[5].center, target.center), 0., 1e-8);
			CHECK_NEAR(paDiff(panels[5].paDeg, 25.), 0., 1e-6);
			CHECK(q.columns == p.columns && q.rows == p.rows);
		}
	}
	CHECK(nearestPanel(computePanels(p), computePanels(p)[7].center) == 7);
	// Invalid index: unchanged
	const MosaicParams same = alignPanelTo(p, 9, frameAt(raDecToVec(0., 0.), 0.));
	CHECK(same.raDeg == p.raDeg && same.paDeg == p.paDeg);
}

int main()
{
	testParseAngle();
	testTelescopiusRoundTrip();
	testDecimalRoundTrip();
	testCsvErrors();
	testFits();
	testAlign();
	if (failures == 0)
		std::printf("All import tests passed\n");
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
