// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
class ToggleSwitch;

class UpscaleWidget : public QWidget {
    Q_OBJECT
public:
    explicit UpscaleWidget(QWidget* parent = nullptr);
    QComboBox *upscaler, *style;
    QDoubleSpinBox* factor;
    QJsonObject state() const;
    void restore(const QJsonObject& state);
    void setResources(const QJsonObject& resources);
    void setArchitecture(const QString& architecture);
    void setCanvasSize(QSize size);
    void setPrompt(const QString& prompt, int regionCount);
    double effectiveStrength() const;
Q_SIGNALS:
    void changed();
    void configureStyle();
private:
    void updateTarget();
    QSlider* m_factorSlider;
    QLabel *m_target, *m_prompt, *m_warning;
    QGroupBox* m_refine;
    QSpinBox *m_strength, *m_guidance, *m_overlap;
    QComboBox* m_overlapMode;
    ToggleSwitch* m_usePrompt;
    QWidget *m_strengthRow, *m_guidanceRow;
    QJsonObject m_resources;
    QSize m_canvas;
    QString m_arch, m_savedUpscaler;
};
