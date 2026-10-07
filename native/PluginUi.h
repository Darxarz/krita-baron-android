// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QIcon>
#include <QMap>
#include <QPlainTextEdit>
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>
#include <QListWidget>
#include <QPointer>

class QAbstractButton;
class QCompleter;
class QListView;
class QStringListModel;
class QTimer;
class QPushButton;
class PromptActions;
class OrchestrionClient;
class HistoryList : public QListWidget {
    Q_OBJECT
public:
    explicit HistoryList(QWidget* parent = nullptr);
Q_SIGNALS:
    void emptyClicked();
    void applyRequested(QListWidgetItem* item);
    void contextRequested(QListWidgetItem* item, const QPoint& globalPosition);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
private:
    void updateButtons();
    void updatePalette();
    QPoint m_pressPosition;
    bool m_emptyPress = false, m_doubleClick = false;
    bool m_scrollGesture = false;
    bool m_updatingPalette = false;
    QPushButton *m_apply, *m_context;
};
class IntervalSlider : public QWidget {
    Q_OBJECT
public:
    explicit IntervalSlider(QWidget* parent = nullptr);
    int low() const { return m_low; }
    int high() const { return m_high; }
    void setInterval(int low, int high);
    QSize sizeHint() const override;
Q_SIGNALS:
    void intervalChanged(int low, int high);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    int position(int value) const;
    int value(int position) const;
    int m_low = 0, m_high = 20, m_dragged = 0;
};
class PromptEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    enum class CompletionDisplay { Automatic, Popup, Embedded };
    explicit PromptEditor(QWidget* parent = nullptr, bool negative = false,
        CompletionDisplay display = CompletionDisplay::Automatic);
    ~PromptEditor() override;
    void setLoraNames(const QStringList& names);
    void setLoraCatalog(const QJsonArray& catalog);
    void insertLora(const QJsonObject& lora, double strength = 1);
    void reloadTags();
    void resetInputState();
    void replacePromptText(const QString& text);
    void setPromptActionTarget(PromptEditor* target);
    void setPromptActionClient(OrchestrionClient* client);
    void setPromptActionModel(const QString& model, const QString& family);
    QJsonArray disabledFragments() const;
    void restoreDisabledFragments(const QJsonArray& fragments);
Q_SIGNALS:
    void activated();
    void heightChanged(int height);
    void contextActionsChanged();

protected:
    bool eventFilter(QObject* object, QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QMimeData* createMimeDataFromSelection() const override;
    void insertFromMimeData(const QMimeData* source) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    friend class PromptActions;
    PromptActions* m_actions;
    int m_actionPasteIndex = -1;
    void updatePalette();
    void updateCompletion();
    void insertCompletion(const QString& value);
    void hideCompletion();
    bool positionCompletion();
    void queueClipboardPaste();
    void insertClipboardText(const QString& text);
    void copySelection(bool cut);
    QCompleter* m_completer;
    QStringListModel* m_loraModel;
    QTimer* m_completionTimer;
    QPointer<QListView> m_completionList;
    QMap<QString, QString> m_triggers;
    QString m_completionPrefix;
    int m_completionStart = 0;
    bool m_loraCompletion = false, m_inserting = false;
    bool m_embeddedCompletion = false, m_composing = false;
    bool m_pastePending = false;
    quint64 m_inputEpoch = 0;
    int m_loggedPaintRevision = -1;
    bool m_negative, m_resizing = false;
    int m_dragY = 0, m_startHeight = 0;
};
namespace PluginUi {
QIcon icon(const QString& name, const QWidget* widget);
void setIcon(QAbstractButton* button, const QString& name);
void refresh(QWidget* widget);
void copyText(const QString& text);
QString architectureIcon(const QString& family);
}
