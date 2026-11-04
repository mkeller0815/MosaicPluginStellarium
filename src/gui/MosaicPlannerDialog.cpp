/*
 * Mosaic Planner plug-in for Stellarium
 * Copyright (C) 2026 Mario Keller
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "MosaicPlannerDialog.hpp"
#include "MosaicPlanner.hpp"

#include "AngleSpinBox.hpp"
#include "Dialog.hpp"
#include "StelApp.hpp"
#include "StelMainView.hpp"
#include "StelTranslator.hpp"
#include "StelUtils.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtMath>

namespace
{
QDoubleSpinBox* makeDoubleSpin(double min, double max, int decimals, const QString& suffix)
{
	auto* spin = new QDoubleSpinBox();
	spin->setRange(min, max);
	spin->setDecimals(decimals);
	spin->setSuffix(suffix);
	spin->setKeyboardTracking(false);
	return spin;
}

QPushButton* makeButton(const QString& text, QWidget* parent)
{
	auto* button = new QPushButton(text, parent);
	button->setAutoDefault(false);
	return button;
}

// Panel scale sliders work in hundredths: 100..200 = 1.00x..2.00x
QSlider* makeScaleSlider(QWidget* parent)
{
	auto* slider = new QSlider(Qt::Horizontal, parent);
	slider->setRange(100, 200);
	slider->setSingleStep(1);
	slider->setPageStep(10);
	return slider;
}

QString scaleString(double scale)
{
	return QString("%1×").arg(scale, 0, 'f', 2);
}

QString arcminString(double deg)
{
	if (deg >= 2.)
		return QString("%1°").arg(deg, 0, 'f', 2);
	return QString("%1'").arg(deg * 60., 0, 'f', 1);
}
} // namespace

MosaicPlannerDialog::MosaicPlannerDialog(MosaicPlanner* plugin)
	: StelDialog("MosaicPlanner")
	, plugin(plugin)
{
	updateTimer.setSingleShot(true);
	updateTimer.setInterval(50);
	connect(&updateTimer, &QTimer::timeout, this, &MosaicPlannerDialog::updateFromModule);
}

MosaicPlannerDialog::~MosaicPlannerDialog() = default;

void MosaicPlannerDialog::retranslate()
{
	if (titleBar)
		titleBar->setTitle(q_("Mosaic Planner"));
}

void MosaicPlannerDialog::createDialogContent()
{
	auto* root = new QVBoxLayout(dialog);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	titleBar = new TitleBar(dialog);
	titleBar->setObjectName("titleBar");
	titleBar->setTitle(q_("Mosaic Planner"));
	root->addWidget(titleBar);
	connect(titleBar, &TitleBar::closeClicked, this, &StelDialog::close);
	connect(titleBar, &TitleBar::movedTo, this, &StelDialog::handleMovedTo);

	auto* tabs = new QTabWidget(dialog);
	root->addWidget(tabs);

	// ---------------------------------------------------------------- Mosaic tab
	auto* mosaicTab = new QWidget();
	auto* mosaicLayout = new QVBoxLayout(mosaicTab);

	auto* targetGroup = new QGroupBox(q_("Target (J2000)"), mosaicTab);
	auto* targetForm = new QFormLayout(targetGroup);
	targetNameEdit = new QLineEdit(targetGroup);
	raSpin = new AngleSpinBox(targetGroup, AngleSpinBox::HMSLetters);
	raSpin->setDecimals(1);
	decSpin = new AngleSpinBox(targetGroup, AngleSpinBox::DMSSymbols, AngleSpinBox::NormalPlus);
	decSpin->setDecimals(0);
	decSpin->setMinimum(-90., true);
	decSpin->setMaximum(90., true);
	targetForm->addRow(q_("Name:"), targetNameEdit);
	targetForm->addRow(q_("Right ascension:"), raSpin);
	targetForm->addRow(q_("Declination:"), decSpin);
	auto* targetButtons = new QHBoxLayout();
	auto* fromSelectionButton = makeButton(q_("Selected object"), targetGroup);
	fromSelectionButton->setToolTip(q_("Center the mosaic on the selected object"));
	auto* fromViewButton = makeButton(q_("View center"), targetGroup);
	fromViewButton->setToolTip(q_("Center the mosaic on the center of the screen"));
	auto* gotoButton = makeButton(q_("Go to mosaic"), targetGroup);
	targetButtons->addWidget(fromSelectionButton);
	targetButtons->addWidget(fromViewButton);
	targetButtons->addWidget(gotoButton);
	targetForm->addRow(targetButtons);
	mosaicLayout->addWidget(targetGroup);

	auto* equipmentGroup = new QGroupBox(q_("Camera framing"), mosaicTab);
	auto* equipmentLayout = new QVBoxLayout(equipmentGroup);
	auto* sourceRow = new QHBoxLayout();
	ocularsRadio = new QRadioButton(q_("From Oculars plug-in"), equipmentGroup);
	manualRadio = new QRadioButton(q_("Manual"), equipmentGroup);
	auto* sourceGroup = new QButtonGroup(equipmentGroup);
	sourceGroup->addButton(ocularsRadio);
	sourceGroup->addButton(manualRadio);
	sourceRow->addWidget(ocularsRadio);
	sourceRow->addWidget(manualRadio);
	sourceRow->addStretch();
	equipmentLayout->addLayout(sourceRow);

	ocularsBox = new QWidget(equipmentGroup);
	auto* ocularsForm = new QFormLayout(ocularsBox);
	ocularsForm->setContentsMargins(0, 0, 0, 0);
	telescopeCombo = new QComboBox(ocularsBox);
	ccdCombo = new QComboBox(ocularsBox);
	lensCombo = new QComboBox(ocularsBox);
	ocularsForm->addRow(q_("Telescope:"), telescopeCombo);
	ocularsForm->addRow(q_("Sensor:"), ccdCombo);
	ocularsForm->addRow(q_("Lens:"), lensCombo);
	equipmentLayout->addWidget(ocularsBox);

	manualBox = new QWidget(equipmentGroup);
	auto* manualForm = new QFormLayout(manualBox);
	manualForm->setContentsMargins(0, 0, 0, 0);
	focalSpin = makeDoubleSpin(1., 100000., 1, " mm");
	sensorWidthSpin = makeDoubleSpin(0.1, 1000., 2, " mm");
	sensorHeightSpin = makeDoubleSpin(0.1, 1000., 2, " mm");
	manualForm->addRow(q_("Focal length:"), focalSpin);
	manualForm->addRow(q_("Sensor width:"), sensorWidthSpin);
	manualForm->addRow(q_("Sensor height:"), sensorHeightSpin);
	equipmentLayout->addWidget(manualBox);

	auto* scaleForm = new QFormLayout();
	scaleForm->setContentsMargins(0, 0, 0, 0);
	auto makeScaleRow = [equipmentGroup](QSlider*& slider, QLabel*& label) {
		auto* row = new QHBoxLayout();
		slider = makeScaleSlider(equipmentGroup);
		label = new QLabel(scaleString(2.), equipmentGroup);
		label->setMinimumWidth(label->sizeHint().width());
		label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		row->addWidget(slider);
		row->addWidget(label);
		return row;
	};
	scaleForm->addRow(q_("Internal mosaic scale X:"), makeScaleRow(scaleXSlider, scaleXLabel));
	scaleForm->addRow(q_("Internal mosaic scale Y:"), makeScaleRow(scaleYSlider, scaleYLabel));
	const QString scaleTip = q_("Enlarges each panel for cameras with a built-in mosaic mode that covers more than the native sensor field");
	scaleXSlider->setToolTip(scaleTip);
	scaleYSlider->setToolTip(scaleTip);
	syncScaleCheck = new QCheckBox(q_("Same scale for X and Y"), equipmentGroup);
	scaleForm->addRow(syncScaleCheck);
	equipmentLayout->addLayout(scaleForm);

	framingLabel = new QLabel(equipmentGroup);
	framingLabel->setWordWrap(true);
	equipmentLayout->addWidget(framingLabel);
	mosaicLayout->addWidget(equipmentGroup);

	auto* layoutGroup = new QGroupBox(q_("Mosaic"), mosaicTab);
	auto* layoutForm = new QFormLayout(layoutGroup);
	auto* sizeRow = new QHBoxLayout();
	columnsSpin = new QSpinBox(layoutGroup);
	columnsSpin->setRange(1, 50);
	rowsSpin = new QSpinBox(layoutGroup);
	rowsSpin->setRange(1, 50);
	sizeRow->addWidget(columnsSpin);
	sizeRow->addWidget(new QLabel("×", layoutGroup));
	sizeRow->addWidget(rowsSpin);
	sizeRow->addStretch();
	layoutForm->addRow(q_("Panels (columns × rows):"), sizeRow);
	auto* overlapRow = new QHBoxLayout();
	overlapHSpin = makeDoubleSpin(0., 90., 0, " %");
	overlapVSpin = makeDoubleSpin(0., 90., 0, " %");
	overlapRow->addWidget(overlapHSpin);
	overlapRow->addWidget(new QLabel("/", layoutGroup));
	overlapRow->addWidget(overlapVSpin);
	overlapRow->addStretch();
	layoutForm->addRow(q_("Overlap (horizontal / vertical):"), overlapRow);
	rotationSpin = makeDoubleSpin(0., 360., 1, "°");
	rotationSpin->setWrapping(true);
	rotationSpin->setToolTip(q_("Position angle of the sensor's up direction, measured from north through east"));
	layoutForm->addRow(q_("Position angle (E of N):"), rotationSpin);
	syncRotationCheck = new QCheckBox(q_("Sync rotation with the Oculars sensor rotation"), layoutGroup);
	layoutForm->addRow(syncRotationCheck);
	constantRotationCheck = new QCheckBox(q_("Same rotation for all panels (do not rotate panes to preserve alignment)"), layoutGroup);
	layoutForm->addRow(constantRotationCheck);
	summaryLabel = new QLabel(layoutGroup);
	summaryLabel->setWordWrap(true);
	layoutForm->addRow(summaryLabel);
	mosaicLayout->addWidget(layoutGroup);
	mosaicLayout->addStretch();
	tabs->addTab(mosaicTab, q_("Mosaic"));

	// ---------------------------------------------------------------- Panels tab
	auto* panelsTab = new QWidget();
	auto* panelsLayout = new QVBoxLayout(panelsTab);
	panelTable = new QTableWidget(panelsTab);
	panelTable->setColumnCount(6);
	panelTable->setHorizontalHeaderLabels({q_("Panel"), q_("Row"), q_("Col"), q_("RA (J2000)"), q_("Dec (J2000)"), q_("PA")});
	panelTable->verticalHeader()->setVisible(false);
	panelTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	panelTable->horizontalHeader()->setStretchLastSection(true);
	panelTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	panelTable->setSelectionMode(QAbstractItemView::SingleSelection);
	panelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	panelTable->setMinimumHeight(260);
	panelsLayout->addWidget(panelTable);
	auto* panelHint = new QLabel(q_("Double-click a panel to point the view at it. "
	                                "Enable the Oculars sensor frame to preview its exact framing."), panelsTab);
	panelHint->setWordWrap(true);
	panelsLayout->addWidget(panelHint);
	auto* panelButtons = new QHBoxLayout();
	auto* gotoPanelButton = makeButton(q_("Go to panel"), panelsTab);
	auto* exportTelescopiusButton = makeButton(q_("Export CSV (Telescopius)…"), panelsTab);
	exportTelescopiusButton->setToolTip(q_("Telescopius mosaic CSV, can be imported by N.I.N.A., ASIAIR, StellarMate and others"));
	auto* exportDecimalButton = makeButton(q_("Export CSV (degrees)…"), panelsTab);
	panelButtons->addWidget(gotoPanelButton);
	panelButtons->addStretch();
	panelButtons->addWidget(exportTelescopiusButton);
	panelButtons->addWidget(exportDecimalButton);
	panelsLayout->addLayout(panelButtons);
	auto* planButtons = new QHBoxLayout();
	auto* savePlanButton = makeButton(q_("Save plan…"), panelsTab);
	auto* loadPlanButton = makeButton(q_("Load plan…"), panelsTab);
	auto* importCsvButton = makeButton(q_("Import CSV…"), panelsTab);
	importCsvButton->setToolTip(q_("Import a mosaic from a Telescopius CSV or from the degrees CSV export"));
	planButtons->addStretch();
	planButtons->addWidget(importCsvButton);
	planButtons->addWidget(savePlanButton);
	planButtons->addWidget(loadPlanButton);
	panelsLayout->addLayout(planButtons);
	tabs->addTab(panelsTab, q_("Panels"));

	// ---------------------------------------------------------------- Reference tab
	auto* referenceTab = new QWidget();
	auto* referenceLayout = new QVBoxLayout(referenceTab);
	auto* imageGroup = new QGroupBox(q_("Reference image"), referenceTab);
	auto* imageLayout = new QVBoxLayout(imageGroup);
	auto* referenceHint = new QLabel(q_("Load a plate-solved FITS file (or an astrometry.net .wcs file) of a panel you have "
	                                    "already captured. Only the header is read. Its frame is shown on the sky."), imageGroup);
	referenceHint->setWordWrap(true);
	imageLayout->addWidget(referenceHint);
	auto* referenceButtons = new QHBoxLayout();
	auto* loadReferenceButton = makeButton(q_("Load FITS…"), imageGroup);
	clearReferenceButton = makeButton(q_("Clear"), imageGroup);
	referenceButtons->addWidget(loadReferenceButton);
	referenceButtons->addWidget(clearReferenceButton);
	referenceButtons->addStretch();
	imageLayout->addLayout(referenceButtons);
	referenceInfoLabel = new QLabel(imageGroup);
	referenceInfoLabel->setWordWrap(true);
	referenceInfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	imageLayout->addWidget(referenceInfoLabel);
	showReferenceCheck = new QCheckBox(q_("Show reference frame"), imageGroup);
	imageLayout->addWidget(showReferenceCheck);
	referenceLayout->addWidget(imageGroup);

	auto* matchGroup = new QGroupBox(q_("Match with a panel"), referenceTab);
	auto* matchForm = new QFormLayout(matchGroup);
	referencePanelCombo = new QComboBox(matchGroup);
	matchForm->addRow(q_("Panel:"), referencePanelCombo);
	referenceMatchLabel = new QLabel(matchGroup);
	referenceMatchLabel->setWordWrap(true);
	matchForm->addRow(referenceMatchLabel);
	alignButton = makeButton(q_("Align mosaic to reference"), matchGroup);
	alignButton->setToolTip(q_("Move and rotate the whole mosaic so that this panel coincides with the reference image"));
	auto* alignRow = new QHBoxLayout();
	alignRow->addWidget(alignButton);
	alignRow->addStretch();
	matchForm->addRow(alignRow);
	referenceLayout->addWidget(matchGroup);
	referenceLayout->addStretch();
	tabs->addTab(referenceTab, q_("Reference"));

	// ---------------------------------------------------------------- Display tab
	auto* displayTab = new QWidget();
	auto* displayLayout = new QVBoxLayout(displayTab);
	showCheck = new QCheckBox(q_("Show mosaic"), displayTab);
	editModeCheck = new QCheckBox(q_("Edit with the mouse"), displayTab);
	auto* editHint = new QLabel(q_("Drag the mosaic to move it, Shift+drag to rotate it, click a panel to select it."), displayTab);
	editHint->setWordWrap(true);
	labelsCheck = new QCheckBox(q_("Show panel numbers and title"), displayTab);
	fillCheck = new QCheckBox(q_("Fill panels"), displayTab);
	auto* colorRow = new QHBoxLayout();
	colorButton = makeButton(QString(), displayTab);
	colorButton->setFixedWidth(60);
	colorRow->addWidget(new QLabel(q_("Color:"), displayTab));
	colorRow->addWidget(colorButton);
	colorRow->addStretch();
	displayLayout->addWidget(showCheck);
	displayLayout->addWidget(editModeCheck);
	displayLayout->addWidget(editHint);
	displayLayout->addWidget(labelsCheck);
	displayLayout->addWidget(fillCheck);
	displayLayout->addLayout(colorRow);
	displayLayout->addStretch();
	tabs->addTab(displayTab, q_("Display"));

	// ---------------------------------------------------------------- Connections
	connect(targetNameEdit, &QLineEdit::editingFinished, this, [this]() { plugin->setTargetName(targetNameEdit->text().trimmed()); });
	connect(raSpin, &AngleSpinBox::valueChanged, this, [this]() { plugin->setCenterRA(raSpin->valueDegrees()); });
	connect(decSpin, &AngleSpinBox::valueChanged, this, [this]() { plugin->setCenterDec(decSpin->valueDegrees()); });
	connect(fromSelectionButton, &QPushButton::clicked, this, [this]() {
		if (!plugin->centerOnSelectedObject())
			messageBox(q_("Mosaic Planner"), q_("Please select an object first."));
	});
	connect(fromViewButton, &QPushButton::clicked, plugin, &MosaicPlanner::centerOnView);
	connect(gotoButton, &QPushButton::clicked, plugin, &MosaicPlanner::gotoMosaic);

	connect(ocularsRadio, &QRadioButton::toggled, plugin, &MosaicPlanner::setUseOculars);
	OcularsBridge* bridge = plugin->getOcularsBridge();
	connect(telescopeCombo, QOverload<int>::of(&QComboBox::activated), bridge, &OcularsBridge::selectTelescope);
	connect(ccdCombo, QOverload<int>::of(&QComboBox::activated), bridge, &OcularsBridge::selectCcd);
	// First lens entry is "none" (-1 in Oculars)
	connect(lensCombo, QOverload<int>::of(&QComboBox::activated), bridge, [bridge](int index) { bridge->selectLens(index - 1); });
	connect(focalSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &MosaicPlanner::setManualFocalLength);
	connect(sensorWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &MosaicPlanner::setManualSensorWidth);
	connect(sensorHeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &MosaicPlanner::setManualSensorHeight);

	connect(scaleXSlider, &QSlider::valueChanged, this, [this](int value) { plugin->setPanelScaleX(value / 100.); });
	connect(scaleYSlider, &QSlider::valueChanged, this, [this](int value) { plugin->setPanelScaleY(value / 100.); });
	connect(syncScaleCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setSyncPanelScale);

	connect(columnsSpin, QOverload<int>::of(&QSpinBox::valueChanged), plugin, &MosaicPlanner::setColumns);
	connect(rowsSpin, QOverload<int>::of(&QSpinBox::valueChanged), plugin, &MosaicPlanner::setRows);
	connect(overlapHSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &MosaicPlanner::setOverlapH);
	connect(overlapVSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &MosaicPlanner::setOverlapV);
	connect(rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), plugin, &MosaicPlanner::setRotation);
	connect(syncRotationCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setSyncRotation);
	connect(constantRotationCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setConstantRotation);

	connect(panelTable, &QTableWidget::cellDoubleClicked, plugin, [this](int row, int) { plugin->gotoPanel(row); });
	connect(panelTable, &QTableWidget::currentCellChanged, plugin, [this](int row, int, int, int) { plugin->setSelectedPanel(row); });
	connect(gotoPanelButton, &QPushButton::clicked, this, &MosaicPlannerDialog::gotoSelectedPanel);
	connect(exportTelescopiusButton, &QPushButton::clicked, this, &MosaicPlannerDialog::exportTelescopius);
	connect(exportDecimalButton, &QPushButton::clicked, this, &MosaicPlannerDialog::exportDecimal);
	connect(savePlanButton, &QPushButton::clicked, this, &MosaicPlannerDialog::savePlan);
	connect(loadPlanButton, &QPushButton::clicked, this, &MosaicPlannerDialog::loadPlan);
	connect(importCsvButton, &QPushButton::clicked, this, &MosaicPlannerDialog::importCsv);

	connect(loadReferenceButton, &QPushButton::clicked, this, &MosaicPlannerDialog::loadReference);
	connect(clearReferenceButton, &QPushButton::clicked, plugin, &MosaicPlanner::clearReferenceImage);
	connect(showReferenceCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setShowReference);
	connect(referencePanelCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
		plugin->setSelectedPanel(index);
		updateReference();
	});
	connect(alignButton, &QPushButton::clicked, this, [this]() { plugin->alignToReference(referencePanelCombo->currentIndex()); });
	connect(plugin, &MosaicPlanner::referenceChanged, this, &MosaicPlannerDialog::scheduleUpdate);

	connect(showCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setEnabled);
	connect(editModeCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setEditMode);
	connect(labelsCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setShowLabels);
	connect(fillCheck, &QCheckBox::toggled, plugin, &MosaicPlanner::setFillPanels);
	connect(colorButton, &QPushButton::clicked, this, &MosaicPlannerDialog::chooseColor);

	connect(plugin, &MosaicPlanner::mosaicChanged, this, [this]() { tableDirty = true; scheduleUpdate(); });
	connect(plugin, &MosaicPlanner::displayChanged, this, &MosaicPlannerDialog::scheduleUpdate);
	connect(plugin, &MosaicPlanner::enabledChanged, this, &MosaicPlannerDialog::scheduleUpdate);
	connect(plugin, &MosaicPlanner::selectedPanelChanged, this, &MosaicPlannerDialog::updateSelection);
	connect(bridge, &OcularsBridge::framingChanged, this, &MosaicPlannerDialog::updateEquipmentLists);
	connect(&StelApp::getInstance(), &StelApp::languageChanged, this, &MosaicPlannerDialog::retranslate);

	// Opening the planner implies the user wants to see the mosaic
	connect(this, &StelDialog::visibleChanged, this, [this](bool visible) {
		if (visible)
		{
			plugin->setEnabled(true);
			updateEquipmentLists();
			tableDirty = true;
			updateFromModule();
		}
	});

	updateEquipmentLists();
	updateFromModule();
}

void MosaicPlannerDialog::scheduleUpdate()
{
	if (!updateTimer.isActive())
		updateTimer.start();
}

void MosaicPlannerDialog::updateEquipmentLists()
{
	if (!dialog)
		return;
	OcularsBridge* bridge = plugin->getOcularsBridge();
	const bool available = bridge->isAvailable();
	ocularsRadio->setEnabled(available);

	auto fill = [](QComboBox* combo, const QStringList& names, int selected) {
		const QSignalBlocker blocker(combo);
		QStringList current;
		for (int i = 0; i < combo->count(); ++i)
			current << combo->itemText(i);
		if (current != names)
		{
			combo->clear();
			combo->addItems(names);
		}
		combo->setCurrentIndex(selected);
	};
	fill(telescopeCombo, bridge->telescopeNames(), bridge->selectedTelescopeIndex());
	fill(ccdCombo, bridge->ccdNames(), bridge->selectedCcdIndex());
	fill(lensCombo, QStringList(q_("None")) + bridge->lensNames(), bridge->selectedLensIndex() + 1);
	scheduleUpdate();
}

void MosaicPlannerDialog::updateFromModule()
{
	if (!dialog)
		return;

	{
		const QSignalBlocker b1(targetNameEdit), b2(raSpin), b3(decSpin), b4(ocularsRadio), b5(manualRadio),
		        b6(focalSpin), b7(sensorWidthSpin), b8(sensorHeightSpin), b9(columnsSpin), b10(rowsSpin),
		        b11(overlapHSpin), b12(overlapVSpin), b13(rotationSpin), b14(syncRotationCheck),
		        b15(constantRotationCheck), b16(showCheck), b17(editModeCheck), b18(labelsCheck), b19(fillCheck),
		        b20(scaleXSlider), b21(scaleYSlider), b22(syncScaleCheck);

		if (!targetNameEdit->hasFocus())
			targetNameEdit->setText(plugin->getTargetName());
		if (!raSpin->hasFocus())
			raSpin->setDegrees(plugin->getCenterRA());
		if (!decSpin->hasFocus())
			decSpin->setDegrees(plugin->getCenterDec());

		const bool useOculars = plugin->getUseOculars();
		ocularsRadio->setChecked(useOculars);
		manualRadio->setChecked(!useOculars);
		ocularsBox->setVisible(useOculars);
		manualBox->setVisible(!useOculars);
		syncRotationCheck->setEnabled(useOculars);
		focalSpin->setValue(plugin->getManualFocalLength());
		sensorWidthSpin->setValue(plugin->getManualSensorWidth());
		sensorHeightSpin->setValue(plugin->getManualSensorHeight());
		scaleXSlider->setValue(qRound(plugin->getPanelScaleX() * 100.));
		scaleYSlider->setValue(qRound(plugin->getPanelScaleY() * 100.));
		scaleXLabel->setText(scaleString(plugin->getPanelScaleX()));
		scaleYLabel->setText(scaleString(plugin->getPanelScaleY()));
		syncScaleCheck->setChecked(plugin->getSyncPanelScale());

		columnsSpin->setValue(plugin->getColumns());
		rowsSpin->setValue(plugin->getRows());
		overlapHSpin->setValue(plugin->getOverlapH());
		overlapVSpin->setValue(plugin->getOverlapV());
		if (!rotationSpin->hasFocus())
			rotationSpin->setValue(plugin->getRotation());
		syncRotationCheck->setChecked(plugin->getSyncRotation());
		constantRotationCheck->setChecked(plugin->getConstantRotation());

		showCheck->setChecked(plugin->isEnabled());
		editModeCheck->setChecked(plugin->getEditMode());
		labelsCheck->setChecked(plugin->getShowLabels());
		fillCheck->setChecked(plugin->getFillPanels());
	}
	updateColorButton();

	double fovX = 0., fovY = 0.;
	QString framing = plugin->getFramingDescription();
	if (plugin->getPanelFov(fovX, fovY))
	{
		framing += "<br>" + QString(q_("Panel field of view: %1 × %2")).arg(arcminString(fovX), arcminString(fovY));
		const double scale = plugin->getPixelScale();
		if (scale > 0.)
			framing += QString(", %1\"/px").arg(scale, 0, 'f', 2);
		const mosaic::MosaicParams params = plugin->getParams();
		summaryLabel->setText(QString(q_("%1 panels, total field %2 × %3"))
		                      .arg(plugin->getColumns() * plugin->getRows())
		                      .arg(arcminString(mosaic::mosaicWidthDeg(params)), arcminString(mosaic::mosaicHeightDeg(params))));
	}
	else
	{
		summaryLabel->setText(q_("The panel field of view is unknown."));
	}
	framingLabel->setText(framing);

	if (tableDirty)
		updatePanelTable();
	updateReference();
}

void MosaicPlannerDialog::updatePanelTable()
{
	tableDirty = false;
	const std::vector<mosaic::Panel> panels = plugin->getPanels();
	const QSignalBlocker blocker(panelTable);
	panelTable->setRowCount(static_cast<int>(panels.size()));
	for (int i = 0; i < static_cast<int>(panels.size()); ++i)
	{
		const mosaic::Panel& p = panels[static_cast<size_t>(i)];
		const QStringList cells = {
		        QString::number(p.number),
		        QString::number(p.row + 1),
		        QString::number(p.column + 1),
		        StelUtils::radToHmsStr(qDegreesToRadians(p.raDeg), true).trimmed(),
		        StelUtils::radToDmsStr(qDegreesToRadians(p.decDeg), true),
		        QString("%1°").arg(p.paDeg, 0, 'f', 2)};
		for (int c = 0; c < cells.size(); ++c)
		{
			QTableWidgetItem* item = panelTable->item(i, c);
			if (!item)
			{
				item = new QTableWidgetItem();
				panelTable->setItem(i, c, item);
			}
			item->setText(cells.at(c));
		}
	}
	updateSelection(plugin->getSelectedPanel());
}

void MosaicPlannerDialog::updateSelection(int index)
{
	if (!dialog)
		return;
	const QSignalBlocker blocker(panelTable);
	if (index < 0 || index >= panelTable->rowCount())
		panelTable->clearSelection();
	else
		panelTable->selectRow(index);
}

void MosaicPlannerDialog::gotoSelectedPanel()
{
	const int row = panelTable->currentRow();
	plugin->gotoPanel(row >= 0 ? row : plugin->getSelectedPanel());
}

void MosaicPlannerDialog::updateColorButton()
{
	const Vec3f c = plugin->getLineColor();
	colorButton->setStyleSheet(QString("background-color: %1;").arg(QColor::fromRgbF(c[0], c[1], c[2]).name()));
}

void MosaicPlannerDialog::chooseColor()
{
	const Vec3f c = plugin->getLineColor();
	const QColor color = QColorDialog::getColor(QColor::fromRgbF(c[0], c[1], c[2]), &StelMainView::getInstance(), q_("Mosaic color"));
	if (color.isValid())
		plugin->setLineColor(Vec3f(static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF())));
}

QString MosaicPlannerDialog::askSaveFile(const QString& title, const QString& suffix, const QString& filter)
{
	QString name = plugin->getTargetName().trimmed();
	if (name.isEmpty())
		name = "mosaic";
	name.replace(QRegularExpression("[^A-Za-z0-9_+-]+"), "_");
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString path = QFileDialog::getSaveFileName(&StelMainView::getInstance(), title,
	                                            QDir(dir).filePath(QString("%1_%2x%3%4").arg(name).arg(plugin->getColumns()).arg(plugin->getRows()).arg(suffix)),
	                                            filter);
	if (!path.isEmpty() && !path.endsWith(suffix, Qt::CaseInsensitive))
		path += suffix;
	return path;
}

void MosaicPlannerDialog::exportTelescopius()
{
	const QString path = askSaveFile(q_("Export mosaic"), ".csv", "CSV (*.csv)");
	if (!path.isEmpty() && !plugin->exportTelescopiusCsv(path))
		messageBox(q_("Mosaic Planner"), q_("Could not write the file."));
}

void MosaicPlannerDialog::exportDecimal()
{
	const QString path = askSaveFile(q_("Export mosaic"), ".csv", "CSV (*.csv)");
	if (!path.isEmpty() && !plugin->exportDecimalCsv(path))
		messageBox(q_("Mosaic Planner"), q_("Could not write the file."));
}

void MosaicPlannerDialog::savePlan()
{
	const QString path = askSaveFile(q_("Save mosaic plan"), ".json", q_("Mosaic plan (*.json)"));
	if (!path.isEmpty() && !plugin->savePlan(path))
		messageBox(q_("Mosaic Planner"), q_("Could not write the file."));
}

void MosaicPlannerDialog::updateReference()
{
	if (!dialog)
		return;
	const bool loaded = plugin->hasReference();
	clearReferenceButton->setEnabled(loaded);
	{
		const QSignalBlocker blocker(showReferenceCheck);
		showReferenceCheck->setChecked(plugin->getShowReference());
	}

	const int count = static_cast<int>(plugin->getPanels().size());
	if (referencePanelCombo->count() != count)
	{
		const QSignalBlocker blocker(referencePanelCombo);
		const int current = referencePanelCombo->currentIndex();
		referencePanelCombo->clear();
		for (int i = 1; i <= count; ++i)
			referencePanelCombo->addItem(QString(q_("Panel %1")).arg(i));
		referencePanelCombo->setCurrentIndex(count > 0 ? qBound(0, current, count - 1) : -1);
	}
	referencePanelCombo->setEnabled(loaded && count > 0);
	alignButton->setEnabled(loaded && count > 0);

	if (!loaded)
	{
		referenceInfoLabel->setText(q_("No reference image loaded."));
		referenceMatchLabel->clear();
		selectBestReferencePanel = true;
		return;
	}

	const mosaic::ReferenceFrame& ref = plugin->getReference();
	double ra, dec;
	mosaic::vecToRaDec(ref.center, ra, dec);
	QStringList info;
	info << QString("<b>%1</b>").arg(QFileInfo(plugin->getReferencePath()).fileName().toHtmlEscaped());
	if (!ref.object.empty())
		info << QString(q_("Object: %1")).arg(QString::fromStdString(ref.object).toHtmlEscaped());
	if (!ref.dateObs.empty())
		info << QString(q_("Date: %1")).arg(QString::fromStdString(ref.dateObs).toHtmlEscaped());
	info << QString(q_("Center: %1 / %2"))
	                .arg(StelUtils::radToHmsStr(qDegreesToRadians(ra), true).trimmed(), StelUtils::radToDmsStr(qDegreesToRadians(dec), true));
	QString size = QString(q_("Field: %1 × %2 (%3 × %4 px)"))
	                       .arg(arcminString(ref.fovXDeg), arcminString(ref.fovYDeg)).arg(ref.width).arg(ref.height);
	if (ref.pixelScaleArcsec > 0.)
		size += QString(", %1\"/px").arg(ref.pixelScaleArcsec, 0, 'f', 2);
	info << size;
	if (ref.hasWcs)
		info << QString(q_("Position angle of the image y axis: %1°")).arg(mosaic::positionAngleOf(ref.center, ref.axisY), 0, 'f', 2);
	else
		info << q_("The file is not plate solved: the position comes from the mount and the rotation is unknown. "
		           "Aligning only moves the mosaic.");
	referenceInfoLabel->setText(info.join("<br>"));

	const int best = plugin->getReferenceBestPanel();
	if (selectBestReferencePanel && best >= 0)
	{
		const QSignalBlocker blocker(referencePanelCombo);
		referencePanelCombo->setCurrentIndex(best);
		selectBestReferencePanel = false;
	}
	double offset, rotation;
	if (plugin->getReferenceMatch(referencePanelCombo->currentIndex(), offset, rotation))
	{
		QString match = QString(q_("Offset from the panel center: %1")).arg(arcminString(offset / 60.));
		if (ref.hasWcs)
			match += "<br>" + QString(q_("Rotation relative to the panel: %1°")).arg(rotation, 0, 'f', 2);
		if (best >= 0)
			match += "<br>" + QString(q_("Closest panel: %1")).arg(best + 1);
		referenceMatchLabel->setText(match);
	}
	else
		referenceMatchLabel->clear();
}

void MosaicPlannerDialog::loadReference()
{
	const QString dir = plugin->hasReference() ? QFileInfo(plugin->getReferencePath()).absolutePath()
	                                           : QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
	const QString path = QFileDialog::getOpenFileName(&StelMainView::getInstance(), q_("Load reference image"), dir,
	                                                  q_("FITS files (*.fits *.fit *.fts *.new *.wcs);;All files (*)"));
	if (path.isEmpty())
		return;
	QString error;
	if (!plugin->loadReferenceFile(path, error))
	{
		messageBox(q_("Mosaic Planner"), q_("Could not load the reference image.") + "\n" + error);
		return;
	}
	plugin->setShowReference(true);
	selectBestReferencePanel = true;
	updateReference();
	plugin->setSelectedPanel(referencePanelCombo->currentIndex());
}

void MosaicPlannerDialog::importCsv()
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString path = QFileDialog::getOpenFileName(&StelMainView::getInstance(), q_("Import mosaic CSV"), dir, "CSV (*.csv *.txt)");
	if (path.isEmpty())
		return;
	QString error, note;
	if (!plugin->importCsvFile(path, error, note))
		messageBox(q_("Mosaic Planner"), q_("This file is not a valid mosaic CSV.") + "\n" + error);
	else if (!note.isEmpty())
		messageBox(q_("Mosaic Planner"), note);
}

void MosaicPlannerDialog::loadPlan()
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString path = QFileDialog::getOpenFileName(&StelMainView::getInstance(), q_("Load mosaic plan"), dir, q_("Mosaic plan (*.json)"));
	if (!path.isEmpty() && !plugin->loadPlan(path))
		messageBox(q_("Mosaic Planner"), q_("This file is not a valid mosaic plan."));
}
