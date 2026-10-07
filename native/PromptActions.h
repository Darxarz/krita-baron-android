// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "PromptLogic.h"
#include <QElapsedTimer>
#include <QPointer>
#include <QPoint>
#include <QSet>
#include <QTimer>

class PromptEditor;
class OrchestrionClient;
class QFrame;
class QLabel;
class QGridLayout;
class QScrollArea;
class PromptActions : public QObject {
    Q_OBJECT
public:
    explicit PromptActions(PromptEditor* editor);
    ~PromptActions() override;
    void setTarget(PromptEditor* target);
    void setClient(OrchestrionClient* client);
    void setModel(const QString& model, const QString& family);
    void open(int position);
    void close();
    bool isOpen() const { return m_index >= 0; }
    void refresh();
    void perform(const QString& action, const QJsonValue& argument = {});
    void pasteAfter(int index, const QString& text);
    QJsonArray disabled() const { return m_parked; }
    void restoreDisabled(const QJsonArray& entries);
    QString message(const QString& key) const;
protected:
    bool eventFilter(QObject* object, QEvent* event) override;
private:
    void buildBar();
    void positionBar();
    void updateBubbles();
    void commit(const QString& text, int caret, int index = -1);
    void restore(int index);
    void organize();
    void translate(const QString& mode);
    QJsonObject options();
    PromptEditor* m_editor;
    QPointer<PromptEditor> m_target;
    QPointer<OrchestrionClient> m_client;
    PromptLogic m_logic;
    QJsonObject m_messages;
    QJsonArray m_segments, m_parked;
    QSet<int> m_selected;
    QPointer<QFrame> m_bar;
    QScrollArea* m_disabled;
    QTimer m_refresh;
    int m_index = -1;
    quint64 m_epoch = 0;
    bool m_more = false, m_multi = false, m_busy = false, m_drag = false;
    QPoint m_press;
    QElapsedTimer m_pressed;
    QString m_model, m_family, m_note;
};
