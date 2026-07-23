/*
 *  Copyright (c) 2020 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */
#ifndef AUTO_REMESHER_PREFERENCES_H
#define AUTO_REMESHER_PREFERENCES_H
#include <QColor>
#include <QSettings>
#include <QSize>

class Preferences : public QObject {
    Q_OBJECT
public:
    static Preferences& instance();
    Preferences();
    QSize mainWindowSize() const;
    void setMainWindowSize(const QSize&);
    QString ftetwildPath() const;
    void setFtetwildPath(const QString&);
    // fTetWild tuning (see AutoRemesher::ExternalRemesher::Parameters). Stored as
    // fractions of the bbox diagonal, matching fTetWild's -l/-e conventions.
    double ftetwildEdgeLengthRel() const;
    void setFtetwildEdgeLengthRel(double);
    double ftetwildEnvelopeRel() const;
    void setFtetwildEnvelopeRel(double);
    bool ftetwildCoarsen() const;
    void setFtetwildCoarsen(bool);
    // Appearance of the busy spinner shown over the viewport during long runs.
    QColor busySpinnerColor() const;
    void setBusySpinnerColor(const QColor&);
    double busySpinnerScale() const; // 1.0 = default size
    void setBusySpinnerScale(double);
    int busySpinnerContrast() const; // 0..100, scrim opacity behind the spinner
    void setBusySpinnerContrast(int);
public slots:
    void reset();

private:
    QSettings m_settings;
};

#endif
