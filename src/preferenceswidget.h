/*
 *  Preferences dialog. Holds settings that are not per-model tuning (currently
 *  the busy spinner's appearance). Kept out of the main controls panel so the
 *  panel only carries per-remesh sliders.
 */
#ifndef AUTO_REMESHER_PREFERENCES_WIDGET_H
#define AUTO_REMESHER_PREFERENCES_WIDGET_H
#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QDoubleSpinBox)
QT_FORWARD_DECLARE_CLASS(QSpinBox)
QT_FORWARD_DECLARE_CLASS(QPushButton)

class PreferencesWidget : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesWidget(QWidget* parent = nullptr);

private slots:
    void pickSpinnerColor();

private:
    void updateSpinnerColorSwatch();

    QPushButton* m_spinnerColorButton = nullptr;
    QDoubleSpinBox* m_spinnerScaleSpinBox = nullptr;
    QSpinBox* m_spinnerContrastSpinBox = nullptr;
};

#endif
