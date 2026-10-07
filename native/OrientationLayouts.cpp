// SPDX-License-Identifier: GPL-3.0-or-later
#include "OrientationLayouts.h"
#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QMainWindow>
#include <QInputMethod>
#include <QScreen>
#include <QToolBar>
#include <QWindow>

QString OrientationLayouts::key(int orientation) {
    return orientation == 1 ? QStringLiteral("orientation_layout/portrait")
                            : QStringLiteral("orientation_layout/landscape");
}
OrientationLayouts::OrientationLayouts(QMainWindow* window, bool observe)
    : QObject(window), m_window(window), m_settings("BaronEdition", "Orchestrion"), m_observeScreen(observe), m_ready(!observe) {
    setObjectName("orientationLayouts");
    m_enabled = m_settings.value("orientation_layouts_enabled", true).toBool();
    m_save.setSingleShot(true); m_save.setInterval(120);
    m_switch.setSingleShot(true); m_switch.setInterval(250);
    m_settle.setSingleShot(true); m_settle.setInterval(300);
    connect(&m_save, &QTimer::timeout, this, &OrientationLayouts::captureNow);
    connect(&m_switch, &QTimer::timeout, this, &OrientationLayouts::switchLayout);
    connect(&m_settle, &QTimer::timeout, this, [this] { m_applying = false; captureNow(); });
    connect(QGuiApplication::inputMethod(), &QInputMethod::visibleChanged, this, [this] {
        if (!QGuiApplication::inputMethod()->isVisible()) m_save.start();
    });
    connect(qApp,&QCoreApplication::aboutToQuit,this,[this] { captureNow();m_settings.sync(); });
    window->installEventFilter(this);
    watchChildren();
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) { captureNow(); m_settings.sync(); }
        else if (m_observeScreen) observeScreen();
    });
    QTimer::singleShot(m_observeScreen ? 2000 : 700, this, [this] {
        m_ready = true;
        watchChildren();
        if (m_observeScreen) observeScreen();
        else if (m_observed >= 0) m_switch.start();
    });
}
OrientationLayouts::~OrientationLayouts() {
    if (m_enabled && m_current >= 0 && !m_snapshot.isEmpty()) m_settings.setValue(key(m_current), m_snapshot);
    m_settings.sync();
}
OrientationLayouts* OrientationLayouts::install(QMainWindow* window) {
    if (!window) return nullptr;
    if (auto existing = window->findChild<OrientationLayouts*>("orientationLayouts", Qt::FindDirectChildrenOnly)) return existing;
    return new OrientationLayouts(window);
}
void OrientationLayouts::applyEnabled(bool enabled) {
    for (auto widget : QApplication::topLevelWidgets()) {
        if (auto window = qobject_cast<QMainWindow*>(widget))
            if (auto controller = window->findChild<OrientationLayouts*>("orientationLayouts")) controller->setEnabled(enabled);
    }
}
void OrientationLayouts::forgetSaved() {
    QSettings settings("BaronEdition", "Orchestrion");
    settings.remove("orientation_layout");
    for (auto widget : QApplication::topLevelWidgets())
        for (auto controller : widget->findChildren<OrientationLayouts*>()) {
            controller->m_settings.remove("orientation_layout");
            controller->m_snapshot.clear();controller->m_pendingLayout.clear();controller->m_expectedDocks.clear();
            controller->captureNow();
        }
}
void OrientationLayouts::setEnabled(bool enabled) {
    if (enabled == m_enabled) return;
    if (!enabled) captureNow();
    m_enabled = enabled;
    m_save.stop(); m_switch.stop(); m_settle.stop(); m_applying = false;
    if (enabled) {
        m_current = -1;
        if (m_observeScreen) observeScreen();
        else if (m_observed >= 0) m_switch.start();
    }
}
void OrientationLayouts::setOrientation(Qt::ScreenOrientation orientation) {
    int value = -1;
    if (orientation == Qt::PortraitOrientation || orientation == Qt::InvertedPortraitOrientation) value = 1;
    if (orientation == Qt::LandscapeOrientation || orientation == Qt::InvertedLandscapeOrientation) value = 0;
    if (value < 0) return;
    m_observed = value;
    if (!m_enabled || value == m_current) return;
    m_save.stop();
    m_switch.start();
}
void OrientationLayouts::observeScreen() {
    if (!m_window || !m_observeScreen || !m_ready) return;
    auto screen = m_window->windowHandle() ? m_window->windowHandle()->screen() : QGuiApplication::primaryScreen();
    if (!screen) return;
    if (screen != m_screen) {
        if (m_screen) disconnect(m_screen, nullptr, this, nullptr);
        m_screen = screen;
        screen->setOrientationUpdateMask(Qt::PortraitOrientation | Qt::InvertedPortraitOrientation
            | Qt::LandscapeOrientation | Qt::InvertedLandscapeOrientation);
        connect(screen, &QScreen::orientationChanged, this, [this] { observeScreen(); });
        connect(screen, &QScreen::geometryChanged, this, [this] { observeScreen(); });
    }
    auto orientation = screen->orientation();
    if (orientation == Qt::PrimaryOrientation)
        orientation = screen->geometry().height() > screen->geometry().width() ? Qt::PortraitOrientation : Qt::LandscapeOrientation;
    setOrientation(orientation);
}
bool OrientationLayouts::docksReady() const {
    if(!m_window)return false;
    for(const auto& name:m_expectedDocks)if(!m_window->findChild<QDockWidget*>(name))return false;
    return true;
}
bool OrientationLayouts::canvasOnly() const {
    if (!m_window) return true;
    for (auto action : m_window->findChildren<QAction*>())
        if (action->objectName() == "view_show_canvas_only" && action->isChecked()) return true;
    return false;
}
void OrientationLayouts::watchChildren() {
    if (!m_window) return;
    QList<QObject*> objects;
    for (auto dock : m_window->findChildren<QDockWidget*>()) objects.append(dock);
    for (auto toolbar : m_window->findChildren<QToolBar*>()) objects.append(toolbar);
    for (auto action : m_window->findChildren<QAction*>())
        if (action->objectName() == "view_show_canvas_only") objects.append(action);
    for (auto object : objects) {
        if (m_watched.contains(object)) continue;
        m_watched.insert(object); object->installEventFilter(this);
        if(m_current>=0 && qobject_cast<QDockWidget*>(object) && !m_pendingLayout.isEmpty())
            QTimer::singleShot(0,this,[this] {
                if(!m_window||m_pendingLayout.isEmpty()||!docksReady())return;
                m_applying=true;m_window->restoreState(m_pendingLayout);m_pendingLayout.clear();m_settle.start();
            });
        connect(object, &QObject::destroyed, this, [this, object] { m_watched.remove(object); });
        if (auto action = qobject_cast<QAction*>(object))
            connect(action, &QAction::toggled, this, [this](bool checked) {
                if (!checked && m_enabled && m_current != m_observed) m_switch.start();
                if (!checked) m_save.start();
            });
    }
}
void OrientationLayouts::captureNow() {
    if (!m_window || !m_ready || !m_enabled || m_applying || !m_pendingLayout.isEmpty() || m_current < 0 || m_current != m_observed
        || !m_window->isVisible() || canvasOnly() || QGuiApplication::inputMethod()->isVisible()) return;
    const auto state = m_window->saveState();
    if (state == m_snapshot) return;
    m_snapshot = state;
    QStringList names;
    for(auto dock:m_window->findChildren<QDockWidget*>())if(!dock->objectName().isEmpty())names.append(dock->objectName());
    m_settings.setValue(key(m_current)+"_dock_names",names);
    m_settings.setValue(key(m_current), state);
    m_settings.sync();
}
void OrientationLayouts::switchLayout() {
    if (!m_window || !m_ready || !m_enabled || m_observed < 0 || !m_window->isVisible() || canvasOnly()) return;
    if (m_observed == m_current) return;
    if (m_current >= 0 && !m_snapshot.isEmpty()) m_settings.setValue(key(m_current), m_snapshot);
    m_current = m_observed;
    m_applying = true;
    m_snapshot.clear();m_pendingLayout.clear();
    m_expectedDocks=m_settings.value(key(m_current)+"_dock_names").toStringList();
    const auto state = m_settings.value(key(m_current)).toByteArray();
    if (!state.isEmpty()) {
        m_snapshot=state;
        m_window->restoreState(state);
        if(!docksReady())m_pendingLayout=state;
    }
    m_settle.start();
}
bool OrientationLayouts::eventFilter(QObject* object, QEvent* event) {
    if (object == m_window && (event->type() == QEvent::ChildAdded || event->type() == QEvent::Show)) {
        QTimer::singleShot(0, this, [this] { watchChildren(); if (m_observeScreen) observeScreen(); });
    }
    if (event->type()==QEvent::Close && object==m_window) { captureNow(); m_settings.sync(); }
    if (m_ready && m_enabled && !m_applying && m_current >= 0) {
        switch (event->type()) {
        case QEvent::Move: case QEvent::Resize: case QEvent::Show: case QEvent::Hide:
        case QEvent::LayoutRequest: case QEvent::ParentChange:
            if (m_observeScreen) observeScreen();
            if (m_current == m_observed) m_save.start();
            break;
        default: break;
        }
    }
    return QObject::eventFilter(object, event);
}
