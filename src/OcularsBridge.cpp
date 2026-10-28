/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "OcularsBridge.hpp"

#include "StelApp.hpp"
#include "StelModuleMgr.hpp"
#include "Oculars.hpp"

#include <QtMath>

namespace
{
// Oculars keeps its device lists private and offers no getters for them.
// Explicit template instantiation is allowed to name private members, which gives
// us a well-defined way to read them without modifying the Oculars plug-in.
template <typename Tag, typename Tag::type Member>
struct PrivateAccess
{
	friend typename Tag::type privateMember(Tag) { return Member; }
};

struct CcdsTag { using type = QList<CCD*> Oculars::*; friend type privateMember(CcdsTag); };
struct TelescopesTag { using type = QList<Telescope*> Oculars::*; friend type privateMember(TelescopesTag); };
struct LensesTag { using type = QList<Lens*> Oculars::*; friend type privateMember(LensesTag); };

template struct PrivateAccess<CcdsTag, &Oculars::ccds>;
template struct PrivateAccess<TelescopesTag, &Oculars::telescopes>;
template struct PrivateAccess<LensesTag, &Oculars::lenses>;

template <typename T>
QStringList namesOf(const QList<T*>& list)
{
	QStringList names;
	for (const T* item : list)
		names << (item ? item->property("name").toString() : QString());
	return names;
}

template <typename T>
T* itemAt(const QList<T*>& list, int index)
{
	return (index >= 0 && index < list.size()) ? list.at(index) : nullptr;
}
} // namespace

OcularsBridge::OcularsBridge(QObject* parent)
	: QObject(parent)
{
}

bool OcularsBridge::attach()
{
	if (oculars)
		return true;
	StelModuleMgr& mgr = StelApp::getInstance().getModuleMgr();
	if (!mgr.isPluginLoaded("Oculars"))
		return false;
	Oculars* module = qobject_cast<Oculars*>(mgr.getModule("Oculars", true));
	if (!module)
		return false;
	oculars = module;

	connect(module, &Oculars::selectedCCDChanged, this, &OcularsBridge::framingChanged);
	connect(module, &Oculars::selectedTelescopeChanged, this, &OcularsBridge::framingChanged);
	connect(module, &Oculars::selectedLensChanged, this, &OcularsBridge::framingChanged);
	connect(module, &Oculars::selectedCCDRotationAngleChanged, this, &OcularsBridge::framingChanged);
	return true;
}

bool OcularsBridge::isAvailable() const
{
	return !oculars.isNull();
}

static Oculars* asOculars(const QPointer<QObject>& p)
{
	return static_cast<Oculars*>(p.data());
}

OcularsFraming OcularsBridge::currentFraming() const
{
	OcularsFraming f;
	Oculars* o = asOculars(oculars);
	if (!o)
		return f;

	const CCD* ccd = itemAt(o->*privateMember(CcdsTag()), o->getSelectedCCDIndex());
	const Telescope* telescope = itemAt(o->*privateMember(TelescopesTag()), o->getSelectedTelescopeIndex());
	const Lens* lens = itemAt(o->*privateMember(LensesTag()), o->getSelectedLensIndex());
	if (!ccd || !telescope)
		return f;

	const double multiplier = lens ? lens->property("multipler").toDouble() : 1.;
	f.telescopeName = telescope->property("name").toString();
	f.ccdName = ccd->property("name").toString();
	f.lensName = lens ? lens->property("name").toString() : QString();
	f.focalLengthMm = telescope->property("focalLength").toDouble() * (multiplier > 0. ? multiplier : 1.);
	f.chipWidthMm = ccd->property("chipWidth").toDouble();
	f.chipHeightMm = ccd->property("chipHeight").toDouble();
	f.resolutionX = ccd->property("resolutionX").toInt();
	f.resolutionY = ccd->property("resolutionY").toInt();
	f.binningX = qMax(1, ccd->property("binningX").toInt());
	f.binningY = qMax(1, ccd->property("binningY").toInt());
	f.chipRotAngleDeg = ccd->property("chipRotAngle").toDouble();
	f.equatorialMount = telescope->property("equatorial").toBool();
	if (f.focalLengthMm <= 0. || f.chipWidthMm <= 0. || f.chipHeightMm <= 0.)
		return f;

	// Same formula as CCD::getActualFOVx/y in Oculars
	f.fovXDeg = qRadiansToDegrees(2. * qAtan(f.chipWidthMm / (2. * f.focalLengthMm)));
	f.fovYDeg = qRadiansToDegrees(2. * qAtan(f.chipHeightMm / (2. * f.focalLengthMm)));
	f.valid = true;
	return f;
}

QStringList OcularsBridge::telescopeNames() const
{
	Oculars* o = asOculars(oculars);
	return o ? namesOf(o->*privateMember(TelescopesTag())) : QStringList();
}

QStringList OcularsBridge::ccdNames() const
{
	Oculars* o = asOculars(oculars);
	return o ? namesOf(o->*privateMember(CcdsTag())) : QStringList();
}

QStringList OcularsBridge::lensNames() const
{
	Oculars* o = asOculars(oculars);
	return o ? namesOf(o->*privateMember(LensesTag())) : QStringList();
}

int OcularsBridge::selectedTelescopeIndex() const
{
	Oculars* o = asOculars(oculars);
	return o ? o->getSelectedTelescopeIndex() : -1;
}

int OcularsBridge::selectedCcdIndex() const
{
	Oculars* o = asOculars(oculars);
	return o ? o->getSelectedCCDIndex() : -1;
}

int OcularsBridge::selectedLensIndex() const
{
	Oculars* o = asOculars(oculars);
	return o ? o->getSelectedLensIndex() : -1;
}

void OcularsBridge::selectTelescope(int index)
{
	if (oculars)
		oculars->setProperty("selectedTelescopeIndex", index);
}

void OcularsBridge::selectCcd(int index)
{
	if (oculars)
		oculars->setProperty("selectedCCDIndex", index);
}

void OcularsBridge::selectLens(int index)
{
	if (oculars)
		oculars->setProperty("selectedLensIndex", index);
}

void OcularsBridge::setCcdRotation(double angleDeg)
{
	if (oculars)
		oculars->setProperty("selectedCCDRotationAngle", angleDeg);
}

void OcularsBridge::setCcdFrameEnabled(bool enabled)
{
	if (oculars)
		oculars->setProperty("enableCCD", enabled);
}

// Oculars rotates the sensor frame's up axis from north towards west for positive angles
// (see Oculars::paintCCDBounds), while the position angle is measured from north towards east.
double OcularsBridge::rotationToPositionAngle(double ccdRotationDeg)
{
	double pa = std::fmod(-ccdRotationDeg, 360.);
	if (pa < 0.)
		pa += 360.;
	return pa;
}

double OcularsBridge::positionAngleToRotation(double paDeg)
{
	// Oculars accepts rotations in [-180, 180]
	double rot = std::fmod(-paDeg, 360.);
	if (rot > 180.)
		rot -= 360.;
	else if (rot < -180.)
		rot += 360.;
	return rot;
}
