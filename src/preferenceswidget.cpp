/*
 *  See preferenceswidget.h.
 */
#include "preferenceswidget.h"
#include "preferences.h"
#include "version.h"
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

PreferencesWidget::PreferencesWidget(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("%1 Preferences").arg(APP_NAME));

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

    QLabel* spinnerHeading = new QLabel(tr("Busy indicator"));
    spinnerHeading->setStyleSheet("font-weight: bold;");

    QFormLayout* form = new QFormLayout;
    form->addRow(spinnerHeading);
    form->addRow(tr("Spinner color:"), m_spinnerColorButton);
    form->addRow(tr("Spinner size:"), m_spinnerScaleSpinBox);
    form->addRow(tr("Contrast pad:"), m_spinnerContrastSpinBox);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addStretch();
    mainLayout->addWidget(buttonBox);

    setMinimumWidth(440);
    updateSpinnerColorSwatch();
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
