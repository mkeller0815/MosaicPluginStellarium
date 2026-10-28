/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef OCULARSBRIDGE_HPP
#define OCULARSBRIDGE_HPP

#include <QObject>
#include <QPointer>
#include <QStringList>

//! Snapshot of the camera framing currently selected in the Oculars plug-in.
struct OcularsFraming
{
	bool valid = false;
	QString telescopeName;
	QString ccdName;
	QString lensName;
	double focalLengthMm = 0.;   //!< effective focal length, including the lens multiplier
	double chipWidthMm = 0.;
	double chipHeightMm = 0.;
	int resolutionX = 0;
	int resolutionY = 0;
	int binningX = 1;
	int binningY = 1;
	double fovXDeg = 0.;
	double fovYDeg = 0.;
	double chipRotAngleDeg = 0.; //!< Oculars' sensor rotation (positive = clockwise on an N-up view)
	bool equatorialMount = true;
};

//! Reads the live telescope / sensor / lens lists and selection from the Oculars plug-in.
//! The Oculars module is only accessed through its public header and Qt's meta-object system;
//! the device lists are private members and are read through a standard-conforming accessor.
class OcularsBridge : public QObject
{
	Q_OBJECT
public:
	explicit OcularsBridge(QObject* parent = nullptr);

	//! Connect to the Oculars module. Safe to call several times.
	bool attach();
	bool isAvailable() const;

	OcularsFraming currentFraming() const;

	QStringList telescopeNames() const;
	QStringList ccdNames() const;
	QStringList lensNames() const;
	int selectedTelescopeIndex() const;
	int selectedCcdIndex() const;
	int selectedLensIndex() const;
	void selectTelescope(int index);
	void selectCcd(int index);
	void selectLens(int index);   //!< -1 = no lens

	//! Oculars sensor rotation of the selected CCD, in degrees.
	void setCcdRotation(double angleDeg);
	//! Enable or disable Oculars' CCD frame overlay.
	void setCcdFrameEnabled(bool enabled);

	//! Convert between Oculars' rotation angle and the position angle used by the mosaic.
	static double rotationToPositionAngle(double ccdRotationDeg);
	static double positionAngleToRotation(double paDeg);

signals:
	//! Emitted when the selection or the rotation in Oculars changes.
	void framingChanged();

private:
	QPointer<QObject> oculars;
};

#endif // OCULARSBRIDGE_HPP
