// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QTimer>

class QMainWindow;
class QScreen;
class OrientationLayouts : public QObject {
    Q_OBJECT
public:
    explicit OrientationLayouts(QMainWindow* window, bool observeScreen = true);
    ~OrientationLayouts() override;
    static OrientationLayouts* install(QMainWindow* window);
    static void applyEnabled(bool enabled);
    static void forgetSaved();
    void setEnabled(bool enabled);
    void setOrientation(Qt::ScreenOrientation orientation);
    void captureNow();
    int currentOrientation() const { return m_current; }
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    void watchChildren();
    void observeScreen();
    void switchLayout();
    bool canvasOnly() const;
    bool docksReady() const;
    QByteArray m_pendingLayout;
    QStringList m_expectedDocks;
    static QString key(int orientation);
    QPointer<QMainWindow> m_window;
    QPointer<QScreen> m_screen;
    QSet<QObject*> m_watched;
    QSettings m_settings;
    QTimer m_save, m_switch, m_settle;
    QByteArray m_snapshot;
    int m_current = -1, m_observed = -1;
    bool m_enabled = true, m_applying = false, m_observeScreen, m_ready;
};
