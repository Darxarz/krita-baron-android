// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QRect>
class QCheckBox;
class QComboBox;
class InpaintWidget : public QWidget {
    Q_OBJECT
public:
    explicit InpaintWidget(QWidget* parent = nullptr);
    QJsonObject state() const;
    void restore(const QJsonObject& state);
    void setLayers(const QJsonArray& layers);
    void setCapabilities(const QString& arch, double strength, bool editing);
    static int diffusionMultiple(const QString& arch);
    static QRect padded(QRect area, int padding, int minSize, int multiple, bool square = false);
    static QRect clamped(QRect area, QRect canvas);
    static QRect contextBounds(QRect canvas, QRect mask, const QJsonObject& options, QRect layer = {});
Q_SIGNALS:
    void changed();
    void editChanged(bool editing);
private:
    QCheckBox *m_seamless, *m_focus, *m_edit;
    QComboBox *m_fill, *m_context;
};
