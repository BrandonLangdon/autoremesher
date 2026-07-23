/*
 *  See preferenceswidget.h.
 */
#include "preferenceswidget.h"
#include "preferences.h"
#include "version.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PreferencesWidget::PreferencesWidget(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("%1 Preferences").arg(APP_NAME));

    // --- fTetWild binary path ---
    m_ftetwildPathLabel = new QLabel;
    m_ftetwildPathLabel->setWordWrap(true);
    m_ftetwildPathLabel->setStyleSheet("color: #cccccc;");

    QPushButton* browseButton = new QPushButton(tr("Browse…"));
    connect(browseButton, &QPushButton::clicked, this, &PreferencesWidget::browseForFtetwildBinary);

    QHBoxLayout* pathLayout = new QHBoxLayout;
    pathLayout->addWidget(m_ftetwildPathLabel, 1);
    pathLayout->addWidget(browseButton);

    // --- fTetWild parameters (item #1) ---
    m_edgeLengthSpinBox = new QDoubleSpinBox;
    m_edgeLengthSpinBox->setDecimals(3);
    m_edgeLengthSpinBox->setRange(0.005, 0.500);
    m_edgeLengthSpinBox->setSingleStep(0.005);
    m_edgeLengthSpinBox->setValue(Preferences::instance().ftetwildEdgeLengthRel());
    m_edgeLengthSpinBox->setToolTip(tr("fTetWild ideal edge length (-l), as a fraction of the model's bounding-box diagonal. Smaller = denser, higher-resolution surface. Default 0.05."));
    connect(m_edgeLengthSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this, [](double value) {
            Preferences::instance().setFtetwildEdgeLengthRel(value);
        });

    m_envelopeSpinBox = new QDoubleSpinBox;
    m_envelopeSpinBox->setDecimals(4);
    m_envelopeSpinBox->setRange(0.0001, 0.0500);
    m_envelopeSpinBox->setSingleStep(0.0005);
    m_envelopeSpinBox->setValue(Preferences::instance().ftetwildEnvelopeRel());
    m_envelopeSpinBox->setToolTip(tr("fTetWild envelope size (-e), as a fraction of the bounding-box diagonal. How far the output surface may deviate from the input. Smaller = more faithful (and slower). Default 0.001."));
    connect(m_envelopeSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this, [](double value) {
            Preferences::instance().setFtetwildEnvelopeRel(value);
        });

    m_coarsenCheckBox = new QCheckBox(tr("Coarsen output"));
    m_coarsenCheckBox->setChecked(Preferences::instance().ftetwildCoarsen());
    m_coarsenCheckBox->setToolTip(tr("Pass --coarsen to fTetWild: simplify the output as much as the envelope allows, producing fewer triangles."));
    connect(m_coarsenCheckBox, &QCheckBox::toggled, this, [](bool checked) {
        Preferences::instance().setFtetwildCoarsen(checked);
    });

    // --- Busy spinner appearance ---
    m_spinnerColorButton = new QPushButton;
    m_spinnerColorButton->setToolTip(tr("Color of the busy spinner and its caption shown over the viewport during long operations."));
    connect(m_spinnerColorButton, &QPushButton::clicked, this, &PreferencesWidget::pickSpinnerColor);

    m_spinnerScaleSpinBox = new QDoubleSpinBox;
    m_spinnerScaleSpinBox->setDecimals(1);
    m_spinnerScaleSpinBox->setRange(0.5, 3.0);
    m_spinnerScaleSpinBox->setSingleStep(0.1);
    m_spinnerScaleSpinBox->setValue(Preferences::instance().busySpinnerScale());
    m_spinnerScaleSpinBox->setToolTip(tr("Size of the busy spinner (1.0 = default)."));
    connect(m_spinnerScaleSpinBox, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
        this, [](double value) {
            Preferences::instance().setBusySpinnerScale(value);
        });

    m_spinnerContrastSpinBox = new QSpinBox;
    m_spinnerContrastSpinBox->setRange(0, 100);
    m_spinnerContrastSpinBox->setSuffix(tr(" %"));
    m_spinnerContrastSpinBox->setValue(Preferences::instance().busySpinnerContrast());
    m_spinnerContrastSpinBox->setToolTip(tr("Darkness of the pad drawn behind the spinner. Higher = stronger contrast against the model; 0 = no pad."));
    connect(m_spinnerContrastSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
        this, [](int value) {
            Preferences::instance().setBusySpinnerContrast(value);
        });

    QFrame* divider = new QFrame;
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Sunken);

    QLabel* spinnerHeading = new QLabel(tr("Busy indicator"));
    spinnerHeading->setStyleSheet("font-weight: bold;");

    QFormLayout* form = new QFormLayout;
    form->addRow(tr("fTetWild binary:"), pathLayout);
    form->addRow(tr("Edge length (rel):"), m_edgeLengthSpinBox);
    form->addRow(tr("Envelope size (rel):"), m_envelopeSpinBox);
    form->addRow(QString(), m_coarsenCheckBox);
    form->addRow(divider);
    form->addRow(spinnerHeading);
    form->addRow(tr("Spinner color:"), m_spinnerColorButton);
    form->addRow(tr("Spinner size:"), m_spinnerScaleSpinBox);
    form->addRow(tr("Contrast pad:"), m_spinnerContrastSpinBox);

    QLabel* hint = new QLabel(tr("The “Run fTetWild” step uses these settings. fTetWild must be built "
                                 "separately; point the path above at its FloatTetwild_bin executable."));
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #999999; font-size: 11px;");

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addWidget(hint);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);

    setMinimumWidth(440);
    updateFtetwildPathLabel();
    updateSpinnerColorSwatch();
}

void PreferencesWidget::browseForFtetwildBinary()
{
    const QString filename = QFileDialog::getOpenFileName(this,
        tr("Select fTetWild binary (FloatTetwild_bin)"),
        Preferences::instance().ftetwildPath());
    if (filename.isEmpty())
        return;
    Preferences::instance().setFtetwildPath(filename);
    // Keep the core in sync: it locates fTetWild via AUTOREMESHER_FTETWILD.
    qputenv("AUTOREMESHER_FTETWILD", filename.toUtf8());
    updateFtetwildPathLabel();
    emit ftetwildConfigurationChanged();
}

void PreferencesWidget::updateFtetwildPathLabel()
{
    const QString path = Preferences::instance().ftetwildPath();
    if (path.isEmpty())
        m_ftetwildPathLabel->setText(tr("<i>Not set — “Run fTetWild” is disabled.</i>"));
    else
        m_ftetwildPathLabel->setText(path);
}

void PreferencesWidget::pickSpinnerColor()
{
    const QColor current = Preferences::instance().busySpinnerColor();
    const QColor chosen = QColorDialog::getColor(current, this, tr("Spinner Color"));
    if (!chosen.isValid())
        return;
    Preferences::instance().setBusySpinnerColor(chosen);
    updateSpinnerColorSwatch();
}

void PreferencesWidget::updateSpinnerColorSwatch()
{
    const QColor color = Preferences::instance().busySpinnerColor();
    // Show the current color as a swatch on the button, with a readable label.
    const QString textColor = (color.lightness() > 127) ? "#000000" : "#ffffff";
    m_spinnerColorButton->setText(color.name());
    m_spinnerColorButton->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; border: 1px solid #555; padding: 4px 10px; }")
            .arg(color.name(), textColor));
}
