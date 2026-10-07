// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonObject>
#include <QPointer>
#include <QRect>
#include <QWidget>

class CanvasHost;
class QImage;
class QVBoxLayout;
class ControlRow;
class RegionRow;
class QPlainTextEdit;

class GuidancePanel : public QWidget {
    Q_OBJECT
public:
    GuidancePanel(CanvasHost* host, QWidget* parent = nullptr);
    QJsonObject input(const QRect& bounds, const QImage& selection, QString* error);
    void setControlLayer(const QString& id, const QString& controlId = {});
    QJsonObject state() const;
    void restoreState(const QJsonObject& state);
    void setRootEditors(QPlainTextEdit* positive, QPlainTextEdit* negative);
    void activateRegion(const QString& id);
    void setArchitecture(const QString& arch);
    void setControlBusy(const QString& id, bool busy);
Q_SIGNALS:
    void controlRequested(const QJsonObject& input);
    void controlMapGenerated(const QString& target, const QString& controlId, const QImage& image);
    void error(const QString& message);
    void changed();
    void activeRegionChanged(const QString& id);
    void activated();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void addControl(const QString& mode, const QString& title);
    void addRegion();
    void updateRegions();
    void updateRootSummary();
    CanvasHost* m_host;
    QVBoxLayout *m_controlsLayout, *m_regionsLayout;
    QList<ControlRow*> m_controls;
    QList<RegionRow*> m_regions;
    QPointer<ControlRow> m_pendingControl;
    QPlainTextEdit *m_rootPositive = nullptr, *m_rootNegative = nullptr;
    QString m_activeRegion;
    QWidget* m_rootSummary = nullptr;
    QString m_architecture;
    QString m_lastControlMode = "scribble";
};
