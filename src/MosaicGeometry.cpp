/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "MosaicGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace mosaic
{

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDegToRad = kPi / 180.;
static constexpr double kRadToDeg = 180. / kPi;

double Vec3::length() const
{
	return std::sqrt(dot(*this));
}

Vec3 Vec3::normalized() const
{
	const double l = length();
	return l > 0. ? (*this) * (1. / l) : *this;
}

double normalizeDeg360(double deg)
{
	double r = std::fmod(deg, 360.);
	if (r < 0.)
		r += 360.;
	// Avoid returning 360 because of rounding
	return r >= 360. ? 0. : r;
}

Vec3 raDecToVec(double raDeg, double decDeg)
{
	const double ra = raDeg * kDegToRad;
	const double dec = decDeg * kDegToRad;
	return {std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};
}

void vecToRaDec(const Vec3& v, double& raDeg, double& decDeg)
{
	const Vec3 n = v.normalized();
	raDeg = normalizeDeg360(std::atan2(n.y, n.x) * kRadToDeg);
	decDeg = std::asin(std::clamp(n.z, -1., 1.)) * kRadToDeg;
}

void localNorthEast(const Vec3& v, Vec3& north, Vec3& east)
{
	const Vec3 n = v.normalized();
	const Vec3 pole(0., 0., 1.);
	east = pole.cross(n);
	if (east.length() < 1e-12)
	{
		// At a celestial pole north/east are undefined: pick RA=0 as reference.
		east = Vec3(0., 1., 0.);
	}
	east = east.normalized();
	north = n.cross(east).normalized();
}

Frame frameAt(const Vec3& v, double paDeg)
{
	Vec3 north, east;
	localNorthEast(v, north, east);
	const double pa = paDeg * kDegToRad;
	Frame f;
	f.center = v.normalized();
	f.up = (north * std::cos(pa) + east * std::sin(pa)).normalized();
	f.right = f.center.cross(f.up).normalized();
	return f;
}

double positionAngleOf(const Vec3& v, const Vec3& dir)
{
	Vec3 north, east;
	localNorthEast(v, north, east);
	return normalizeDeg360(std::atan2(dir.dot(east), dir.dot(north)) * kRadToDeg);
}

double positionAngleOfPoint(const Vec3& c, const Vec3& p)
{
	const Vec3 n = c.normalized();
	const Vec3 tangent = p - n * p.dot(n);
	return positionAngleOf(n, tangent);
}

double tangentSize(double fovDeg)
{
	return 2. * std::tan(0.5 * fovDeg * kDegToRad);
}

static double mosaicTangentWidth(const MosaicParams& p)
{
	const double w = tangentSize(p.fovXDeg);
	return w * (p.columns - (p.columns - 1) * p.overlapX);
}

static double mosaicTangentHeight(const MosaicParams& p)
{
	const double h = tangentSize(p.fovYDeg);
	return h * (p.rows - (p.rows - 1) * p.overlapY);
}

double mosaicWidthDeg(const MosaicParams& p)
{
	return 2. * std::atan(0.5 * mosaicTangentWidth(p)) * kRadToDeg;
}

double mosaicHeightDeg(const MosaicParams& p)
{
	return 2. * std::atan(0.5 * mosaicTangentHeight(p)) * kRadToDeg;
}

std::vector<Panel> computePanels(const MosaicParams& p)
{
	std::vector<Panel> panels;
	if (p.columns < 1 || p.rows < 1 || p.fovXDeg <= 0. || p.fovYDeg <= 0.)
		return panels;

	const Frame mosaicFrame = frameAt(raDecToVec(p.raDeg, p.decDeg), p.paDeg);
	const double w = tangentSize(p.fovXDeg);
	const double h = tangentSize(p.fovYDeg);
	const double stepX = w * (1. - p.overlapX);
	const double stepY = h * (1. - p.overlapY);

	panels.reserve(static_cast<size_t>(p.columns * p.rows));
	for (int row = 0; row < p.rows; ++row)
	{
		for (int col = 0; col < p.columns; ++col)
		{
			const double x = (col - 0.5 * (p.columns - 1)) * stepX;
			const double y = (0.5 * (p.rows - 1) - row) * stepY;

			Panel panel;
			panel.number = row * p.columns + col + 1;
			panel.row = row;
			panel.column = col;
			panel.center = (mosaicFrame.center + mosaicFrame.right * x + mosaicFrame.up * y).normalized();

			Frame f;
			if (p.constantRotation)
			{
				f = frameAt(panel.center, p.paDeg);
			}
			else
			{
				// Direction of the mosaic's "up" grid line through this panel center
				f.center = panel.center;
				f.up = (mosaicFrame.up - panel.center * panel.center.dot(mosaicFrame.up)).normalized();
				f.right = f.center.cross(f.up).normalized();
			}
			panel.up = f.up;
			panel.right = f.right;
			panel.paDeg = positionAngleOf(panel.center, f.up);
			vecToRaDec(panel.center, panel.raDeg, panel.decDeg);

			const Vec3 dx = f.right * (0.5 * w);
			const Vec3 dy = f.up * (0.5 * h);
			panel.corners[0] = (f.center - dx + dy).normalized();
			panel.corners[1] = (f.center - dx - dy).normalized();
			panel.corners[2] = (f.center + dx - dy).normalized();
			panel.corners[3] = (f.center + dx + dy).normalized();
			panels.push_back(panel);
		}
	}
	return panels;
}

bool projectToFrame(const Frame& f, const Vec3& p, double& x, double& y)
{
	const double d = p.dot(f.center);
	if (d <= 1e-9)
		return false;
	x = p.dot(f.right) / d;
	y = p.dot(f.up) / d;
	return true;
}

bool mosaicContains(const MosaicParams& p, const Vec3& v)
{
	const Frame f = frameAt(raDecToVec(p.raDeg, p.decDeg), p.paDeg);
	double x, y;
	if (!projectToFrame(f, v.normalized(), x, y))
		return false;
	return std::fabs(x) <= 0.5 * mosaicTangentWidth(p) && std::fabs(y) <= 0.5 * mosaicTangentHeight(p);
}

int panelAt(const std::vector<Panel>& panels, const MosaicParams& p, const Vec3& v)
{
	const double halfW = 0.5 * tangentSize(p.fovXDeg);
	const double halfH = 0.5 * tangentSize(p.fovYDeg);
	const Vec3 n = v.normalized();
	int best = -1;
	double bestDist = std::numeric_limits<double>::max();
	for (size_t i = 0; i < panels.size(); ++i)
	{
		Frame f{panels[i].center, panels[i].up, panels[i].right};
		double x, y;
		if (!projectToFrame(f, n, x, y))
			continue;
		if (std::fabs(x) > halfW || std::fabs(y) > halfH)
			continue;
		const double dist = x * x + y * y;
		if (dist < bestDist)
		{
			bestDist = dist;
			best = static_cast<int>(i);
		}
	}
	return best;
}

// Split a non-negative value into whole units, minutes and rounded seconds, carrying overflow.
static void splitSexagesimal(double value, int& units, int& minutes, int& seconds)
{
	long total = std::lround(value * 3600.);
	units = static_cast<int>(total / 3600);
	minutes = static_cast<int>((total / 60) % 60);
	seconds = static_cast<int>(total % 60);
}

std::string formatRaTelescopius(double raDeg)
{
	int h, m, s;
	splitSexagesimal(normalizeDeg360(raDeg) / 15., h, m, s);
	if (h >= 24)
		h -= 24;
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%02dhr %02d' %02d\"", h, m, s);
	return buf;
}

std::string formatDecTelescopius(double decDeg)
{
	int d, m, s;
	splitSexagesimal(std::fabs(decDeg), d, m, s);
	const bool negative = decDeg < 0. && (d != 0 || m != 0 || s != 0);
	char buf[32];
	// U+00BA (masculine ordinal indicator) is what Telescopius uses as the degree sign.
	std::snprintf(buf, sizeof(buf), "%s%d\xC2\xBA %02d' %02d\"", negative ? "-" : "", d, m, s);
	return buf;
}

} // namespace mosaic
