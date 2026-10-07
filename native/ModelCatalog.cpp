// SPDX-License-Identifier: GPL-3.0-or-later
#include "ModelCatalog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QSettings>
#include <QSet>
#include <QSignalBlocker>
#include <QSharedPointer>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QTextDocument>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>

namespace {
QRect informationRect(const QRect& rect) { return QRect(rect.right()-47, rect.bottom()-47, 44, 44); }
QStringList strings(const QJsonValue& value) {
    QStringList result;
    if (value.isString()) result = value.toString().split(',', Qt::SkipEmptyParts);
    else for (auto entry : value.toArray()) if (entry.isString()) result.append(entry.toString());
    for (auto& word : result) word = word.trimmed();
    result.removeAll(QString()); result.removeDuplicates(); return result;
}
class CatalogGallery : public QListWidget {
public:
    using QListWidget::QListWidget;
    std::function<void(QListWidgetItem*)> information;
protected:
    void mousePressEvent(QMouseEvent* event) override {
        auto item = itemAt(event->pos());
        m_infoPress = event->button() == Qt::LeftButton && item && informationRect(visualItemRect(item)).contains(event->pos());
        if (m_infoPress) { event->accept(); return; }
        QListWidget::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (m_infoPress) {
            m_infoPress = false;
            auto item = itemAt(event->pos());
            if (item && informationRect(visualItemRect(item)).contains(event->pos()) && information) {
                setCurrentItem(item); information(item);
            }
            event->accept(); return;
        }
        QListWidget::mouseReleaseEvent(event);
    }
    bool m_infoPress = false;
};
class CardDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {210, 260}; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const bool selected = option.state & QStyle::State_Selected;
        const auto card = option.rect.adjusted(3, 3, -3, -3);
        const auto data = index.data(Qt::UserRole).toJsonObject();
        auto border = option.palette.text().color(); border.setAlpha(65);
        painter->setPen(selected ? option.palette.highlight().color() : border);
        painter->setBrush(selected ? option.palette.highlight() : option.palette.base());
        painter->drawRoundedRect(card, 8, 8);
        const QRect artwork(card.left()+10, card.top()+10, card.width()-20, 150);
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        if (!icon.isNull()) icon.paint(painter, artwork, Qt::AlignCenter);
        else {
            painter->fillRect(artwork, option.palette.alternateBase());
            painter->setPen(option.palette.mid().color());
            painter->drawText(artwork, Qt::AlignCenter, data["family"].toString());
        }
        painter->setPen(selected ? option.palette.highlightedText().color() : option.palette.text().color());
        auto font = option.font; font.setBold(true); painter->setFont(font);
        painter->drawText(QRect(card.left()+10, card.top()+166, card.width()-20, 40),
            Qt::TextWordWrap, data["title"].toString(data["name"].toString()));
        font.setBold(false); painter->setFont(font);
        painter->drawText(QRect(card.left()+10, card.top()+210, card.width()-20, 18),
            painter->fontMetrics().elidedText(data["family"].toString() +
                (data["kind"] == "lora" ? " · LoRA" : ""), Qt::ElideRight, card.width()-20));
        painter->setPen(option.palette.text().color());
        const QString path = ModelCatalog::folder(data["name"].toString());
        painter->drawText(QRect(card.left()+10, card.top()+234, card.width()-58, 16),
            painter->fontMetrics().elidedText(path, Qt::ElideMiddle, card.width()-58));
        const auto info = informationRect(option.rect).adjusted(8, 8, -8, -8);
        painter->setBrush(selected ? option.palette.highlightedText() : option.palette.button());
        painter->setPen(Qt::NoPen); painter->drawEllipse(info);
        painter->setPen(selected ? option.palette.highlight().color() : option.palette.text().color());
        font.setBold(true); painter->setFont(font); painter->drawText(info, Qt::AlignCenter, "i");
        if (index.data(Qt::UserRole+1).toBool())
            painter->drawPixmap(QRect(artwork.right()-20, artwork.top()+4, 16, 16),
                QPixmap(":/baron/icons/star.png"));
        painter->restore();
    }
};
class CardItem : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;
    bool operator<(const QListWidgetItem& other) const override {
        return data(Qt::UserRole+2).toString().compare(other.data(Qt::UserRole+2).toString(), Qt::CaseInsensitive)<0;
    }
};
}
ModelCatalog::ModelCatalog(QWidget* parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
    auto searchRow = new QHBoxLayout;
    m_search = new QLineEdit(this); m_search->setObjectName("modelSearchField");
    m_search->setPlaceholderText(tr("Search models, folders, tags and trigger words"));
    m_search->setClearButtonEnabled(true); m_search->setMinimumHeight(40);
    searchRow->addWidget(m_search,1);
    auto sidebar = new QToolButton(this); sidebar->setText(tr("Folders and tags")); sidebar->setCheckable(true); sidebar->setChecked(true);
    sidebar->setMinimumHeight(40); searchRow->addWidget(sidebar);
    auto reset = new QToolButton(this); reset->setText(tr("Reset filters")); searchRow->addWidget(reset);
    layout->addLayout(searchRow);
    auto filters = new QHBoxLayout;
    m_kind = new QComboBox(this); m_kind->setObjectName("modelKindFilter");
    m_kind->addItem(tr("All models"),"all"); m_kind->addItem(tr("Checkpoints"),"checkpoint"); m_kind->addItem(tr("LoRAs"),"lora");
    m_family = new QComboBox(this); m_family->setObjectName("modelFamilyFilter"); m_family->addItem(tr("All architectures"),"");
    m_sort = new QComboBox(this); m_sort->setObjectName("modelSort");
    m_sort->addItems({tr("Name A–Z"),tr("Name Z–A"),tr("Folder")});
    m_favoritesOnly = new QCheckBox(tr("Favorites"),this); m_favoritesOnly->setObjectName("modelFavoritesOnly");
    for(auto combo : {m_kind,m_family,m_sort})combo->setMinimumHeight(40);
    filters->addWidget(m_kind); filters->addWidget(m_family,1); filters->addWidget(m_sort); filters->addWidget(m_favoritesOnly);
    layout->addLayout(filters);
    auto splitter = new QSplitter(this); layout->addWidget(splitter,1);
    auto folders = new QWidget(splitter); auto folderLayout = new QVBoxLayout(folders); folderLayout->setContentsMargins(0,0,4,0);
    m_folderSearch = new QLineEdit(folders); m_folderSearch->setObjectName("modelFolderSearch");
    m_folderSearch->setPlaceholderText(tr("Search folders")); m_folderSearch->setClearButtonEnabled(true);
    m_folderSearch->setMinimumHeight(36);
    folderLayout->addWidget(m_folderSearch);
    m_folders = new QTreeWidget(folders); m_folders->setObjectName("modelFolders"); m_folders->setHeaderHidden(true);
    m_folders->setMinimumWidth(110); folderLayout->addWidget(m_folders,1);
    m_tagSearch = new QLineEdit(folders); m_tagSearch->setObjectName("modelTagSearch");
    m_tagSearch->setPlaceholderText(tr("Search tags")); m_tagSearch->setClearButtonEnabled(true); folderLayout->addWidget(m_tagSearch);
    m_tagSearch->setMinimumHeight(36);
    m_tags = new QListWidget(folders); m_tags->setObjectName("modelTags");
    m_tags->setSelectionMode(QAbstractItemView::ExtendedSelection); m_tags->setMaximumHeight(240);
    folderLayout->addWidget(m_tags,1);
    auto gallery = new CatalogGallery(splitter); m_gallery = gallery; m_gallery->setObjectName("modelGallery");
    gallery->information = [this](QListWidgetItem* item) { showInformation(item->data(Qt::UserRole).toJsonObject()); };
    m_gallery->setViewMode(QListView::IconMode); m_gallery->setResizeMode(QListView::Adjust);
    m_gallery->setMovement(QListView::Static); m_gallery->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_gallery->setDragEnabled(false); m_gallery->setAcceptDrops(false); m_gallery->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_gallery->setIconSize(QSize(160,160)); m_gallery->setGridSize(QSize(210,260));
    m_gallery->setWordWrap(true); m_gallery->setSpacing(8); m_gallery->setItemDelegate(new CardDelegate(m_gallery));
    m_gallery->setContextMenuPolicy(Qt::CustomContextMenu);
    splitter->setSizes({220,900}); splitter->setStretchFactor(1,1);
    QScroller::grabGesture(m_gallery->viewport(),QScroller::TouchGesture);
    QScroller::grabGesture(m_folders->viewport(),QScroller::TouchGesture);
    QScroller::grabGesture(m_tags->viewport(),QScroller::TouchGesture);
    connect(sidebar, &QToolButton::toggled, folders, &QWidget::setVisible);
    auto footer = new QHBoxLayout;
    m_count = new QLabel(this); m_count->setObjectName("modelResultCount"); footer->addWidget(m_count);
    m_details = new QLabel(this); m_details->setWordWrap(true); m_details->setTextFormat(Qt::PlainText);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse); footer->addWidget(m_details,1);
    m_star = new QToolButton(this); m_star->setIcon(QIcon(":/baron/icons/star.png"));
    m_star->setCheckable(true); m_star->setEnabled(false);
    m_star->setToolTip(tr("Add to favorites")); m_star->setObjectName("modelFavoriteButton");
    footer->addWidget(m_star); layout->addLayout(footer);
    auto information = new QToolButton(this); information->setObjectName("modelInformationButton");
    information->setText(tr("Model information")); information->setMinimumHeight(40); footer->addWidget(information);
    connect(information, &QToolButton::clicked, this, [this] {
        if (auto item = m_gallery->currentItem()) showInformation(item->data(Qt::UserRole).toJsonObject());
    });
    m_triggers = new QLabel(this); m_triggers->setTextFormat(Qt::PlainText); m_triggers->setWordWrap(true); layout->addWidget(m_triggers);
    m_favorites = QSettings("BaronEdition","Orchestrion").value("modelFavorites").toStringList();
    for (auto combo : {m_kind,m_family,m_sort})
        connect(combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{applyFilters();});
    connect(m_search,&QLineEdit::textChanged,this,[this]{applyFilters();});
    connect(m_favoritesOnly,&QCheckBox::toggled,this,[this]{applyFilters();});
    connect(m_folders,&QTreeWidget::currentItemChanged,this,[this]{applyFilters();});
    connect(m_folderSearch,&QLineEdit::textChanged,this,[this]{filterFolders();});
    connect(m_tags, &QListWidget::itemChanged, this, [this] { applyFilters(); });
    connect(m_tagSearch, &QLineEdit::textChanged, this, [this](const QString& query) {
        for (int i=0;i<m_tags->count();++i) m_tags->item(i)->setHidden(!m_tags->item(i)->text().contains(query,Qt::CaseInsensitive));
    });
    connect(m_gallery,&QListWidget::currentItemChanged,this,[this](QListWidgetItem* item){updateDetails(item);});
    connect(m_star,&QToolButton::clicked,this,[this]{toggleFavorite(m_gallery->currentItem());});
    connect(m_gallery,&QListWidget::customContextMenuRequested,this,[this](const QPoint& pos){
        auto item=m_gallery->itemAt(pos); if(!item)return;
        const auto model = item->data(Qt::UserRole).toJsonObject();
        QMenu menu(this); auto info = menu.addAction(tr("Model information"));
        auto favorite=menu.addAction(m_favorites.contains(key(item))?tr("Remove from favorites"):tr("Add to favorites"));
        const auto action = menu.exec(m_gallery->viewport()->mapToGlobal(pos));
        if(action==favorite)toggleFavorite(item); else if(action==info)showInformation(model);
    });
    connect(reset,&QToolButton::clicked,this,[this]{
        m_search->clear(); m_folderSearch->clear(); m_kind->setCurrentIndex(0); m_family->setCurrentIndex(0);
        m_favoritesOnly->setChecked(false); m_sort->setCurrentIndex(0); m_folders->setCurrentItem(m_folders->topLevelItem(0));
        m_tagSearch->clear();
        { QSignalBlocker blocker(m_tags); for(int i=0;i<m_tags->count();++i)m_tags->item(i)->setCheckState(Qt::Unchecked); }
        applyFilters();
    });
}
QString ModelCatalog::folder(const QString& name) {
    const auto path=QString(name).replace('\\','/');
    return path.contains('/')?path.left(path.lastIndexOf('/')):QString();
}
QString ModelCatalog::key(QListWidgetItem* item) const {
    const auto model=item->data(Qt::UserRole).toJsonObject();
    return QString(model["kind"].toString()+"|"+model["name"].toString());
}
void ModelCatalog::setModels(const QJsonArray& models) {
    const auto family=m_family->currentData().toString();
    const auto selectedFolder=m_folders->currentItem()?m_folders->currentItem()->data(0,Qt::UserRole).toString():QString();
    QSignalBlocker familyBlock(m_family),folderBlock(m_folders);
    m_gallery->clear(); m_folders->clear(); m_family->clear(); m_family->addItem(tr("All architectures"),"");
    auto all=new QTreeWidgetItem(m_folders,{tr("All folders")}); all->setData(0,Qt::UserRole,QString());
    auto root=new QTreeWidgetItem(all,{tr("No folder")}); root->setData(0,Qt::UserRole,"__root__");
    all->setSizeHint(0,QSize(100,36)); root->setSizeHint(0,QSize(100,36));
    QMap<QString,QTreeWidgetItem*> paths; paths[""]=all; paths["__root__"]=root;
    QSet<QString> families;
    for(auto value:models) {
        const auto model=normalize(value.toObject()); auto item=new CardItem(model["title"].toString(model["name"].toString()),m_gallery);
        item->setData(Qt::UserRole,model); item->setData(Qt::UserRole+1,m_favorites.contains(key(item)));
        item->setToolTip(model["name"].toString());
        const auto family=model["family"].toString(); if(!family.isEmpty())families.insert(family);
        const auto path=folder(model["name"].toString()); QString prefix;
        for(auto part:path.split('/',Qt::SkipEmptyParts)) {
            const QString next=prefix.isEmpty()?part:QString(prefix+"/"+part);
            if(!paths.contains(next)) { auto node=new QTreeWidgetItem(paths[prefix],{part}); node->setData(0,Qt::UserRole,next); node->setSizeHint(0,QSize(100,36)); paths[next]=node; }
            prefix=next;
        }
    }
    auto sorted=families.values(); sorted.sort(Qt::CaseInsensitive);
    for(auto name:sorted)m_family->addItem(name,name);
    m_family->setCurrentIndex(qMax(0,m_family->findData(family)));
    all->setExpanded(true); m_folders->sortItems(0,Qt::AscendingOrder);
    m_folders->setCurrentItem(paths.value(selectedFolder,all)); rebuildTags(); filterFolders(); applyFilters();
}
void ModelCatalog::applyFilters() {
    const auto selected=m_folders->currentItem(); const auto path=selected?selected->data(0,Qt::UserRole).toString():QString();
    const auto kind=m_kind->currentData().toString(),family=m_family->currentData().toString();
    const auto words=m_search->text().replace('\\','/').split(' ',Qt::SkipEmptyParts); int visible=0;
    QStringList tags;
    for(int i=0;i<m_tags->count();++i)if(m_tags->item(i)->checkState()==Qt::Checked)tags.append(m_tags->item(i)->data(Qt::UserRole).toString());
    for(int i=0;i<m_gallery->count();++i) {
        auto item=m_gallery->item(i);const auto model=item->data(Qt::UserRole).toJsonObject();
        const auto directory=folder(model["name"].toString());
        bool match=kind=="all"||(kind=="lora"?model["kind"]=="lora":model["kind"]!="lora");
        match &= family.isEmpty()||model["family"].toString()==family;
        match &= path.isEmpty()||(path=="__root__"?directory.isEmpty():directory==path||directory.startsWith(path+"/"));
        match &= !m_favoritesOnly->isChecked()||m_favorites.contains(key(item));
        QString haystack=model["title"].toString()+" "+QString(model["name"].toString()).replace('\\','/')+" "+model["family"].toString();
        for(auto trigger:model["triggerWords"].toArray())haystack+=QString(" "+trigger.toString());
        const auto modelTags = strings(model.value("tags"));
        haystack += QString(" " + modelTags.join(" "));
        for(const auto& tag : tags)match &= modelTags.contains(tag,Qt::CaseInsensitive);
        for(auto word:words)match &= haystack.contains(word,Qt::CaseInsensitive);
        item->setHidden(!match);visible+=match;
        const QString title=model["title"].toString(model["name"].toString());
        item->setData(Qt::UserRole+2,m_sort->currentIndex()==2?QString(directory+"/"+title):title);
    }
    m_gallery->sortItems(m_sort->currentIndex()==1?Qt::DescendingOrder:Qt::AscendingOrder);
    m_count->setText(tr("%1 / %2 models").arg(visible).arg(m_gallery->count()));
    emit filtersChanged();
}
void ModelCatalog::filterFolders() {
    const auto query=m_folderSearch->text().trimmed();
    std::function<bool(QTreeWidgetItem*)> visit=[&](QTreeWidgetItem* item){
        bool match=query.isEmpty()||item->data(0,Qt::UserRole).toString().contains(query,Qt::CaseInsensitive);
        for(int i=0;i<item->childCount();++i)match=visit(item->child(i))||match;
        item->setHidden(!match); if(!query.isEmpty()&&match)item->setExpanded(true); return match;
    };
    for(int i=0;i<m_folders->topLevelItemCount();++i)visit(m_folders->topLevelItem(i));
}
void ModelCatalog::updateDetails(QListWidgetItem* item) {
    m_star->setEnabled(item!=nullptr); m_details->clear(); m_triggers->clear();
    if(!item){m_star->setChecked(false);return;}
    const auto model=item->data(Qt::UserRole).toJsonObject();
    m_details->setText(model["name"].toString());
    const bool favorite=m_favorites.contains(key(item));
    m_star->setChecked(favorite);
    m_star->setToolTip(favorite?tr("Remove from favorites"):tr("Add to favorites"));
    QStringList words;for(auto word:model["triggerWords"].toArray())words.append(word.toString());
    if(!words.isEmpty())m_triggers->setText(tr("Trigger words: %1").arg(words.join(", ")));
}
void ModelCatalog::toggleFavorite(QListWidgetItem* item) {
    if(!item)return;const auto name=key(item);
    if(m_favorites.contains(name))m_favorites.removeAll(name);else m_favorites.append(name);
    QSettings("BaronEdition","Orchestrion").setValue("modelFavorites",m_favorites);
    item->setData(Qt::UserRole+1,m_favorites.contains(name));updateDetails(item);applyFilters();
}

QJsonObject ModelCatalog::normalize(const QJsonObject& source) {
    auto model = source;
    QStringList triggers;
    for(const auto& field : {"triggerWords", "triggers", "trainedWords"}) triggers.append(strings(source.value(field)));
    triggers.removeDuplicates(); model["triggerWords"] = QJsonArray::fromStringList(triggers);
    model["tags"] = QJsonArray::fromStringList(strings(source.value("tags")));
    if(model.value("title").toString().isEmpty())
        model["title"] = source.value("displayName").toString(source.value("modelName").toString(source.value("name").toString()));
    if(model.value("preview").toString().isEmpty()) {
        for(const auto& field : {"imageUrl", "thumbnail", "thumbnailUrl"}) {
            const QUrl url(source.value(field).toString());
            if(url.scheme()=="https" && (url.host()=="image.civitai.com" || url.host()=="blobs-b2.civitai.com")) { model["preview"] = url.toString(); break; }
        }
        if(model.value("preview").toString().isEmpty())
            for(const auto& image : source.value("images").toArray()) {
                const QUrl url(image.toObject().value("originalUrl").toString());
                if(url.scheme()=="https" && (url.host()=="image.civitai.com" || url.host()=="blobs-b2.civitai.com")) { model["preview"] = url.toString(); break; }
            }
    }
    return model;
}

void ModelCatalog::mergeMetadata(const QJsonArray& models) {
    QHash<QString,QJsonObject> entries;
    for(const auto& value : models) {
        const auto model = value.toObject();
        const QString path = QString(model.value("name").toString()).replace('\\','/');
        if(!path.isEmpty()) entries[path] = model;
    }
    // The site's bulk LoRA endpoint can describe another server. Only enrich exact
    // paths already present in this connection; never add or guess model names.
    for(int i=0;i<m_gallery->count();++i) {
        auto item = m_gallery->item(i); auto model = item->data(Qt::UserRole).toJsonObject();
        if(model.value("kind") != "lora") continue;
        const auto found = entries.constFind(QString(model.value("name").toString()).replace('\\','/'));
        if(found==entries.cend()) continue;
        for(auto it=found->begin();it!=found->end();++it)
            if(it.key()!="name" && it.key()!="kind" && it.key()!="family" && it.key()!="preview" && it.key()!="title")model[it.key()]=it.value();
        model = normalize(model);
        item->setData(Qt::UserRole,model);
    }
    rebuildTags(); updateDetails(m_gallery->currentItem()); applyFilters();
}

void ModelCatalog::rebuildTags() {
    QSet<QString> selected;
    for(int i=0;i<m_tags->count();++i)if(m_tags->item(i)->checkState()==Qt::Checked)selected.insert(m_tags->item(i)->data(Qt::UserRole).toString());
    QSignalBlocker blocker(m_tags); m_tags->clear();
    QMap<QString,int> counts;
    for(int i=0;i<m_gallery->count();++i)
        for(const auto& tag : strings(m_gallery->item(i)->data(Qt::UserRole).toJsonObject().value("tags")))counts[tag]++;
    for(auto it=counts.cbegin();it!=counts.cend();++it) {
        auto item = new QListWidgetItem(QString("%1 (%2)").arg(it.key()).arg(it.value()),m_tags);
        item->setData(Qt::UserRole,it.key()); item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
        item->setCheckState(selected.contains(it.key()) ? Qt::Checked : Qt::Unchecked);
        item->setSizeHint(QSize(100,36));
        item->setHidden(!item->text().contains(m_tagSearch->text(),Qt::CaseInsensitive));
    }
}

void ModelCatalog::showInformation(const QJsonObject& source) {
    QDialog dialog(this); dialog.setObjectName("modelInformationDialog");
    dialog.setWindowTitle(tr("Model information")); dialog.resize(760,780);
    auto layout = new QVBoxLayout(&dialog);
    auto title = new QLabel(&dialog); title->setWordWrap(true); title->setTextFormat(Qt::PlainText);
    auto font = title->font(); font.setPointSize(font.pointSize()+3); font.setBold(true); title->setFont(font); layout->addWidget(title);
    auto scroll = new QScrollArea(&dialog); scroll->setWidgetResizable(true); layout->addWidget(scroll,1);
    auto contents = new QWidget(scroll); auto details = new QVBoxLayout(contents);
    auto identity = new QLabel(contents); identity->setObjectName("modelIdentity");
    auto triggers = new QLabel(contents); triggers->setObjectName("modelTriggerWords");
    auto author = new QLabel(contents); author->setObjectName("modelAuthorNotes");
    auto settings = new QLabel(contents); settings->setObjectName("modelRecommendedSettings");
    auto examples = new QLabel(contents); examples->setObjectName("modelExampleSettings");
    for(auto label : {identity,triggers,author,settings,examples}) {
        label->setTextFormat(Qt::PlainText); label->setWordWrap(true); label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        details->addWidget(label);
    }
    details->addStretch(); scroll->setWidget(contents); QScroller::grabGesture(scroll->viewport(),QScroller::TouchGesture);
    auto status = new QLabel(&dialog); status->setTextFormat(Qt::PlainText); status->setWordWrap(true); layout->addWidget(status);
    auto actions = new QHBoxLayout;
    auto insert = new QPushButton(tr("Insert trigger words"),&dialog); insert->setObjectName("insertModelTriggers");
    auto link = new QPushButton(tr("Open on Civitai"),&dialog);
    auto refresh = new QPushButton(tr("Refresh information"),&dialog); refresh->setObjectName("refreshModelInformation");
    for(auto button : {insert,link,refresh}) { button->setMinimumHeight(44); actions->addWidget(button); }
    layout->addLayout(actions);
    auto buttons = new QDialogButtonBox(&dialog);
    auto use = buttons->addButton(tr("Use model"),QDialogButtonBox::AcceptRole); use->setObjectName("useCatalogModel");
    buttons->addButton(tr("Done"),QDialogButtonBox::RejectRole); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    auto current = QSharedPointer<QJsonObject>::create(normalize(source));
    auto render = [current,title,identity,triggers,author,settings,examples,insert,link] {
        const auto model = *current;
        title->setText(model.value("title").toString());
        const auto creator = model.value("creator").isObject() ? model.value("creator").toObject().value("username").toString() : model.value("creator").toString();
        identity->setText(QStringList{model.value("name").toString(),model.value("baseModel").toString(model.value("family").toString()),
            model.value("versionName").toString(),creator,strings(model.value("tags")).join(" · ")}.join("\n"));
        const auto words = strings(model.value("triggerWords"));
        triggers->setText(words.isEmpty() ? tr("No trigger words provided") : tr("Trigger words: %1").arg(words.join(", "))); insert->setEnabled(!words.isEmpty());
        QStringList notes;
        for(const auto& field : {"modelDescription","versionDescription","description"}) {
            QTextDocument text; text.setHtml(model.value(field).toString());
            const auto note = text.toPlainText().trimmed(); if(!note.isEmpty() && !notes.contains(note))notes.append(note);
        }
        author->setText(tr("Author notes") + "\n" + (notes.isEmpty() ? tr("No author recommendations provided") : notes.join("\n\n")));
        const auto recommended = model.value("recommendedSettings");
        settings->setVisible(!recommended.isUndefined() && !recommended.isNull());
        QStringList recommendations;
        const auto parameters = recommended.toObject();
        for(auto it=parameters.begin();it!=parameters.end();++it) {
            const auto label = it.key()=="steps" ? tr("Steps") : it.key()=="strength" ? tr("LoRA strength") : it.key()=="cfgScale" || it.key()=="cfg" ? tr("CFG") : it.key();
            const auto value = it.value().isString() ? it.value().toString() : it.value().isDouble() ? QString::number(it.value().toDouble()) : it.value().isBool() ? it.value().toBool() ? tr("Enabled") : tr("Disabled") : QString();
            if(!value.isEmpty())recommendations.append(QString("%1: %2").arg(label,value));
        }
        settings->setText(tr("Recommended settings") + "\n" + (recommended.isObject() ? recommendations.join("\n") : recommended.toString()));
        QStringList sampleSettings;
        for(const auto& value : model.value("images").toArray()) {
            const auto meta = value.toObject().value("meta").toObject(); QStringList values;
            for(const auto& field : {"sampler","steps","cfgScale","clipSkip","seed"}) {
                const auto value = meta.value(field);
                if(value.isString())values.append(QString("%1: %2").arg(field,value.toString()));
                else if(value.isDouble())values.append(QString("%1: %2").arg(field).arg(value.toDouble()));
            }
            const auto line = values.join(" · "); if(!line.isEmpty() && !sampleSettings.contains(line))sampleSettings.append(line);
            if(sampleSettings.size()>=3)break;
        }
        examples->setVisible(!sampleSettings.isEmpty());
        examples->setText(tr("Example settings (not author recommendations)") + "\n" + sampleSettings.join("\n"));
        const QUrl url(model.value("modelUrl").toString());
        link->setEnabled(url.scheme()=="https" && url.host()=="civitai.com" && url.userInfo().isEmpty());
    };
    render();
    connect(use,&QPushButton::clicked,&dialog,[this,&dialog,current] { dialog.accept(); emit modelChosen(*current); });
    connect(insert,&QPushButton::clicked,&dialog,[this,&dialog,current] {
        const auto words = strings(current->value("triggerWords")); dialog.accept(); emit triggerWordsRequested(words);
    });
    connect(link,&QPushButton::clicked,&dialog,[current] { QDesktopServices::openUrl(QUrl(current->value("modelUrl").toString())); });
    const QPointer<QDialog> guard(&dialog);
    auto load = [this,guard,current,status,refresh,render,source](bool force) {
        if(!m_metadata) { status->setText(tr("Model information is unavailable for this connection")); refresh->setEnabled(false); return; }
        refresh->setEnabled(false); status->setText(tr("Loading model information…"));
        m_metadata(source.value("name").toString(),source.value("kind").toString(),
            [this,guard,current,status,refresh,render,source](const QJsonObject& data,const QString& error) {
                if(!guard)return;
                refresh->setEnabled(true); status->setText(error);
                if(!data.isEmpty()) {
                    auto model = *current;
                    for(auto it=data.begin();it!=data.end();++it)if(it.key()!="name" && it.key()!="kind" && it.key()!="title" && it.key()!="preview")model[it.key()]=it.value();
                    *current = normalize(model); render();
                    for(int i=0;i<m_gallery->count();++i) {
                        auto item = m_gallery->item(i); const auto entry = item->data(Qt::UserRole).toJsonObject();
                        if(entry.value("name")==source.value("name") && entry.value("kind")==source.value("kind"))item->setData(Qt::UserRole,*current);
                    }
                    rebuildTags(); updateDetails(m_gallery->currentItem()); applyFilters();
                }
            },force);
    };
    connect(refresh,&QPushButton::clicked,&dialog,[load] { load(true); });
    load(false);
#ifdef Q_OS_ANDROID
    dialog.setWindowState(Qt::WindowFullScreen);
#endif
    dialog.exec();
}
