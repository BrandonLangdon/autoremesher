/*
 *  Preferences dialog. Holds settings that are not per-model tuning: the
 *  location of the external fTetWild binary and the fTetWild parameters used by
 *  the standalone "Run fTetWild" step. Kept out of the main controls panel so
 *  the panel only carries per-remesh sliders.
 */
#ifndef AUTO_REMESHER_PREFERENCES_WIDGET_H
#define AUTO_REMESHER_PREFERENCES_WIDGET_H
#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QCheckBox)
QT_FORWARD_DECLARE_CLASS(QPushButton)

class PreferencesWidget : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesWidget(QWidget* parent = nullptr);

signals:
    // Emitted whenever the fTetWild binary path changes, so the main window can
    // refresh any state that depends on whether fTetWild is available.
    void ftetwildConfigurationChanged();

private slots:
    void browseForFtetwildBinary();
    void pickSpinnerColor();

private:
    void updateFtetwildPathLabel();
    void updateSpinnerColorSwatch();

    QLabel* m_ftetwildPathLabel = nullptr;
    QDoubleSpinBox* m_edgeLengthSpinBox = nullptr;
    QDoubleSpinBox* m_envelopeSpinBox = nullptr;
    QCheckBox* m_coarsenCheckBox = nullptr;
    QPushButton* m_spinnerColorButton = nullptr;
    QDoubleSpinBox* m_spinnerScaleSpinBox = nullptr;
    QSpinBox* m_spinnerContrastSpinBox = nullptr;
};

#endif
