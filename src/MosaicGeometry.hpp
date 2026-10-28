/*
 * Mosaic Planner plug-in for Stellarium
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef MOSAICGEOMETRY_HPP
#define MOSAICGEOMETRY_HPP

#include <array>
#include <string>
#include <vector>

//! Pure geometry for mosaic planning. Has no dependency on Stellarium or Qt
//! so it can be unit tested on its own.
//!
//! Conventions:
//! - Coordinates are J2000 equatorial, unit vectors with x towards RA=0/Dec=0
//!   and z towards the north celestial pole.
//! - The position angle (PA) of a panel is the direction of the panel's "up"
//!   axis (sensor height), measured from north through east, in degrees.
//!   PA=0 means north up / east left, the sensor width runs east-west.
//! - Panels are laid out on the tangent (gnomonic) plane of the mosaic center,
//!   which is exactly the projection a flat camera sensor sees.
namespace mosaic
{

struct Vec3
{
	double x = 0., y = 0., z = 0.;

	Vec3() = default;
	Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

	Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
	Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
	Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
	double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
	Vec3 cross(const Vec3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
	double length() const;
	Vec3 normalized() const;
};

struct MosaicParams
{
	double raDeg = 0.;          //!< J2000 RA of the mosaic center
	double decDeg = 0.;         //!< J2000 Dec of the mosaic center
	double paDeg = 0.;          //!< position angle of the mosaic (east of north)
	int columns = 2;
	int rows = 2;
	double overlapX = 0.1;      //!< overlap fraction between columns, 0..<1
	double overlapY = 0.1;      //!< overlap fraction between rows, 0..<1
	double fovXDeg = 1.;        //!< panel field of view along the sensor width
	double fovYDeg = 1.;        //!< panel field of view along the sensor height
	//! If true every panel uses the same PA (the camera is never rotated during the session).
	//! If false each panel is rotated to stay aligned with the mosaic grid ("preserve alignment").
	bool constantRotation = false;
};

struct Panel
{
	int number = 0;             //!< 1-based, row-major from the top-left of the mosaic
	int row = 0;                //!< 0-based, 0 = top row (towards the mosaic "up" direction)
	int column = 0;             //!< 0-based, 0 = left column
	double raDeg = 0.;
	double decDeg = 0.;
	double paDeg = 0.;
	Vec3 center;
	Vec3 up;                    //!< panel "up" (sensor height) direction, tangent at center
	Vec3 right;                 //!< panel "right" (sensor width) direction, tangent at center
	//! Corners in order top-left, bottom-left, bottom-right, top-right.
	//! This is the vertex order SphericalConvexPolygon expects (verified in the tests).
	std::array<Vec3, 4> corners;
};

struct Frame
{
	Vec3 center;  //!< unit vector
	Vec3 up;      //!< unit vector tangent at center
	Vec3 right;   //!< unit vector tangent at center (= center x up)
};

Vec3 raDecToVec(double raDeg, double decDeg);
void vecToRaDec(const Vec3& v, double& raDeg, double& decDeg);

//! Local north and east unit vectors at the given position.
void localNorthEast(const Vec3& v, Vec3& north, Vec3& east);
//! Frame at position v whose up axis has the given position angle.
Frame frameAt(const Vec3& v, double paDeg);
//! Position angle (east of north) of the tangent direction dir at position v.
double positionAngleOf(const Vec3& v, const Vec3& dir);
//! Position angle (east of north) of point p as seen from center c.
double positionAngleOfPoint(const Vec3& c, const Vec3& p);

//! Size of the full panel in tangent-plane units (2*tan(fov/2)).
double tangentSize(double fovDeg);
//! Width/height of the whole mosaic, in degrees.
double mosaicWidthDeg(const MosaicParams& p);
double mosaicHeightDeg(const MosaicParams& p);

std::vector<Panel> computePanels(const MosaicParams& p);

//! Gnomonic projection of p onto the tangent plane of frame f. Returns false if p is on the far hemisphere.
bool projectToFrame(const Frame& f, const Vec3& p, double& x, double& y);
//! True if p lies inside the outer bounding rectangle of the mosaic.
bool mosaicContains(const MosaicParams& p, const Vec3& v);
//! Index (0-based) of the panel containing v, or -1. If panels overlap the panel whose center is closest wins.
int panelAt(const std::vector<Panel>& panels, const MosaicParams& p, const Vec3& v);

//! "20hr 16' 00\"" style RA as used by the Telescopius CSV export.
std::string formatRaTelescopius(double raDeg);
//! "38º 54' 18\"" style Dec as used by the Telescopius CSV export.
std::string formatDecTelescopius(double decDeg);

double normalizeDeg360(double deg);

} // namespace mosaic

#endif // MOSAICGEOMETRY_HPP
