/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "MosaicPlanner.hpp"
#include "gui/MosaicPlannerDialog.hpp"

#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelGui.hpp"
#include "StelGuiItems.hpp"
#include "StelModuleMgr.hpp"
#include "StelMovementMgr.hpp"
#include "StelObjectMgr.hpp"
#include "StelPainter.hpp"
#include "StelProjector.hpp"
#include "StelSphereGeometry.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"

#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextStream>
#include <QtMath>

#include <cmath>

// Normally set from the project version in CMakeLists.txt
#ifndef MOSAICPLANNER_VERSION
#define MOSAICPLANNER_VERSION "unknown"
#endif
// The Stellarium version ("major.minor") the plug-in was built for, set in CMakeLists.txt
#ifndef MOSAICPLANNER_STELLARIUM_VERSION
#error "MOSAICPLANNER_STELLARIUM_VERSION must be defined"
#endif

using mosaic::Vec3;

// Stellarium has no stable ABI: loading a plug-in built for another version crashes.
// Only plain exported functions are used here, which are safe to call in any version.
static bool isBuiltForRunningStellarium()
{
	return StelUtils::getApplicationPublicVersion() == QLatin1String(MOSAICPLANNER_STELLARIUM_VERSION);
}

StelModule* MosaicPlannerStelPluginInterface::getStelModule() const
{
	if (!isBuiltForRunningStellarium())
	{
		qWarning().noquote() << "[MosaicPlanner] This build of the plug-in is for Stellarium" << MOSAICPLANNER_STELLARIUM_VERSION
		                     << "but this is Stellarium" << StelUtils::getApplicationPublicVersion()
		                     << "- the plug-in is not loaded. Download the matching version from"
		                     << "https://github.com/mkeller0815/MosaicPluginStellarium/releases";
		// Stellarium skips plug-ins that return no module
		return nullptr;
	}
	return new MosaicPlanner();
}

StelPluginInfo MosaicPlannerStelPluginInterface::getPluginInfo() const
{
	StelPluginInfo info;
	info.id = "MosaicPlanner";
	info.displayedName = N_("Mosaic Planner");
	info.authors = "Mario Keller";
	info.contact = "mkl0815@googlemail.com";
	info.description = N_("Plan multi-panel mosaics for astrophotography directly on the sky. "
	                      "The panel size and rotation are taken from the telescope, sensor and lens "
	                      "selected in the Oculars plug-in (or from a manual setup). "
	                      "Panels can be moved and rotated with the mouse and exported as CSV in the "
	                      "Telescopius format for N.I.N.A., ASIAIR and other capture software.");
	if (!isBuiltForRunningStellarium())
		info.description = QString("<b>This build is for Stellarium %1 and will not be loaded into Stellarium %2. "
		                           "Please install the matching version of the plug-in.</b><br>")
		                           .arg(MOSAICPLANNER_STELLARIUM_VERSION, StelUtils::getApplicationPublicVersion())
		                   + info.description;
	info.version = QString("%1 (for Stellarium %2)").arg(MOSAICPLANNER_VERSION, MOSAICPLANNER_STELLARIUM_VERSION);
	info.license = "GNU GPLv2 or later";
	info.startByDefault = true;
	return info;
}

namespace
{
Vec3 toM(const Vec3d& v) { return Vec3(v[0], v[1], v[2]); }
Vec3d toStel(const Vec3& v) { return Vec3d(v.x, v.y, v.z); }

// Rotate v around the unit axis k by angle (Rodrigues' formula)
Vec3 rotateAround(const Vec3& v, const Vec3& k, double angle)
{
	const double c = std::cos(angle), s = std::sin(angle);
	return v * c + k.cross(v) * s + k * (k.dot(v) * (1. - c));
}

QPixmap makeToolbarIcon(bool on)
{
	const int size = 32;
	QPixmap pix(size, size);
	pix.fill(Qt::transparent);
	QPainter p(&pix);
	p.setRenderHint(QPainter::Antialiasing);
	const QColor color = on ? QColor(255, 255, 255) : QColor(150, 150, 150);
	p.translate(size / 2., size / 2.);
	p.rotate(-15.);
	p.setPen(QPen(color, 1.6));
	// 2x2 overlapping panels
	const double w = 12., h = 9.;
	for (int r = 0; r < 2; ++r)
		for (int c = 0; c < 2; ++c)
			p.drawRect(QRectF(-(2. * w - 3.) / 2. + c * (w - 3.), -(2. * h - 2.) / 2. + r * (h - 2.), w, h));
	return pix;
}

QJsonObject paramsToJson(const MosaicPlanner* m)
{
	QJsonObject o;
	o["version"] = 1;
	o["targetName"] = m->getTargetName();
	o["raDeg"] = m->getCenterRA();
	o["decDeg"] = m->getCenterDec();
	o["positionAngle"] = m->getRotation();
	o["columns"] = m->getColumns();
	o["rows"] = m->getRows();
	o["overlapH"] = m->getOverlapH();
	o["overlapV"] = m->getOverlapV();
	o["constantRotation"] = m->getConstantRotation();
	o["useOculars"] = m->getUseOculars();
	o["manualFocalLength"] = m->getManualFocalLength();
	o["manualSensorWidth"] = m->getManualSensorWidth();
	o["manualSensorHeight"] = m->getManualSensorHeight();
	double fx = 0., fy = 0.;
	if (m->getPanelFov(fx, fy))
	{
		o["panelFovXDeg"] = fx;
		o["panelFovYDeg"] = fy;
	}
	if (m->getUseOculars())
	{
		const OcularsFraming f = m->getOcularsBridge()->currentFraming();
		if (f.valid)
		{
			o["telescope"] = f.telescopeName;
			o["camera"] = f.ccdName;
			o["lens"] = f.lensName;
		}
	}
	return o;
}
} // namespace

MosaicPlanner::MosaicPlanner()
{
	setObjectName("MosaicPlanner");
	bridge = new OcularsBridge(this);
	connect(bridge, &OcularsBridge::framingChanged, this, &MosaicPlanner::onOcularsFramingChanged);
	font.setPixelSize(StelApp::getInstance().getScreenFontSize());
}

MosaicPlanner::~MosaicPlanner()
{
	delete dialog;
}

double MosaicPlanner::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName == StelModule::ActionDraw)
		return StelApp::getInstance().getModuleMgr().getModule("LandscapeMgr")->getCallOrder(actionName) + 10.;
	if (actionName == StelModule::ActionHandleMouseClicks || actionName == StelModule::ActionHandleMouseMoves)
		return -11.;
	return 0.;
}

void MosaicPlanner::init()
{
	conf = StelApp::getInstance().getSettings();
	if (!conf->childGroups().contains("MosaicPlanner"))
		restoreDefaultSettings();
	loadSettings();

	dialog = new MosaicPlannerDialog(this);

	addAction("actionShow_MosaicPlanner", N_("Mosaic Planner"), N_("Show mosaic"), "enabled");
	addAction("actionShow_MosaicPlanner_dialog", N_("Mosaic Planner"), N_("Mosaic Planner window"), dialog, "visible");
	addAction("actionMosaicPlanner_editMode", N_("Mosaic Planner"), N_("Move/rotate mosaic with the mouse"), "editMode");
	addAction("actionMosaicPlanner_centerOnSelection", N_("Mosaic Planner"), N_("Center mosaic on selected object"), "centerOnSelectedObject()");

	StelGui* gui = dynamic_cast<StelGui*>(StelApp::getInstance().getGui());
	if (gui)
	{
		toolbarButton = new StelButton(nullptr,
		                               makeToolbarIcon(true),
		                               makeToolbarIcon(false),
		                               QPixmap(":/graphicGui/miscGlow32x32.png"),
		                               "actionShow_MosaicPlanner_dialog",
		                               false,
		                               "actionShow_MosaicPlanner");
		gui->getButtonBar()->addButton(toolbarButton, "065-pluginsGroup");
	}

	connect(&StelApp::getInstance(), &StelApp::screenFontSizeChanged, this, [this](int size) { font.setPixelSize(size); });

	// The Oculars plug-in may be initialized after us, and some of its changes
	// (e.g. editing a sensor in its dialog) are not signalled. Poll it cheaply.
	pollTimer.setInterval(1000);
	connect(&pollTimer, &QTimer::timeout, this, &MosaicPlanner::pollOculars);
	pollTimer.start();
	QTimer::singleShot(0, this, &MosaicPlanner::pollOculars);
}

void MosaicPlanner::deinit()
{
	pollTimer.stop();
	saveSettings();
}

bool MosaicPlanner::configureGui(bool show)
{
	if (show)
		dialog->setVisible(true);
	return true;
}

void MosaicPlanner::setDialogVisible(bool visible)
{
	if (dialog)
		dialog->setVisible(visible);
}

/////////////////////////////////////////////////////////////////////////////
// Framing

bool MosaicPlanner::getPanelFov(double& fovXDeg, double& fovYDeg) const
{
	if (useOculars)
	{
		const OcularsFraming f = bridge->currentFraming();
		if (!f.valid)
			return false;
		fovXDeg = f.fovXDeg;
		fovYDeg = f.fovYDeg;
		return true;
	}
	if (manualFocalLength <= 0. || manualSensorWidth <= 0. || manualSensorHeight <= 0.)
		return false;
	fovXDeg = qRadiansToDegrees(2. * qAtan(manualSensorWidth / (2. * manualFocalLength)));
	fovYDeg = qRadiansToDegrees(2. * qAtan(manualSensorHeight / (2. * manualFocalLength)));
	return true;
}

double MosaicPlanner::getPixelScale() const
{
	if (!useOculars)
		return 0.;
	const OcularsFraming f = bridge->currentFraming();
	if (!f.valid || f.resolutionX <= 0)
		return 0.;
	const double pixelMm = f.chipWidthMm * f.binningX / f.resolutionX;
	return qRadiansToDegrees(2. * qAtan(pixelMm / (2. * f.focalLengthMm))) * 3600.;
}

QString MosaicPlanner::getFramingDescription() const
{
	if (!useOculars)
		return QString("%1 mm, %2 × %3 mm").arg(manualFocalLength).arg(manualSensorWidth).arg(manualSensorHeight);
	if (!bridge->isAvailable())
		return q_("Oculars plug-in is not loaded");
	const OcularsFraming f = bridge->currentFraming();
	if (!f.valid)
		return q_("Select a telescope and a sensor in the Oculars plug-in");
	QString s = QString("%1 + %2").arg(f.telescopeName, f.ccdName);
	if (!f.lensName.isEmpty())
		s += QString(" + %1").arg(f.lensName);
	return s;
}

mosaic::MosaicParams MosaicPlanner::getParams() const
{
	mosaic::MosaicParams p;
	p.raDeg = centerRA;
	p.decDeg = centerDec;
	p.paDeg = rotation;
	p.columns = columns;
	p.rows = rows;
	p.overlapX = overlapH / 100.;
	p.overlapY = overlapV / 100.;
	p.constantRotation = constantRotation;
	if (!getPanelFov(p.fovXDeg, p.fovYDeg))
		p.fovXDeg = p.fovYDeg = 0.;
	return p;
}

std::vector<mosaic::Panel> MosaicPlanner::getPanels() const
{
	return mosaic::computePanels(getParams());
}

Vec3d MosaicPlanner::centerVector() const
{
	return toStel(mosaic::raDecToVec(centerRA, centerDec));
}

void MosaicPlanner::onOcularsFramingChanged()
{
	if (useOculars && syncRotation && !pushingRotation)
	{
		const OcularsFraming f = bridge->currentFraming();
		if (f.valid)
		{
			const double pa = OcularsBridge::rotationToPositionAngle(f.chipRotAngleDeg);
			if (std::fabs(pa - rotation) > 1e-6)
				rotation = pa;
		}
	}
	lastFraming = bridge->currentFraming();
	changed();
}

void MosaicPlanner::pollOculars()
{
	if (!bridge->isAvailable())
	{
		if (!bridge->attach())
			return;
		onOcularsFramingChanged();
		return;
	}
	const OcularsFraming f = bridge->currentFraming();
	if (f.valid != lastFraming.valid || f.fovXDeg != lastFraming.fovXDeg || f.fovYDeg != lastFraming.fovYDeg
	    || f.chipRotAngleDeg != lastFraming.chipRotAngleDeg || f.ccdName != lastFraming.ccdName
	    || f.telescopeName != lastFraming.telescopeName || f.lensName != lastFraming.lensName
	    || f.resolutionX != lastFraming.resolutionX || f.binningX != lastFraming.binningX)
		onOcularsFramingChanged();
}

/////////////////////////////////////////////////////////////////////////////
// Drawing

void MosaicPlanner::drawCross(StelPainter& painter, const Vec3& center, double size) const
{
	const mosaic::Frame f = mosaic::frameAt(center, rotation);
	const double t = std::tan(qDegreesToRadians(size));
	painter.drawGreatCircleArc(toStel((f.center + f.right * t).normalized()), toStel((f.center - f.right * t).normalized()));
	painter.drawGreatCircleArc(toStel((f.center + f.up * t).normalized()), toStel((f.center - f.up * t).normalized()));
}

void MosaicPlanner::draw(StelCore* core)
{
	if (!flagEnabled)
		return;
	const mosaic::MosaicParams params = getParams();
	if (params.fovXDeg <= 0.)
		return;
	const std::vector<mosaic::Panel> panels = mosaic::computePanels(params);

	const StelProjectorP prj = core->getProjection(StelCore::FrameJ2000);
	StelPainter painter(prj);
	painter.setFont(font);
	painter.setBlending(true);
	painter.setLineSmooth(true);
	const float ppx = static_cast<float>(prj->getDevicePixelsPerPixel());
	const QFontMetrics fm = painter.getFontMetrics();

	for (size_t i = 0; i < panels.size(); ++i)
	{
		const mosaic::Panel& panel = panels[i];
		const bool selected = static_cast<int>(i) == selectedPanel;
		if (fillPanels || selected)
		{
			SphericalConvexPolygon poly(toStel(panel.corners[0]), toStel(panel.corners[1]),
			                            toStel(panel.corners[2]), toStel(panel.corners[3]));
			painter.setColor(lineColor, selected ? 0.28f : 0.07f);
			painter.drawSphericalRegion(&poly, StelPainter::SphericalPolygonDrawModeFill);
		}
	}

	painter.setLineWidth((editMode ? 2.f : 1.5f) * ppx);
	for (size_t i = 0; i < panels.size(); ++i)
	{
		const mosaic::Panel& panel = panels[i];
		const bool selected = static_cast<int>(i) == selectedPanel;
		painter.setColor(lineColor, selected ? 1.f : 0.85f);
		for (int k = 0; k < 4; ++k)
			painter.drawGreatCircleArc(toStel(panel.corners[k]), toStel(panel.corners[(k + 1) % 4]));
	}
	painter.setLineWidth(1.f);

	if (showLabels)
	{
		painter.setColor(lineColor, 1.f);
		for (const mosaic::Panel& panel : panels)
		{
			const QString text = QString::number(panel.number);
			const float w = static_cast<float>(fm.boundingRect(text).width());
			const float h = static_cast<float>(fm.ascent());
			painter.drawText(toStel(panel.center), text, 0.f, -0.5f * w, -0.4f * h);
		}
		if (!panels.empty())
		{
			QString title = QString("%1 × %2 — %3' × %4'")
			                    .arg(columns).arg(rows)
			                    .arg(mosaic::mosaicWidthDeg(params) * 60., 0, 'f', 0)
			                    .arg(mosaic::mosaicHeightDeg(params) * 60., 0, 'f', 0);
			if (!targetName.isEmpty())
				title = targetName + "  " + title;
			// Write the title along the top edge of the top-left panel, outside of the mosaic
			const mosaic::Panel& first = panels.front();
			Vec3d topLeft, topRight;
			if (prj->project(toStel(first.corners[0]), topLeft) && prj->project(toStel(first.corners[3]), topRight))
			{
				float angle = static_cast<float>(qRadiansToDegrees(std::atan2(topRight[1] - topLeft[1], topRight[0] - topLeft[0])));
				const float gap = 4.f * ppx;
				if (std::fabs(angle) <= 90.f)
					painter.drawText(static_cast<float>(topLeft[0]), static_cast<float>(topLeft[1]), title, angle, gap, gap);
				else
				{
					// Keep the text readable when the mosaic is upside down on screen
					angle += 180.f;
					painter.drawText(static_cast<float>(topRight[0]), static_cast<float>(topRight[1]), title, angle,
					                 gap, -gap - static_cast<float>(fm.ascent()));
				}
			}
		}
	}

	painter.setColor(lineColor, 1.f);
	drawCross(painter, mosaic::raDecToVec(centerRA, centerDec), 0.04 * std::min(params.fovXDeg, params.fovYDeg));
}

/////////////////////////////////////////////////////////////////////////////
// Mouse interaction

void MosaicPlanner::handleMouseClicks(QMouseEvent* event)
{
	if (!flagEnabled || !editMode)
		return;
	const StelProjectorP prj = StelApp::getInstance().getCore()->getProjection(StelCore::FrameJ2000);
	const qreal x = event->position().x(), y = event->position().y();

	if (event->type() == QEvent::MouseButtonPress && event->button() == Qt::LeftButton)
	{
		Vec3d v;
		if (!prj->unProject(x, y, v))
			return;
		v.normalize();
		const mosaic::MosaicParams params = getParams();
		if (params.fovXDeg <= 0. || !mosaic::mosaicContains(params, toM(v)))
			return;
		dragging = true;
		dragMoved = false;
		dragRotate = event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier);
		dragStart = toM(v);
		dragCenterStart = mosaic::raDecToVec(centerRA, centerDec);
		dragRotationStart = rotation;
		event->setAccepted(true);
		return;
	}
	if (event->type() == QEvent::MouseButtonRelease && event->button() == Qt::LeftButton && dragging)
	{
		dragging = false;
		if (!dragMoved)
		{
			// A click without movement selects the panel under the cursor
			const mosaic::MosaicParams params = getParams();
			const int index = mosaic::panelAt(mosaic::computePanels(params), params, dragStart);
			setSelectedPanel(index == selectedPanel ? -1 : index);
		}
		saveSettings();
		event->setAccepted(true);
	}
}

bool MosaicPlanner::handleMouseMoves(int x, int y, Qt::MouseButtons b)
{
	if (!dragging)
		return false;
	if (!(b & Qt::LeftButton))
	{
		dragging = false;
		return false;
	}
	const StelProjectorP prj = StelApp::getInstance().getCore()->getProjection(StelCore::FrameJ2000);
	Vec3d v;
	if (!prj->unProject(x, y, v))
		return true;
	v.normalize();
	const Vec3 current = toM(v);
	dragMoved = true;

	if (dragRotate)
	{
		const double start = mosaic::positionAngleOfPoint(dragCenterStart, dragStart);
		const double now = mosaic::positionAngleOfPoint(dragCenterStart, current);
		setRotation(dragRotationStart + now - start);
	}
	else
	{
		Vec3 axis = dragStart.cross(current);
		const double s = axis.length();
		if (s < 1e-12)
			return true;
		axis = axis * (1. / s);
		const double angle = std::atan2(s, dragStart.dot(current));
		double ra, dec;
		mosaic::vecToRaDec(rotateAround(dragCenterStart, axis, angle), ra, dec);
		setCenter(ra, dec);
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Setters

void MosaicPlanner::changed()
{
	emit mosaicChanged();
}

void MosaicPlanner::setEnabled(bool b)
{
	if (b == flagEnabled)
		return;
	flagEnabled = b;
	if (conf)
		conf->setValue("MosaicPlanner/enabled", b);
	emit enabledChanged(b);
}

void MosaicPlanner::setCenterRA(double raDeg)
{
	setCenter(raDeg, centerDec);
}

void MosaicPlanner::setCenterDec(double decDeg)
{
	setCenter(centerRA, decDeg);
}

void MosaicPlanner::setCenter(double raDeg, double decDeg)
{
	raDeg = mosaic::normalizeDeg360(raDeg);
	decDeg = qBound(-90., decDeg, 90.);
	if (raDeg == centerRA && decDeg == centerDec)
		return;
	centerRA = raDeg;
	centerDec = decDeg;
	changed();
}

void MosaicPlanner::setRotation(double paDeg)
{
	paDeg = mosaic::normalizeDeg360(paDeg);
	if (paDeg == rotation)
		return;
	rotation = paDeg;
	if (useOculars && syncRotation && bridge->isAvailable())
	{
		pushingRotation = true;
		bridge->setCcdRotation(OcularsBridge::positionAngleToRotation(rotation));
		lastFraming = bridge->currentFraming();
		pushingRotation = false;
	}
	changed();
}

void MosaicPlanner::setColumns(int n)
{
	n = qBound(1, n, 50);
	if (n == columns)
		return;
	columns = n;
	selectedPanel = -1;
	changed();
}

void MosaicPlanner::setRows(int n)
{
	n = qBound(1, n, 50);
	if (n == rows)
		return;
	rows = n;
	selectedPanel = -1;
	changed();
}

void MosaicPlanner::setOverlapH(double percent)
{
	percent = qBound(0., percent, 90.);
	if (percent == overlapH)
		return;
	overlapH = percent;
	changed();
}

void MosaicPlanner::setOverlapV(double percent)
{
	percent = qBound(0., percent, 90.);
	if (percent == overlapV)
		return;
	overlapV = percent;
	changed();
}

void MosaicPlanner::setUseOculars(bool b)
{
	if (b == useOculars)
		return;
	useOculars = b;
	if (useOculars && syncRotation)
		onOcularsFramingChanged();
	changed();
}

void MosaicPlanner::setManualFocalLength(double mm)
{
	if (mm == manualFocalLength)
		return;
	manualFocalLength = mm;
	changed();
}

void MosaicPlanner::setManualSensorWidth(double mm)
{
	if (mm == manualSensorWidth)
		return;
	manualSensorWidth = mm;
	changed();
}

void MosaicPlanner::setManualSensorHeight(double mm)
{
	if (mm == manualSensorHeight)
		return;
	manualSensorHeight = mm;
	changed();
}

void MosaicPlanner::setSyncRotation(bool b)
{
	if (b == syncRotation)
		return;
	syncRotation = b;
	// Adopt the rotation currently set in Oculars
	if (syncRotation)
		onOcularsFramingChanged();
	changed();
}

void MosaicPlanner::setConstantRotation(bool b)
{
	if (b == constantRotation)
		return;
	constantRotation = b;
	changed();
}

void MosaicPlanner::setEditMode(bool b)
{
	if (b == editMode)
		return;
	editMode = b;
	dragging = false;
	emit displayChanged();
}

void MosaicPlanner::setShowLabels(bool b)
{
	if (b == showLabels)
		return;
	showLabels = b;
	emit displayChanged();
}

void MosaicPlanner::setFillPanels(bool b)
{
	if (b == fillPanels)
		return;
	fillPanels = b;
	emit displayChanged();
}

void MosaicPlanner::setLineColor(const Vec3f& c)
{
	if (c == lineColor)
		return;
	lineColor = c;
	emit displayChanged();
}

void MosaicPlanner::setSelectedPanel(int index)
{
	if (index < -1 || index >= columns * rows)
		index = -1;
	if (index == selectedPanel)
		return;
	selectedPanel = index;
	emit selectedPanelChanged(index);
}

void MosaicPlanner::setTargetName(const QString& name)
{
	if (name == targetName)
		return;
	targetName = name;
	changed();
}

/////////////////////////////////////////////////////////////////////////////
// Actions

bool MosaicPlanner::centerOnSelectedObject()
{
	StelCore* core = StelApp::getInstance().getCore();
	const QList<StelObjectP> selection = GETSTELMODULE(StelObjectMgr)->getSelectedObject();
	if (selection.isEmpty())
		return false;
	const StelObjectP obj = selection.first();
	double ra, dec;
	StelUtils::rectToSphe(&ra, &dec, obj->getJ2000EquatorialPos(core));
	QString name = obj->getNameI18n();
	if (name.isEmpty())
		name = obj->getID();
	setTargetName(name);
	setCenter(qRadiansToDegrees(ra), qRadiansToDegrees(dec));
	return true;
}

void MosaicPlanner::centerOnView()
{
	const StelMovementMgr* mvmgr = StelApp::getInstance().getCore()->getMovementMgr();
	double ra, dec;
	StelUtils::rectToSphe(&ra, &dec, mvmgr->getViewDirectionJ2000());
	setCenter(qRadiansToDegrees(ra), qRadiansToDegrees(dec));
}

void MosaicPlanner::gotoMosaic()
{
	StelMovementMgr* mvmgr = StelApp::getInstance().getCore()->getMovementMgr();
	const mosaic::MosaicParams params = getParams();
	mvmgr->setFlagTracking(false);
	mvmgr->moveToJ2000(centerVector(), mvmgr->mountFrameToJ2000(Vec3d(0., 0., 1.)), mvmgr->getAutoMoveDuration());
	if (params.fovXDeg > 0.)
		mvmgr->zoomTo(1.5 * std::max(mosaic::mosaicWidthDeg(params), mosaic::mosaicHeightDeg(params)), mvmgr->getAutoMoveDuration());
}

void MosaicPlanner::gotoPanel(int index)
{
	const std::vector<mosaic::Panel> panels = getPanels();
	if (index < 0 || index >= static_cast<int>(panels.size()))
		return;
	setSelectedPanel(index);
	StelMovementMgr* mvmgr = StelApp::getInstance().getCore()->getMovementMgr();
	const mosaic::MosaicParams params = getParams();
	mvmgr->setFlagTracking(false);
	mvmgr->moveToJ2000(toStel(panels[index].center), mvmgr->mountFrameToJ2000(Vec3d(0., 0., 1.)), mvmgr->getAutoMoveDuration());
	mvmgr->zoomTo(2. * std::max(params.fovXDeg, params.fovYDeg), mvmgr->getAutoMoveDuration());
}

/////////////////////////////////////////////////////////////////////////////
// Import / export

bool MosaicPlanner::exportTelescopiusCsv(const QString& path) const
{
	const mosaic::MosaicParams params = getParams();
	if (params.fovXDeg <= 0.)
		return false;
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
		return false;
	QTextStream out(&file);
	out.setEncoding(QStringConverter::Utf8);

	const QString width = QString::number(params.fovXDeg * 60., 'f', 2);
	const QString height = QString::number(params.fovYDeg * 60., 'f', 2);
	// Telescopius only knows a single overlap value
	const QString overlap = QString("%1%").arg(qRound(overlapH));
	out << "Pane, RA, DEC, Position Angle (East), Pane width (arcmins), Pane height (arcmins), Overlap, Row, Column\n";
	out << "Center, " << QString::fromStdString(mosaic::formatRaTelescopius(params.raDeg)) << ", "
	    << QString::fromStdString(mosaic::formatDecTelescopius(params.decDeg)) << ", "
	    << QString::number(params.paDeg, 'f', 2) << ", " << width << ", " << height << ", " << overlap << ", -, -\n";
	for (const mosaic::Panel& panel : mosaic::computePanels(params))
	{
		out << "Pane " << panel.number << ", "
		    << QString::fromStdString(mosaic::formatRaTelescopius(panel.raDeg)) << ", "
		    << QString::fromStdString(mosaic::formatDecTelescopius(panel.decDeg)) << ", "
		    << QString::number(panel.paDeg, 'f', 2) << ", " << width << ", " << height << ", " << overlap << ", "
		    << panel.row + 1 << ", " << panel.column + 1 << "\n";
	}
	return true;
}

bool MosaicPlanner::exportDecimalCsv(const QString& path) const
{
	const mosaic::MosaicParams params = getParams();
	if (params.fovXDeg <= 0.)
		return false;
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
		return false;
	QTextStream out(&file);
	out.setEncoding(QStringConverter::Utf8);
	const QString name = targetName.isEmpty() ? QString("Mosaic") : targetName;
	out << "name,panel,row,column,ra_deg,dec_deg,ra_hms,dec_dms,position_angle_deg,fov_x_arcmin,fov_y_arcmin\n";
	for (const mosaic::Panel& panel : mosaic::computePanels(params))
	{
		out << '"' << QString(name).replace('"', "\"\"") << " P" << panel.number << "\","
		    << panel.number << ',' << panel.row + 1 << ',' << panel.column + 1 << ','
		    << QString::number(panel.raDeg, 'f', 6) << ',' << QString::number(panel.decDeg, 'f', 6) << ','
		    << StelUtils::radToHmsStr(qDegreesToRadians(panel.raDeg), true).trimmed() << ','
		    << StelUtils::radToDmsStr(qDegreesToRadians(panel.decDeg), true).trimmed() << ','
		    << QString::number(panel.paDeg, 'f', 2) << ','
		    << QString::number(params.fovXDeg * 60., 'f', 2) << ',' << QString::number(params.fovYDeg * 60., 'f', 2) << "\n";
	}
	return true;
}

bool MosaicPlanner::savePlan(const QString& path) const
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	file.write(QJsonDocument(paramsToJson(this)).toJson(QJsonDocument::Indented));
	return true;
}

bool MosaicPlanner::loadPlan(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
	if (!doc.isObject())
		return false;
	const QJsonObject o = doc.object();
	if (!o.contains("raDeg") || !o.contains("decDeg"))
		return false;

	setTargetName(o.value("targetName").toString());
	setColumns(o.value("columns").toInt(columns));
	setRows(o.value("rows").toInt(rows));
	setOverlapH(o.value("overlapH").toDouble(overlapH));
	setOverlapV(o.value("overlapV").toDouble(overlapV));
	setConstantRotation(o.value("constantRotation").toBool(constantRotation));
	setManualFocalLength(o.value("manualFocalLength").toDouble(manualFocalLength));
	setManualSensorWidth(o.value("manualSensorWidth").toDouble(manualSensorWidth));
	setManualSensorHeight(o.value("manualSensorHeight").toDouble(manualSensorHeight));
	setUseOculars(o.value("useOculars").toBool(useOculars));
	if (useOculars && bridge->isAvailable())
	{
		// Restore the equipment by name if it still exists in Oculars
		const int telescope = bridge->telescopeNames().indexOf(o.value("telescope").toString());
		const int camera = bridge->ccdNames().indexOf(o.value("camera").toString());
		if (telescope >= 0)
			bridge->selectTelescope(telescope);
		if (camera >= 0)
			bridge->selectCcd(camera);
		const QString lens = o.value("lens").toString();
		bridge->selectLens(lens.isEmpty() ? -1 : bridge->lensNames().indexOf(lens));
	}
	setCenter(o.value("raDeg").toDouble(), o.value("decDeg").toDouble());
	setRotation(o.value("positionAngle").toDouble(rotation));
	saveSettings();
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// Settings

void MosaicPlanner::restoreDefaultSettings()
{
	conf->remove("MosaicPlanner");
	conf->beginGroup("MosaicPlanner");
	conf->setValue("enabled", false);
	conf->setValue("center_ra", 83.8221);
	conf->setValue("center_dec", -5.3911);
	conf->setValue("target_name", "M 42");
	conf->setValue("rotation", 0.);
	conf->setValue("columns", 2);
	conf->setValue("rows", 2);
	conf->setValue("overlap_h", 10.);
	conf->setValue("overlap_v", 10.);
	conf->setValue("use_oculars", true);
	conf->setValue("manual_focal_length", 500.);
	conf->setValue("manual_sensor_width", 23.5);
	conf->setValue("manual_sensor_height", 15.7);
	conf->setValue("sync_rotation", false);
	conf->setValue("constant_rotation", false);
	conf->setValue("show_labels", true);
	conf->setValue("fill_panels", true);
	conf->setValue("line_color", "1.0,0.55,0.0");
	conf->endGroup();
}

void MosaicPlanner::loadSettings()
{
	conf->beginGroup("MosaicPlanner");
	flagEnabled = conf->value("enabled", false).toBool();
	centerRA = conf->value("center_ra", centerRA).toDouble();
	centerDec = conf->value("center_dec", centerDec).toDouble();
	targetName = conf->value("target_name", targetName).toString();
	rotation = conf->value("rotation", rotation).toDouble();
	columns = qBound(1, conf->value("columns", columns).toInt(), 50);
	rows = qBound(1, conf->value("rows", rows).toInt(), 50);
	overlapH = conf->value("overlap_h", overlapH).toDouble();
	overlapV = conf->value("overlap_v", overlapV).toDouble();
	useOculars = conf->value("use_oculars", useOculars).toBool();
	manualFocalLength = conf->value("manual_focal_length", manualFocalLength).toDouble();
	manualSensorWidth = conf->value("manual_sensor_width", manualSensorWidth).toDouble();
	manualSensorHeight = conf->value("manual_sensor_height", manualSensorHeight).toDouble();
	syncRotation = conf->value("sync_rotation", syncRotation).toBool();
	constantRotation = conf->value("constant_rotation", constantRotation).toBool();
	showLabels = conf->value("show_labels", showLabels).toBool();
	fillPanels = conf->value("fill_panels", fillPanels).toBool();
	lineColor = Vec3f(conf->value("line_color", "1.0,0.55,0.0").toString());
	conf->endGroup();
}

void MosaicPlanner::saveSettings()
{
	if (!conf)
		return;
	conf->beginGroup("MosaicPlanner");
	conf->setValue("enabled", flagEnabled);
	conf->setValue("center_ra", centerRA);
	conf->setValue("center_dec", centerDec);
	conf->setValue("target_name", targetName);
	conf->setValue("rotation", rotation);
	conf->setValue("columns", columns);
	conf->setValue("rows", rows);
	conf->setValue("overlap_h", overlapH);
	conf->setValue("overlap_v", overlapV);
	conf->setValue("use_oculars", useOculars);
	conf->setValue("manual_focal_length", manualFocalLength);
	conf->setValue("manual_sensor_width", manualSensorWidth);
	conf->setValue("manual_sensor_height", manualSensorHeight);
	conf->setValue("sync_rotation", syncRotation);
	conf->setValue("constant_rotation", constantRotation);
	conf->setValue("show_labels", showLabels);
	conf->setValue("fill_panels", fillPanels);
	conf->setValue("line_color", lineColor.toStr());
	conf->endGroup();
}
