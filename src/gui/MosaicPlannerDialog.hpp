/*
 * Mosaic Planner plug-in for Stellarium
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef MOSAICPLANNERDIALOG_HPP
#define MOSAICPLANNERDIALOG_HPP

#include "StelDialog.hpp"

#include <QTimer>

class MosaicPlanner;
class AngleSpinBox;
class TitleBar;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTableWidget;
class QWidget;

//! Main window of the Mosaic Planner plug-in. The widgets are built in code.
class MosaicPlannerDialog : public StelDialog
{
	Q_OBJECT
public:
	explicit MosaicPlannerDialog(MosaicPlanner* plugin);
	~MosaicPlannerDialog() override;

public slots:
	void retranslate() override;

protected:
	void createDialogContent() override;

private slots:
	void scheduleUpdate();
	void updateFromModule();
	void updateEquipmentLists();
	void updatePanelTable();
	void updateSelection(int index);
	void exportTelescopius();
	void exportDecimal();
	void savePlan();
	void loadPlan();
	void chooseColor();
	void gotoSelectedPanel();

private:
	QString askSaveFile(const QString& title, const QString& suffix, const QString& filter);
	void updateColorButton();

	MosaicPlanner* plugin;
	QTimer updateTimer;
	bool tableDirty = true;

	TitleBar* titleBar = nullptr;
	QLineEdit* targetNameEdit = nullptr;
	AngleSpinBox* raSpin = nullptr;
	AngleSpinBox* decSpin = nullptr;

	QRadioButton* ocularsRadio = nullptr;
	QRadioButton* manualRadio = nullptr;
	QWidget* ocularsBox = nullptr;
	QWidget* manualBox = nullptr;
	QComboBox* telescopeCombo = nullptr;
	QComboBox* ccdCombo = nullptr;
	QComboBox* lensCombo = nullptr;
	QDoubleSpinBox* focalSpin = nullptr;
	QDoubleSpinBox* sensorWidthSpin = nullptr;
	QDoubleSpinBox* sensorHeightSpin = nullptr;
	QLabel* framingLabel = nullptr;

	QSpinBox* columnsSpin = nullptr;
	QSpinBox* rowsSpin = nullptr;
	QDoubleSpinBox* overlapHSpin = nullptr;
	QDoubleSpinBox* overlapVSpin = nullptr;
	QDoubleSpinBox* rotationSpin = nullptr;
	QCheckBox* syncRotationCheck = nullptr;
	QCheckBox* constantRotationCheck = nullptr;
	QLabel* summaryLabel = nullptr;

	QTableWidget* panelTable = nullptr;

	QCheckBox* showCheck = nullptr;
	QCheckBox* editModeCheck = nullptr;
	QCheckBox* labelsCheck = nullptr;
	QCheckBox* fillCheck = nullptr;
	QPushButton* colorButton = nullptr;
};

#endif // MOSAICPLANNERDIALOG_HPP
