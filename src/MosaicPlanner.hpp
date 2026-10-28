/*
 * Mosaic Planner plug-in for Stellarium
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef MOSAICPLANNER_HPP
#define MOSAICPLANNER_HPP

#include "MosaicGeometry.hpp"
#include "OcularsBridge.hpp"

#include "StelModule.hpp"
#include "StelPluginInterface.hpp"
#include "VecMath.hpp"

#include <QFont>
#include <QSettings>
#include <QTimer>

class StelButton;
class MosaicPlannerDialog;

/*! @defgroup mosaicPlanner Mosaic Planner plug-in
@{
Plans multi-panel mosaics for astrophotography, using the camera framing
(telescope, sensor, lens and rotation) selected in the Oculars plug-in.
@}
*/

//! @class MosaicPlanner
//! Main class of the Mosaic Planner plug-in.
//! @ingroup mosaicPlanner
class MosaicPlanner : public StelModule
{
	Q_OBJECT
	Q_PROPERTY(bool enabled              READ isEnabled             WRITE setEnabled             NOTIFY enabledChanged)
	Q_PROPERTY(double centerRA           READ getCenterRA           WRITE setCenterRA            NOTIFY mosaicChanged)
	Q_PROPERTY(double centerDec          READ getCenterDec          WRITE setCenterDec           NOTIFY mosaicChanged)
	Q_PROPERTY(double rotation           READ getRotation           WRITE setRotation            NOTIFY mosaicChanged)
	Q_PROPERTY(int columns               READ getColumns            WRITE setColumns             NOTIFY mosaicChanged)
	Q_PROPERTY(int rows                  READ getRows               WRITE setRows                NOTIFY mosaicChanged)
	Q_PROPERTY(double overlapH           READ getOverlapH           WRITE setOverlapH            NOTIFY mosaicChanged)
	Q_PROPERTY(double overlapV           READ getOverlapV           WRITE setOverlapV            NOTIFY mosaicChanged)
	Q_PROPERTY(bool useOculars           READ getUseOculars         WRITE setUseOculars          NOTIFY mosaicChanged)
	Q_PROPERTY(double manualFocalLength  READ getManualFocalLength  WRITE setManualFocalLength   NOTIFY mosaicChanged)
	Q_PROPERTY(double manualSensorWidth  READ getManualSensorWidth  WRITE setManualSensorWidth   NOTIFY mosaicChanged)
	Q_PROPERTY(double manualSensorHeight READ getManualSensorHeight WRITE setManualSensorHeight  NOTIFY mosaicChanged)
	Q_PROPERTY(bool syncRotation         READ getSyncRotation       WRITE setSyncRotation        NOTIFY mosaicChanged)
	Q_PROPERTY(bool constantRotation     READ getConstantRotation   WRITE setConstantRotation    NOTIFY mosaicChanged)
	Q_PROPERTY(bool editMode             READ getEditMode           WRITE setEditMode            NOTIFY displayChanged)
	Q_PROPERTY(bool showLabels           READ getShowLabels         WRITE setShowLabels          NOTIFY displayChanged)
	Q_PROPERTY(bool fillPanels           READ getFillPanels         WRITE setFillPanels          NOTIFY displayChanged)
	Q_PROPERTY(Vec3f lineColor           READ getLineColor          WRITE setLineColor           NOTIFY displayChanged)
	Q_PROPERTY(int selectedPanel         READ getSelectedPanel      WRITE setSelectedPanel       NOTIFY selectedPanelChanged)
	Q_PROPERTY(QString targetName        READ getTargetName         WRITE setTargetName          NOTIFY mosaicChanged)

public:
	MosaicPlanner();
	~MosaicPlanner() override;

	void init() override;
	void deinit() override;
	void draw(StelCore* core) override;
	double getCallOrder(StelModuleActionName actionName) const override;
	bool configureGui(bool show = true) override;
	void handleMouseClicks(class QMouseEvent* event) override;
	bool handleMouseMoves(int x, int y, Qt::MouseButtons b) override;

	bool isEnabled() const { return flagEnabled; }
	double getCenterRA() const { return centerRA; }
	double getCenterDec() const { return centerDec; }
	double getRotation() const { return rotation; }
	int getColumns() const { return columns; }
	int getRows() const { return rows; }
	double getOverlapH() const { return overlapH; }
	double getOverlapV() const { return overlapV; }
	bool getUseOculars() const { return useOculars; }
	double getManualFocalLength() const { return manualFocalLength; }
	double getManualSensorWidth() const { return manualSensorWidth; }
	double getManualSensorHeight() const { return manualSensorHeight; }
	bool getSyncRotation() const { return syncRotation; }
	bool getConstantRotation() const { return constantRotation; }
	bool getEditMode() const { return editMode; }
	bool getShowLabels() const { return showLabels; }
	bool getFillPanels() const { return fillPanels; }
	Vec3f getLineColor() const { return lineColor; }
	int getSelectedPanel() const { return selectedPanel; }
	QString getTargetName() const { return targetName; }

	OcularsBridge* getOcularsBridge() const { return bridge; }
	//! The panel field of view currently in use (from Oculars or the manual setup). Returns false if unknown.
	bool getPanelFov(double& fovXDeg, double& fovYDeg) const;
	//! Pixel scale in arcsec/pixel, or 0 if unknown.
	double getPixelScale() const;
	//! Human readable description of the framing source.
	QString getFramingDescription() const;
	mosaic::MosaicParams getParams() const;
	std::vector<mosaic::Panel> getPanels() const;

public slots:
	void setEnabled(bool b);
	void setCenterRA(double raDeg);
	void setCenterDec(double decDeg);
	void setCenter(double raDeg, double decDeg);
	void setRotation(double paDeg);
	void setColumns(int n);
	void setRows(int n);
	void setOverlapH(double percent);
	void setOverlapV(double percent);
	void setUseOculars(bool b);
	void setManualFocalLength(double mm);
	void setManualSensorWidth(double mm);
	void setManualSensorHeight(double mm);
	void setSyncRotation(bool b);
	void setConstantRotation(bool b);
	void setEditMode(bool b);
	void setShowLabels(bool b);
	void setFillPanels(bool b);
	void setLineColor(const Vec3f& c);
	void setSelectedPanel(int index);
	void setTargetName(const QString& name);

	//! Center the mosaic on the currently selected object. Returns false if nothing is selected.
	bool centerOnSelectedObject();
	//! Center the mosaic on the current view direction.
	void centerOnView();
	//! Point the view at the whole mosaic.
	void gotoMosaic();
	//! Point the view at the given panel (0-based index).
	void gotoPanel(int index);

	//! Export the panels in the CSV format of Telescopius (importable by N.I.N.A., ASIAIR, ...).
	bool exportTelescopiusCsv(const QString& path) const;
	//! Export the panels as CSV with decimal degrees.
	bool exportDecimalCsv(const QString& path) const;
	//! Save the plan (all mosaic parameters) to a JSON file.
	bool savePlan(const QString& path) const;
	//! Load a plan from a JSON file.
	bool loadPlan(const QString& path);

	//! Show or hide the Mosaic Planner window.
	void setDialogVisible(bool visible);

	void saveSettings();
	void restoreDefaultSettings();

signals:
	void enabledChanged(bool b);
	void mosaicChanged();
	void displayChanged();
	void selectedPanelChanged(int index);

private slots:
	void onOcularsFramingChanged();
	void pollOculars();

private:
	void loadSettings();
	void changed();
	Vec3d centerVector() const;
	void drawCross(class StelPainter& painter, const mosaic::Vec3& center, double size) const;

	QSettings* conf = nullptr;
	OcularsBridge* bridge = nullptr;
	MosaicPlannerDialog* dialog = nullptr;
	StelButton* toolbarButton = nullptr;
	QTimer pollTimer;
	QFont font;
	OcularsFraming lastFraming;

	bool flagEnabled = false;
	double centerRA = 83.8221;   // M42
	double centerDec = -5.3911;
	double rotation = 0.;
	int columns = 2;
	int rows = 2;
	double overlapH = 10.;
	double overlapV = 10.;
	bool useOculars = true;
	double manualFocalLength = 500.;
	double manualSensorWidth = 23.5;
	double manualSensorHeight = 15.7;
	bool syncRotation = false;
	bool constantRotation = false;
	bool editMode = false;
	bool showLabels = true;
	bool fillPanels = true;
	Vec3f lineColor = Vec3f(1.f, 0.55f, 0.f);
	int selectedPanel = -1;
	QString targetName;

	// Mouse dragging state
	bool dragging = false;
	bool dragRotate = false;
	bool dragMoved = false;
	mosaic::Vec3 dragStart;
	mosaic::Vec3 dragCenterStart;
	double dragRotationStart = 0.;
	bool pushingRotation = false;
};

//! This class is used by Qt to manage a plug-in interface
class MosaicPlannerStelPluginInterface : public QObject, public StelPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID StelPluginInterface_iid)
	Q_INTERFACES(StelPluginInterface)
public:
	StelModule* getStelModule() const override;
	StelPluginInfo getPluginInfo() const override;
};

#endif // MOSAICPLANNER_HPP
