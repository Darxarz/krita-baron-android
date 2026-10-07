// SPDX-License-Identifier: GPL-3.0-or-later
#include "PluginUi.h"
#include "PromptActions.h"
#include "TagModel.h"
#include "BaronDiagnostics.h"
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMimeData>
#include <QFocusEvent>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QListView>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSettings>
#include <QStringListModel>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextBlock>
#include <QTextLayout>
#include <QTimer>
#include <QVariant>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QStyledItemDelegate>
#include <QPushButton>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QResizeEvent>
#include <QScopedValueRollback>
#ifdef Q_OS_ANDROID
#include <QtAndroid>
#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#endif

HistoryList::HistoryList(QWidget* parent) : QListWidget(parent) {
    setMovement(QListView::Static);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setDragEnabled(false);
    setAcceptDrops(false);
    viewport()->setAcceptDrops(false);
    setDropIndicatorShown(false);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    QScroller::grabGesture(viewport(), QScroller::TouchGesture);
    auto scroller = QScroller::scroller(viewport());
    auto properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(properties);
    m_apply = new QPushButton(PluginUi::icon("apply", this), QCoreApplication::translate("BaronPanel", "Apply"), viewport());
    m_apply->setObjectName("historyApplyOverlay");
    m_context = new QPushButton(PluginUi::icon("context", this), "", viewport());
    m_context->setObjectName("historyContextOverlay");
    for (auto button : { m_apply, m_context }) {
        button->hide();
        button->setFixedHeight(fontMetrics().height() + 8);
    }
    updatePalette();
    connect(m_apply, &QPushButton::clicked, this, [this] {
        if (!selectedItems().isEmpty()) emit applyRequested(selectedItems().first());
    });
    connect(m_context, &QPushButton::clicked, this, [this] {
        if (!selectedItems().isEmpty()) emit contextRequested(selectedItems().first(), m_context->mapToGlobal(QPoint(0, m_context->height())));
    });
    connect(this, &QListWidget::itemSelectionChanged, this, &HistoryList::updateButtons);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { updateButtons(); });
    connect(scroller, &QScroller::stateChanged, this, [this](QScroller::State state) {
        if (state == QScroller::Dragging || state == QScroller::Scrolling) {
            m_scrollGesture = true;
            m_emptyPress = false;
            m_doubleClick = false;
        }
        updateButtons();
    });
}
void HistoryList::updateButtons() {
    const auto state = QScroller::scroller(viewport())->state();
    if (state == QScroller::Dragging || state == QScroller::Scrolling) {
        m_apply->hide(); m_context->hide(); return;
    }
    const auto items = selectedItems();
    if (items.isEmpty()) { m_apply->hide(); m_context->hide(); return; }
    const auto rect = visualItemRect(items.first());
    if (!rect.intersects(viewport()->rect())) { m_apply->hide(); m_context->hide(); return; }
    const int height = fontMetrics().height() + 8;
    const bool context = rect.width() >= .6 * iconSize().width();
    const int contextWidth = context ? height : 0;
    m_context->setGeometry(rect.right() - contextWidth - 2, rect.bottom() - height - 2, contextWidth, height);
    m_context->setVisible(context);
    m_apply->setText(fontMetrics().horizontalAdvance(QCoreApplication::translate("BaronPanel", "Apply")) < .35 * rect.width()
        ? QCoreApplication::translate("BaronPanel", "Apply") : QString());
    m_apply->setGeometry(rect.left() + 3, rect.bottom() - height - 2, qMax(1, rect.width() - contextWidth - 8), height);
    m_apply->show();
    m_apply->raise(); m_context->raise();
}
void HistoryList::resizeEvent(QResizeEvent* event) {
    QListWidget::resizeEvent(event);
    updateButtons();
}
void HistoryList::updatePalette() {
    if (m_updatingPalette) return;
    QScopedValueRollback<bool> updating(m_updatingPalette, true);
    const bool dark = palette().color(QPalette::Window).lightness() < 128;
    const QString grey = dark ? "#888888" : "#666666";
    const QString normal = dark ? "rgba(64, 64, 64, 170)" : "rgba(240, 240, 240, 160)";
    const QString hover = dark ? "rgba(72, 72, 72, 210)" : "rgba(240, 240, 240, 200)";
    setStyleSheet(QString("QListWidget { background: transparent; } QListWidget::item:selected { border: 1px solid %1; }").arg(grey));
    for (auto button : { m_apply, m_context }) {
        button->setStyleSheet(QString("QPushButton { border: 1px solid %1; background: %2; padding: 2px; } QPushButton:hover { background: %3; }").arg(grey, normal, hover));
        button->setIcon(PluginUi::icon(button == m_apply ? "apply" : "context", this));
    }
}
void HistoryList::changeEvent(QEvent* event) {
    QListWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) updatePalette();
}

void HistoryList::mousePressEvent(QMouseEvent* event) {
    const auto state = QScroller::scroller(viewport())->state();
    m_scrollGesture = state == QScroller::Dragging || state == QScroller::Scrolling;
    m_pressPosition = event->pos();
    m_emptyPress = event->button() == Qt::LeftButton && event->modifiers() == Qt::NoModifier
        && !itemAt(event->pos());
    QListWidget::mousePressEvent(event);
}
void HistoryList::mouseReleaseEvent(QMouseEvent* event) {
    if (m_scrollGesture || (event->pos() - m_pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
        m_scrollGesture = false; m_emptyPress = false; m_doubleClick = false;
        QMouseEvent cancelled(event->type(), QPointF(-10000, -10000), event->button(), event->buttons(), event->modifiers());
        QListWidget::mouseReleaseEvent(&cancelled);
        event->accept();
        return;
    }
    if (m_doubleClick) { m_doubleClick = false; event->accept(); return; }
    const bool clicked = m_emptyPress && event->button() == Qt::LeftButton
        && !itemAt(event->pos()) && viewport()->rect().contains(event->pos())
        && (event->pos() - m_pressPosition).manhattanLength() < QApplication::startDragDistance();
    m_emptyPress = false;
    QListWidget::mouseReleaseEvent(event);
    if (clicked) emit emptyClicked();
}
void HistoryList::mouseDoubleClickEvent(QMouseEvent* event) {
    auto item = itemAt(event->pos());
    if (event->button() == Qt::LeftButton && item && (item->flags() & Qt::ItemIsSelectable)) {
        m_doubleClick = true;
        setCurrentItem(item);
        emit itemDoubleClicked(item);
        event->accept();
    } else QListWidget::mouseDoubleClickEvent(event);
}
IntervalSlider::IntervalSlider(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(50);
}
QSize IntervalSlider::sizeHint() const {
    return QSize(150, qMax(24, style()->pixelMetric(QStyle::PM_SliderThickness, nullptr, this)));
}
int IntervalSlider::position(int v) const {
    const int handle = style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
    return handle / 2 + QStyle::sliderPositionFromValue(0, 20, v, qMax(1, width() - handle));
}
int IntervalSlider::value(int x) const {
    const int handle = style()->pixelMetric(QStyle::PM_SliderLength, nullptr, this);
    return QStyle::sliderValueFromPosition(0, 20, x - handle / 2, qMax(1, width() - handle));
}
void IntervalSlider::setInterval(int low, int high) {
    low = qBound(0, low, 20);
    high = qBound(low, high, 20);
    if (low == m_low && high == m_high) return;
    m_low = low; m_high = high;
    update();
    emit intervalChanged(low, high);
}
void IntervalSlider::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    QStyleOptionSlider opt;
    opt.initFrom(this);
    opt.orientation = Qt::Horizontal;
    opt.minimum = 0; opt.maximum = 20;
    opt.subControls = QStyle::SC_SliderGroove;
    style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
    painter.fillRect(QRect(position(m_low), height() / 2 - 1, position(m_high) - position(m_low), 3),
        palette().brush(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::Highlight));
    for (int i = 1; i <= 2; ++i) {
        opt.sliderPosition = opt.sliderValue = i == 1 ? m_low : m_high;
        opt.subControls = QStyle::SC_SliderHandle;
        opt.activeSubControls = m_dragged == i ? QStyle::SC_SliderHandle : QStyle::SC_None;
        style()->drawComplexControl(QStyle::CC_Slider, &opt, &painter, this);
    }
}
void IntervalSlider::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) { event->ignore(); return; }
    setFocus(Qt::MouseFocusReason);
    m_dragged = qAbs(event->pos().x() - position(m_low)) < qAbs(event->pos().x() - position(m_high)) ? 1 : 2;
    mouseMoveEvent(event);
}
void IntervalSlider::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragged == 1) setInterval(qMin(value(event->pos().x()), m_high), m_high);
    else if (m_dragged == 2) setInterval(m_low, qMax(value(event->pos().x()), m_low));
    else { event->ignore(); return; }
    event->accept();
}
void IntervalSlider::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) { mouseMoveEvent(event); m_dragged = 0; update(); }
}
void IntervalSlider::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        const int direction = event->key() == Qt::Key_Left ? -1 : 1;
        if (event->modifiers() & Qt::ShiftModifier) setInterval(m_low, qMax(m_low, m_high + direction));
        else setInterval(qMin(m_high, m_low + direction), m_high);
        event->accept();
    } else QWidget::keyPressEvent(event);
}

namespace PluginUi {
void copyText(const QString& text) {
#ifdef Q_OS_ANDROID
    QtAndroid::runOnAndroidThread([text] {
        const auto value = QAndroidJniObject::fromString(text);
        QAndroidJniObject::callStaticMethod<void>("org/baron/krita/PromptClipboard", "writeText",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            QtAndroid::androidActivity().object<jobject>(), value.object<jstring>());
        QAndroidJniEnvironment environment;
        if (environment->ExceptionCheck()) environment->ExceptionClear();
    });
#else
    QApplication::clipboard()->setText(text);
#endif
}
QIcon icon(const QString& name, const QWidget* widget) {
    const auto theme = widget->palette().color(QPalette::Window).lightness() < 128
        ? QStringLiteral("dark")
        : QStringLiteral("light");
    const QString stem = QStringLiteral(":/baron/icons/") + name + "-" + theme;
    return QIcon(QFile::exists(stem + ".svg") ? stem + ".svg" : stem + ".png");
}
void setIcon(QAbstractButton* button, const QString& name) {
    button->setProperty("pluginIcon", name);
    button->setIcon(icon(name, button));
}
void refresh(QWidget* widget) {
    for (auto button : widget->findChildren<QAbstractButton*>()) {
        const auto name = button->property("pluginIcon").toString();
        if (!name.isEmpty())
            button->setIcon(icon(name, button));
    }
}
QString architectureIcon(const QString& family) {
    const auto name = family.toLower();
    if (name.contains("qwen"))
        return "sd-version-qwen";
    if (name.contains("krea"))
        return "sd-version-krea2";
    if (name.contains("flux 2") || name.contains("klein"))
        return "sd-version-flux-2";
    if (name.contains("flux"))
        return "sd-version-flux";
    if (name.contains("illustrious") || name.contains("noobai"))
        return "sd-version-illu";
    if (name.contains("z-image"))
        return "sd-version-z-image";
    if (name.contains("anima"))
        return "sd-version-anima";
    if (name.contains("sd1") || name.contains("sd 1"))
        return "sd-version-15";
    return "sd-version-xl";
}
}

namespace {
QJsonArray cachedLoraCatalog;
class PromptHighlighter : public QSyntaxHighlighter {
public:
    explicit PromptHighlighter(QTextDocument* document)
        : QSyntaxHighlighter(document) { }
    void highlightBlock(const QString& text) override {
        if (text.size() > 128) BaronDiagnostics::record("highlight.begin", {{ "characters", text.size() }});
        QTextCharFormat format;
        format.setForeground(QColor("#d07a40"));
        auto matches = QRegularExpression("<lora:[^>]+>").globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), format);
        }
        if (text.size() > 128) BaronDiagnostics::record("highlight.end");
    }
};
}
namespace {
class PromptCompletionDelegate : public QStyledItemDelegate {
public:
    explicit PromptCompletionDelegate(QObject* parent, bool touch = false)
        : QStyledItemDelegate(parent), m_touch(touch) { }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), m_touch ? 40 : option.fontMetrics.height() + 4));
        return size;
    }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto metadata = index.data(Qt::UserRole + 1).toString();
        if (metadata.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        painter->save();
        const bool selected = option.state & QStyle::State_Selected;
        const auto background = selected ? option.palette.highlight().color()
            : index.data(Qt::BackgroundRole).value<QColor>();
        painter->fillRect(option.rect, background);
        const auto textColor = selected ? option.palette.highlightedText().color() : option.palette.text().color();
        painter->setPen(textColor);
        auto smallFont = option.font;
        smallFont.setPointSize(qMax(7, smallFont.pointSize() - 2));
        smallFont.setItalic(true);
        const int metadataWidth = QFontMetrics(smallFont).horizontalAdvance(metadata) + 10;
        const auto rect = option.rect.adjusted(4, 0, -4, 0);
        auto textRect = rect; textRect.setRight(qMax(rect.left(), rect.right() - metadataWidth));
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
            option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, textRect.width()));
        painter->setFont(smallFont);
        painter->setPen(textColor.lighter(150));
        painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter, metadata);
        painter->restore();
    }
private:
    bool m_touch;
};
}
PromptEditor::PromptEditor(QWidget* parent, bool negative, CompletionDisplay display)
    : QPlainTextEdit(parent)
    , m_negative(negative) {
    setFrameShape(QFrame::NoFrame);
    setTabChangesFocus(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    new PromptHighlighter(document());
    m_completer = new QCompleter(this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_loraModel = new QStringListModel(this);
    m_completer->setModel(m_loraModel);
    m_embeddedCompletion = display == CompletionDisplay::Embedded;
#ifdef Q_OS_ANDROID
    m_embeddedCompletion = display != CompletionDisplay::Popup;
#endif
    if (m_embeddedCompletion) {
        qApp->installEventFilter(this);
        connect(QGuiApplication::inputMethod(), &QInputMethod::keyboardRectangleChanged,
            this, &PromptEditor::positionCompletion);
    } else {
        m_completer->setWidget(this);
        m_completer->popup()->setItemDelegate(new PromptCompletionDelegate(m_completer->popup()));
    }
    m_completionTimer = new QTimer(this);
    m_completionTimer->setSingleShot(true); m_completionTimer->setInterval(50);
    connect(m_completionTimer,&QTimer::timeout,this,&PromptEditor::updateCompletion);
    connect(this,&QPlainTextEdit::textChanged,this,[this]{if(!m_inserting&&hasFocus())m_completionTimer->start();});
    connect(this,&QPlainTextEdit::cursorPositionChanged,this,[this]{if(!m_inserting&&hasFocus())m_completionTimer->start();});
    connect(this, &QPlainTextEdit::selectionChanged, this, [this] {
        if (textCursor().hasSelection()) {
            hideCompletion();
            m_completionTimer->stop();
        }
    });
    connect(m_completer, QOverload<const QString&>::of(&QCompleter::activated), this,&PromptEditor::insertCompletion);
    reloadTags();
    setLoraCatalog(cachedLoraCatalog);
    updatePalette();
    m_actions = new PromptActions(this);
}
void PromptEditor::setPromptActionTarget(PromptEditor* target) { m_actions->setTarget(target); }
void PromptEditor::setPromptActionClient(OrchestrionClient* client) { m_actions->setClient(client); }
void PromptEditor::setPromptActionModel(const QString& model, const QString& family) { m_actions->setModel(model, family); }
QJsonArray PromptEditor::disabledFragments() const { return m_actions->disabled(); }
void PromptEditor::restoreDisabledFragments(const QJsonArray& fragments) { m_actions->restoreDisabled(fragments); }
PromptEditor::~PromptEditor() {
    if (qApp) qApp->removeEventFilter(this);
    delete m_completionList;
    m_completionTimer->stop();
    m_completer->setWidget(nullptr);
    delete m_completer;
}
void PromptEditor::setLoraNames(const QStringList& names) {
    QStringList normalized;
    for(auto name:names) {
        name.replace('\\','/');if(name.endsWith(".safetensors",Qt::CaseInsensitive))name.chop(12);
        normalized.append(name);
    }
    m_loraModel->setStringList(normalized);
}
void PromptEditor::setLoraCatalog(const QJsonArray& catalog) {
    cachedLoraCatalog = catalog;
    QStringList names;m_triggers.clear();
    for(auto entry:catalog) {
        const auto data=entry.toObject();if(data["kind"]!="lora")continue;
        QString name=data["name"].toString().replace('\\','/');names.append(name);
        if(name.endsWith(".safetensors",Qt::CaseInsensitive))name.chop(12);
        QStringList words;for(auto word:data["triggerWords"].toArray())words.append(word.toString());
        m_triggers[name]=words.join(", ");
    }
    setLoraNames(names);
}
void PromptEditor::reloadTags() {
    TagModel::shared()->reload(QSettings("BaronEdition","Orchestrion").value("tagFiles",QStringList{"Danbooru","e621"}).toStringList());
}
void PromptEditor::updateCompletion() {
    if (textCursor().hasSelection()) {
        hideCompletion();
        m_completionTimer->stop();
        return;
    }
    if(m_inserting||m_composing||m_pastePending||!hasFocus())return;
    const auto before=toPlainText().left(textCursor().position());
    const int lora=before.lastIndexOf("<lora:", -1, Qt::CaseInsensitive);
    const auto suffix=lora<0?QString():before.mid(lora+6);
    m_loraCompletion=lora>=0&&!suffix.contains('>')&&!suffix.contains(':');
    if(m_loraCompletion) {
        m_completionStart=lora+6;m_completionPrefix=suffix;m_completer->setModel(m_loraModel);
    } else {
        int start=before.size();
        const QString separators="()>,|{\n";
        while(start>0) {
            if(separators.contains(before[start-1])&&(start<2||before[start-2]!='\\'))break;
            --start;
        }
        while(start<before.size()&&before[start].isSpace())++start;
        m_completionStart=start;m_completionPrefix=before.mid(start);
        if(m_completionPrefix.trimmed().size()<3||m_completionPrefix.startsWith('<')){hideCompletion();return;}
        m_completer->setModel(TagModel::shared());
    }
    QString prefix=m_completionPrefix;prefix.replace("\\(","(").replace("\\)",")");
    m_completer->setCompletionPrefix(prefix);
    if(!m_completer->completionCount()){hideCompletion();return;}
    if (m_embeddedCompletion) {
        if (m_completionList && m_completionList->parentWidget() != window()) {
            m_completionList->hide();
            m_completionList->setParent(window());
        }
        if (!m_completionList) {
            m_completionList = new QListView(window());
            m_completionList->setObjectName("promptCompletionList");
            m_completionList->setWindowFlags(Qt::Widget);
            m_completionList->setAttribute(Qt::WA_ShowWithoutActivating);
            m_completionList->setFocusPolicy(Qt::NoFocus);
            m_completionList->viewport()->setFocusPolicy(Qt::NoFocus);
            m_completionList->setEditTriggers(QAbstractItemView::NoEditTriggers);
            m_completionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_completionList->setSelectionMode(QAbstractItemView::SingleSelection);
            m_completionList->setItemDelegate(new PromptCompletionDelegate(m_completionList, true));
            connect(m_completionList, &QListView::clicked, this, [this](const QModelIndex& index) {
                insertCompletion(index.data(Qt::EditRole).toString());
            });
        }
        m_completionList->setModel(m_completer->completionModel());
        m_completionList->setFont(font());
        m_completionList->setPalette(palette());
        m_completionList->setCurrentIndex(m_completer->completionModel()->index(0, 0));
        if (!positionCompletion()) return;
        m_completionList->show();
        m_completionList->raise();
        return;
    }
    auto popup=cursorRect();popup.setWidth(qMax(220,qMin(560,width()-20)));
    m_completer->complete(popup);
    m_completer->popup()->setCurrentIndex(m_completer->completionModel()->index(0,0));
}
void PromptEditor::insertCompletion(const QString& value) {
    m_inserting=true;m_completionTimer->stop();
    auto cursor=textCursor();cursor.setPosition(m_completionStart,QTextCursor::KeepAnchor);
    QString text=value;
    if(m_loraCompletion)text+=QString(":1>"+(m_triggers.value(value).isEmpty()?QString():QString(" "+m_triggers.value(value))));
    else text.replace("(","\\(").replace(")","\\)");
    cursor.insertText(text);setTextCursor(cursor);hideCompletion();m_inserting=false;
}
void PromptEditor::insertLora(const QJsonObject& lora, double strength) {
    QString name=lora["name"].toString().replace('\\','/');
    if(name.endsWith(".safetensors",Qt::CaseInsensitive))name.chop(12);
    if(name.isEmpty()||name.contains(':')||name.contains('<')||name.contains('>'))return;
    const QString tag=QString("<lora:%1:%2>").arg(name,QString::number(strength,'g',8));
    const auto text=toPlainText();auto matches=QRegularExpression("<lora:([^:<>]+)(?::[^:<>]*)?>",QRegularExpression::CaseInsensitiveOption).globalMatch(text);
    auto cursor=textCursor();bool existing=false;
    while(matches.hasNext()) {
        const auto match=matches.next();QString normalized=match.captured(1).replace('\\','/');
        if(normalized.endsWith(".safetensors",Qt::CaseInsensitive))normalized.chop(12);
        if(normalized.compare(name,Qt::CaseInsensitive)==0){cursor.setPosition(match.capturedStart());cursor.setPosition(match.capturedEnd(),QTextCursor::KeepAnchor);existing=true;break;}
    }
    QString fill=tag;
    if(!existing) {
        QStringList words;for(auto word:lora["triggerWords"].toArray())words.append(word.toString());
        if(!words.isEmpty())fill+=QString(" "+words.join(", "));
        if(cursor.selectionStart()>0&&!text[cursor.selectionStart()-1].isSpace())fill.prepend(' ');
        if(cursor.selectionEnd()<text.size()&&!text[cursor.selectionEnd()].isSpace())fill.append(' ');
    }
    m_inserting=true;m_completionTimer->stop();cursor.insertText(fill);setTextCursor(cursor);hideCompletion();m_inserting=false;
}
void PromptEditor::hideCompletion() {
    if (!m_embeddedCompletion) m_completer->popup()->hide();
    if (m_completionList) m_completionList->hide();
}
bool PromptEditor::positionCompletion() {
    if (!m_completionList) return false;
    auto host = m_completionList->parentWidget();
    if (!host) return false;
    const auto caret = QRect(host->mapFromGlobal(viewport()->mapToGlobal(cursorRect().topLeft())), cursorRect().size());
    auto area = host->rect().adjusted(4, 4, -4, -4);
    const auto keyboard = QGuiApplication::inputMethod()->keyboardRectangle().toAlignedRect();
    if (QGuiApplication::inputMethod()->isVisible() && !keyboard.isEmpty())
        area.setBottom(qMin(area.bottom(), keyboard.top() - 4));
    if (area.width() <= 0 || area.height() <= 0) {
        m_completionList->hide();
        return false;
    }
    const int rowHeight = qMax(m_completionList->sizeHintForRow(0), 40);
    const int desiredHeight = qMin(5, m_completer->completionCount()) * rowHeight + 4;
    const int below = qMax(0, area.bottom() - caret.bottom());
    const int above = qMax(0, caret.top() - area.top());
    const bool upward = below < desiredHeight && above > below;
    const int height = qMin(desiredHeight, upward ? above : below);
    if (height < rowHeight) {
        m_completionList->hide();
        return false;
    }
    const int width = qMin(qMax(220, this->width()), area.width());
    const int left = qBound(area.left(), caret.left(), area.right() - width + 1);
    const int top = upward ? caret.top() - height : caret.bottom() + 1;
    m_completionList->setGeometry(left, top, width, height);
    return true;
}
bool PromptEditor::eventFilter(QObject* object, QEvent* event) {
    if (m_completionList && m_completionList->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto mouse = static_cast<QMouseEvent*>(event);
            if (!QRect(m_completionList->mapToGlobal(QPoint()), m_completionList->size()).contains(mouse->globalPos())) {
                hideCompletion();
                m_completionTimer->stop();
            }
        } else if (object == m_completionList->parentWidget()
            && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
            positionCompletion();
        }
    }
    return QPlainTextEdit::eventFilter(object, event);
}
void PromptEditor::focusOutEvent(QFocusEvent* event) {
    m_actionPasteIndex = -1;
    if (m_embeddedCompletion) {
        hideCompletion();
        m_completionTimer->stop();
    }
    QPlainTextEdit::focusOutEvent(event);
}
void PromptEditor::inputMethodEvent(QInputMethodEvent* event) {
    if (isReadOnly()) { QPlainTextEdit::inputMethodEvent(event); return; }
    BaronDiagnostics::record("ime.begin", {{ "commit", event->commitString().size() }, { "preedit", event->preeditString().size() },
        { "replacement-start", event->replacementStart() }, { "replacement-length", event->replacementLength() }});
    const auto& commit = event->commitString();
    const bool bulkCommit = commit.size() > 32 || commit.contains(' ') || commit.contains('\n') || commit.contains('\t');
    QScopedValueRollback<bool> inserting(m_inserting, m_inserting || bulkCommit);
    if (bulkCommit) {
        hideCompletion();
        m_completionTimer->stop();
    }
    const bool hadPreedit = m_composing || !textCursor().block().layout()->preeditAreaText().isEmpty();
    m_composing = !event->preeditString().isEmpty();
    QList<QInputMethodEvent::Attribute> attributes;
    QList<QInputMethodEvent::Attribute> selections;
    for (const auto& attribute : event->attributes()) {
        if (m_embeddedCompletion && event->preeditString().isEmpty()
            && attribute.type == QInputMethodEvent::Selection) selections.append(attribute);
        else attributes.append(attribute);
    }
    if (selections.isEmpty()) {
        QPlainTextEdit::inputMethodEvent(event);
    } else {
        hideCompletion();
        m_completionTimer->stop();
        // Qt 5 scrolls Selection attributes inside an unfinished text edit block.
        // Finish the commit/preedit cancellation before moving the cursor (Samsung crash).
        if (hadPreedit || !commit.isEmpty() || event->replacementLength() != 0) {
            QInputMethodEvent textEvent(event->preeditString(), attributes);
            textEvent.setCommitString(commit, event->replacementStart(), event->replacementLength());
            QPlainTextEdit::inputMethodEvent(&textEvent);
        }
        auto cursor = textCursor();
        const int lastPosition = document()->characterCount() - 1;
        for (const auto& selection : selections) {
            const qint64 position = qint64(cursor.block().position()) + selection.start;
            cursor.setPosition(int(qBound(qint64(0), position, qint64(lastPosition))));
            cursor.setPosition(int(qBound(qint64(0), position + selection.length, qint64(lastPosition))), QTextCursor::KeepAnchor);
        }
        auto layout = qobject_cast<QPlainTextDocumentLayout*>(document()->documentLayout());
        if (layout) layout->ensureBlockLayout(cursor.block());
        if (cursor.position() != textCursor().position() || cursor.anchor() != textCursor().anchor())
            setTextCursor(cursor);
        event->accept();
        BaronDiagnostics::record("ime.selection-after-layout", {{ "position", cursor.position() }, { "anchor", cursor.anchor() }});
    }
    if (m_composing || textCursor().hasSelection() || !selections.isEmpty()) {
        hideCompletion();
        m_completionTimer->stop();
    } else if (!bulkCommit && hasFocus()) m_completionTimer->start();
    BaronDiagnostics::record("ime.end");
}
void PromptEditor::insertClipboardText(const QString& text) {
    if (isReadOnly() || text.isEmpty()) { m_actionPasteIndex = -1; return; }
    if (m_actionPasteIndex >= 0) {
        const int index = m_actionPasteIndex; m_actionPasteIndex = -1;
        m_actions->pasteAfter(index, text); return;
    }
    QScopedValueRollback<bool> inserting(m_inserting, true);
    hideCompletion();
    m_completionTimer->stop();
    BaronDiagnostics::record("paste.insert.begin", {{ "characters", text.size() }, { "position", textCursor().position() }, { "anchor", textCursor().anchor() }});
    insertPlainText(text);
    BaronDiagnostics::record("paste.insert.end", {{ "revision", document()->revision() }});
    ensureCursorVisible();
    BaronDiagnostics::record("paste.cursor-visible.end");
}
void PromptEditor::resetInputState() {
    QScopedValueRollback<bool> inserting(m_inserting, true);
    ++m_inputEpoch;
    m_actionPasteIndex = -1;
    m_actions->close();
    m_pastePending = false;
    hideCompletion();
    m_completionTimer->stop();
    if (m_embeddedCompletion && hasFocus()) QGuiApplication::inputMethod()->reset();
    m_composing = false;
}
void PromptEditor::replacePromptText(const QString& text) {
    QScopedValueRollback<bool> inserting(m_inserting, true);
    resetInputState();
    setPlainText(text);
    if (hasFocus()) QGuiApplication::inputMethod()->update(Qt::ImQueryAll);
}
void PromptEditor::copySelection(bool cut) {
    if (!textCursor().hasSelection() || (cut && isReadOnly())) return;
    QScopedPointer<QMimeData> data(createMimeDataFromSelection());
    PluginUi::copyText(data->text());
    if (cut) {
        QScopedValueRollback<bool> inserting(m_inserting, true);
        hideCompletion();
        m_completionTimer->stop();
        auto cursor = textCursor();
        cursor.removeSelectedText();
        setTextCursor(cursor);
    }
}
void PromptEditor::insertFromMimeData(const QMimeData* source) {
    if (!source || isReadOnly()) return;
    const QString text = source->text();
    insertClipboardText(text);
}
QMimeData* PromptEditor::createMimeDataFromSelection() const {
    if (!m_embeddedCompletion) return QPlainTextEdit::createMimeDataFromSelection();
    auto data = new QMimeData;
    QString text = textCursor().selectedText();
    text.replace(QChar::ParagraphSeparator, '\n');
    text.replace(QChar::LineSeparator, '\n');
    text.replace(QChar::Nbsp, ' ');
    data->setText(text);
    return data;
}
void PromptEditor::queueClipboardPaste() {
    if (isReadOnly() || m_pastePending) return;
    hideCompletion();
    m_completionTimer->stop();
    m_pastePending = true;
    BaronDiagnostics::record("paste.request");
    // Android's edit popup dispatches Ctrl+V from inside a synchronous IME batch.
    const int revision = document()->revision();
    const int position = textCursor().position(), anchor = textCursor().anchor();
    const auto epoch = m_inputEpoch;
    QTimer::singleShot(0, this, [this, revision, position, anchor, epoch] {
        if (epoch != m_inputEpoch) return;
#ifdef Q_OS_ANDROID
        const QPointer<PromptEditor> editor(this);
        QtAndroid::runOnAndroidThread([editor, revision, position, anchor, epoch] {
            const auto result = QAndroidJniObject::callStaticObjectMethod("org/baron/krita/PromptClipboard",
                "readText", "(Landroid/content/Context;)Ljava/lang/String;",
                QtAndroid::androidActivity().object<jobject>());
            QAndroidJniEnvironment environment;
            QString text;
            if (environment->ExceptionCheck()) environment->ExceptionClear();
            else if (result.isValid()) text = result.toString();
            QMetaObject::invokeMethod(qApp, [editor, text, revision, position, anchor, epoch] {
                if (!editor || editor->m_inputEpoch != epoch) return;
                editor->m_pastePending = false;
                BaronDiagnostics::record("paste.android-read.end", {{ "characters", text.size() }});
                if (editor->hasFocus() && editor->document()->revision() == revision
                    && editor->textCursor().position() == position
                    && editor->textCursor().anchor() == anchor) editor->insertClipboardText(text);
            }, Qt::QueuedConnection);
        });
#else
        m_pastePending = false;
        if (hasFocus() && document()->revision() == revision
            && textCursor().position() == position && textCursor().anchor() == anchor)
            insertClipboardText(QApplication::clipboard()->text());
#endif
    });
}
void PromptEditor::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_embeddedCompletion) {
        QPlainTextEdit::contextMenuEvent(event);
        return;
    }
    hideCompletion();
    m_completionTimer->stop();
    auto menu = new QMenu(this);
    auto add = [menu](const QString& text, const QString& name, bool enabled, const auto& slot) {
        auto action = menu->addAction(text);
        action->setObjectName(name);
        action->setEnabled(enabled);
        QObject::connect(action, &QAction::triggered, menu, slot);
    };
    const auto label = [](const char* text) { return QCoreApplication::translate("QWidgetTextControl", text); };
    add(label("&Undo"), "edit-undo", !isReadOnly() && document()->isUndoAvailable(), [this] { undo(); });
    add(label("&Redo"), "edit-redo", !isReadOnly() && document()->isRedoAvailable(), [this] { redo(); });
    menu->addSeparator();
    add(label("Cu&t"), "edit-cut", !isReadOnly() && textCursor().hasSelection(), [this] { copySelection(true); });
    add(label("&Copy"), "edit-copy", textCursor().hasSelection(), [this] { copySelection(false); });
    // The stock menu queries Qt's Android MIME clipboard synchronously before showing.
    add(label("&Paste"), "edit-paste", !isReadOnly(), [this, menu] {
        connect(menu, &QObject::destroyed, this, [this] {
            QTimer::singleShot(0, this, [this] {
                window()->activateWindow();
                setFocus(Qt::OtherFocusReason);
                queueClipboardPaste();
            });
        });
    });
    add(label("Delete"), "edit-delete", !isReadOnly() && textCursor().hasSelection(), [this] {
        auto cursor = textCursor(); cursor.removeSelectedText(); setTextCursor(cursor);
    });
    menu->addSeparator();
    add(label("Select All"), "select-all", !toPlainText().isEmpty(), [this] { selectAll(); });
    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popup(event->globalPos());
}
void PromptEditor::updatePalette() {
    if (!m_negative)
        return;
    auto colors = parentWidget() ? parentWidget()->palette() : QApplication::palette();
    auto base = colors.color(QPalette::Base);
    base.setRed(qMin(255, base.red() + 14));
    colors.setColor(QPalette::Base, base);
    setPalette(colors);
}
void PromptEditor::changeEvent(QEvent* event) {
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && m_negative && parentWidget()
        && palette().color(QPalette::Base) == parentWidget()->palette().color(QPalette::Base))
        updatePalette();
}
void PromptEditor::keyPressEvent(QKeyEvent* event) {
    if ((event->key() == Qt::Key_Escape || event->key() == Qt::Key_Back) && m_actions->isOpen()) {
        m_actions->close(); event->accept(); return;
    }
    if (m_embeddedCompletion && (event->matches(QKeySequence::Copy) || event->matches(QKeySequence::Cut))) {
        copySelection(event->matches(QKeySequence::Cut));
        event->accept();
        return;
    }
    if (m_embeddedCompletion && event->matches(QKeySequence::Paste)) {
        queueClipboardPaste();
        event->accept();
        return;
    }
    if (m_completionList && m_completionList->isVisible()) {
        if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Back) {
            hideCompletion();
            m_completionTimer->stop();
            event->accept();
            return;
        }
        if (event->modifiers() == Qt::NoModifier
            && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Tab)) {
            insertCompletion(m_completionList->currentIndex().data(Qt::EditRole).toString());
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            const int row = qBound(0, m_completionList->currentIndex().row()
                + (event->key() == Qt::Key_Down ? 1 : -1), m_completer->completionCount() - 1);
            m_completionList->setCurrentIndex(m_completer->completionModel()->index(row, 0));
            m_completionList->scrollTo(m_completionList->currentIndex());
            event->accept();
            return;
        }
    }
    if (!m_embeddedCompletion && m_completer->popup()->isVisible()
        && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter
            || event->key() == Qt::Key_Tab || event->key() == Qt::Key_Escape)) {
        event->ignore();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        && event->modifiers() & Qt::ControlModifier) {
        emit activated();
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
    m_completionTimer->start();
}
void PromptEditor::mousePressEvent(QMouseEvent* event) {
    if (!m_negative && event->button() == Qt::LeftButton
        && event->pos().x() > viewport()->width() - 14
        && event->pos().y() > viewport()->height() - 14) {
        m_resizing = true;
        m_dragY = event->globalY();
        m_startHeight = height();
        event->accept();
        return;
    }
    QPlainTextEdit::mousePressEvent(event);
}
void PromptEditor::paintEvent(QPaintEvent* event) {
    const bool trace = document()->characterCount() > 128 && m_loggedPaintRevision != document()->revision();
    if (trace) BaronDiagnostics::record("prompt-paint.begin", {{ "revision", document()->revision() }});
    QPlainTextEdit::paintEvent(event);
    if (trace) { m_loggedPaintRevision = document()->revision(); BaronDiagnostics::record("prompt-paint.end"); }
    if (!m_negative) {
        QPainter painter(viewport());
        painter.setPen(palette().color(QPalette::Mid));
        for (int i = 0; i < 3; ++i)
            painter.drawLine(viewport()->width() - 3 - i * 4, viewport()->height() - 3,
                viewport()->width() - 3, viewport()->height() - 3 - i * 4);
    }
}
void PromptEditor::mouseMoveEvent(QMouseEvent* event) {
    if (m_resizing) {
        setFixedHeight(qBound(fontMetrics().height() * 3,
            m_startHeight + event->globalY() - m_dragY, fontMetrics().height() * 40));
        emit heightChanged(height());
        event->accept();
        return;
    }
    QPlainTextEdit::mouseMoveEvent(event);
}
void PromptEditor::mouseReleaseEvent(QMouseEvent* event) {
    if (m_resizing) {
        m_resizing = false;
        event->accept();
        return;
    }
    QPlainTextEdit::mouseReleaseEvent(event);
}
