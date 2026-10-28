// Standalone tests for MosaicGeometry (no Stellarium/Qt needed).
// Copyright (C) 2026 Mario Keller, GNU GPL v2 or later.
#include "MosaicGeometry.hpp"

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

// Same rule as Stellarium's SphericalConvexPolygon::contains()
static bool stelConvexContains(const std::array<Vec3, 4>& c, const Vec3& p)
{
	for (int i = 0; i < 4; ++i)
	{
		const Vec3& a = c[(i + 1) % 4];
		const Vec3& b = c[i];
		if (a.cross(b).dot(p) < -1e-17)
			return false;
	}
	return true;
}

static void testBasics()
{
	double ra, dec;
	vecToRaDec(raDecToVec(83.82, -5.39), ra, dec);
	CHECK_NEAR(ra, 83.82, 1e-9);
	CHECK_NEAR(dec, -5.39, 1e-9);

	// At RA=0/Dec=0 with PA=0: up = north (+z), right = west (-y)
	const Frame f = frameAt(raDecToVec(0., 0.), 0.);
	CHECK_NEAR(f.up.z, 1., 1e-12);
	CHECK_NEAR(f.right.y, -1., 1e-12);
	// PA=90: up points east (+y)
	const Frame f90 = frameAt(raDecToVec(0., 0.), 90.);
	CHECK_NEAR(f90.up.y, 1., 1e-12);
	CHECK_NEAR(positionAngleOf(raDecToVec(0., 0.), f90.up), 90., 1e-9);

	CHECK_NEAR(normalizeDeg360(-30.), 330., 1e-12);
	CHECK_NEAR(normalizeDeg360(720.), 0., 1e-12);
}

static void testSinglePanel()
{
	MosaicParams p;
	p.raDeg = 10.68; p.decDeg = 41.27; p.paDeg = 33.;
	p.columns = 1; p.rows = 1;
	p.fovXDeg = 2.; p.fovYDeg = 1.5;
	const auto panels = computePanels(p);
	CHECK(panels.size() == 1);
	CHECK_NEAR(panels[0].raDeg, p.raDeg, 1e-9);
	CHECK_NEAR(panels[0].decDeg, p.decDeg, 1e-9);
	CHECK_NEAR(panels[0].paDeg, 33., 1e-9);
	CHECK_NEAR(mosaicWidthDeg(p), 2., 1e-9);
	CHECK_NEAR(mosaicHeightDeg(p), 1.5, 1e-9);
	// Top edge midpoint lies fovY/2 away from the center towards PA
	const Vec3 topMid = (panels[0].corners[0] + panels[0].corners[3]).normalized();
	CHECK_NEAR(angDistDeg(topMid, panels[0].center), 0.75, 1e-9);
	CHECK_NEAR(positionAngleOfPoint(panels[0].center, topMid), 33., 1e-6);
	// Corner order must match Stellarium's convex polygon convention
	CHECK(stelConvexContains(panels[0].corners, panels[0].center));
	CHECK(!stelConvexContains(panels[0].corners, raDecToVec(p.raDeg + 10., p.decDeg)));
}

static void testGrid()
{
	MosaicParams p;
	p.raDeg = 314.75; p.decDeg = 44.5; p.paDeg = 0.;
	p.columns = 3; p.rows = 2;
	p.overlapX = p.overlapY = 0.2;
	p.fovXDeg = 3.; p.fovYDeg = 2.;
	auto panels = computePanels(p);
	CHECK(panels.size() == 6);
	CHECK(panels[0].number == 1 && panels[0].row == 0 && panels[0].column == 0);
	CHECK(panels[5].number == 6 && panels[5].row == 1 && panels[5].column == 2);
	// Top-left panel at PA 0 is north-east of the center
	CHECK(panels[0].decDeg > p.decDeg);
	CHECK(panels[0].raDeg > p.raDeg);
	// Mosaic is symmetric around its center
	const Vec3 sum = panels[0].center + panels[5].center;
	CHECK_NEAR(angDistDeg(sum, raDecToVec(p.raDeg, p.decDeg)), 0., 1e-9);
	// Neighbouring panels overlap by roughly 20 %
	const double tanStep = tangentSize(3.) * 0.8;
	CHECK_NEAR(angDistDeg(panels[0].center, panels[1].center), 2. * std::atan(tanStep / 2.) * 180. / M_PI, 0.02);
	for (const auto& panel : panels)
		CHECK(stelConvexContains(panel.corners, panel.center));

	CHECK(mosaicContains(p, raDecToVec(p.raDeg, p.decDeg)));
	CHECK(!mosaicContains(p, raDecToVec(p.raDeg, p.decDeg + 5.)));
	CHECK(panelAt(panels, p, panels[4].center) == 4);
	CHECK(panelAt(panels, p, raDecToVec(p.raDeg + 90., p.decDeg)) == -1);

	// Aligned panels near the pole get different PAs, constant rotation keeps them equal
	p.decDeg = 80.;
	panels = computePanels(p);
	CHECK(std::fabs(panels[0].paDeg - panels[2].paDeg) > 1.);
	p.constantRotation = true;
	panels = computePanels(p);
	for (const auto& panel : panels)
		CHECK_NEAR(panel.paDeg, 0., 1e-9);
}

static void testFormatting()
{
	CHECK(formatRaTelescopius(304.) == "20hr 16' 00\"");
	CHECK(formatDecTelescopius(38. + 54. / 60. + 18. / 3600.) == "38\xC2\xBA 54' 18\"");
	CHECK(formatDecTelescopius(-(5. + 23. / 60. + 28. / 3600.)) == "-5\xC2\xBA 23' 28\"");
	CHECK(formatRaTelescopius(359.99999) == "00hr 00' 00\"");
	CHECK(formatDecTelescopius(-0.000001) == "0\xC2\xBA 00' 00\"");
}

int main()
{
	testBasics();
	testSinglePanel();
	testGrid();
	testFormatting();
	if (failures == 0)
		std::printf("All geometry tests passed\n");
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
