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
#include "preferences.h"

Preferences& Preferences::instance()
{
    static Preferences* s_preferences = nullptr;
    if (nullptr == s_preferences) {
        s_preferences = new Preferences;
    }
    return *s_preferences;
}

Preferences::Preferences()
{
}

QSize Preferences::mainWindowSize() const
{
    return m_settings.value("mainWindowSize", QSize()).toSize();
}

void Preferences::setMainWindowSize(const QSize& size)
{
    m_settings.setValue("mainWindowSize", size);
}

QColor Preferences::busySpinnerColor() const
{
    // A bright amber default reads well over both light and dark meshes.
    return m_settings.value("busySpinnerColor", QColor(0xff, 0xc1, 0x07)).value<QColor>();
}

void Preferences::setBusySpinnerColor(const QColor& color)
{
    m_settings.setValue("busySpinnerColor", color);
}

double Preferences::busySpinnerScale() const
{
    return m_settings.value("busySpinnerScale", 1.0).toDouble();
}

void Preferences::setBusySpinnerScale(double value)
{
    m_settings.setValue("busySpinnerScale", value);
}

int Preferences::busySpinnerContrast() const
{
    return m_settings.value("busySpinnerContrast", 85).toInt();
}

void Preferences::setBusySpinnerContrast(int value)
{
    m_settings.setValue("busySpinnerContrast", value);
}

void Preferences::reset()
{
    m_settings.clear();
}
