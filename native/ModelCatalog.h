// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QJsonArray>
#include <QWidget>
#include <QJsonObject>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class ModelCatalog : public QWidget {
    Q_OBJECT
public:
    explicit ModelCatalog(QWidget* parent = nullptr);
    QListWidget* gallery() const { return m_gallery; }
    QLineEdit* search() const { return m_search; }
    QComboBox* kind() const { return m_kind; }
    void setModels(const QJsonArray& models);
    void applyFilters();
    void mergeMetadata(const QJsonArray& models);
    using MetadataCallback = std::function<void(const QJsonObject&, const QString&)>;
    using MetadataProvider = std::function<void(const QString&, const QString&, MetadataCallback, bool)>;
    void setMetadataProvider(MetadataProvider provider) { m_metadata = std::move(provider); }
    void showInformation(const QJsonObject& model);
    static QJsonObject normalize(const QJsonObject& model);
    static QString folder(const QString& name);
Q_SIGNALS:
    void filtersChanged();
    void modelChosen(const QJsonObject& model);
    void triggerWordsRequested(const QStringList& words);
private:
    void updateDetails(QListWidgetItem* item);
    void toggleFavorite(QListWidgetItem* item);
    void filterFolders();
    void rebuildTags();
    QString key(QListWidgetItem* item) const;
    QListWidget* m_gallery;
    QLineEdit *m_search, *m_folderSearch;
    QComboBox *m_kind, *m_family, *m_sort;
    QTreeWidget* m_folders;
    QListWidget* m_tags;
    QLineEdit* m_tagSearch;
    QCheckBox* m_favoritesOnly;
    QLabel *m_count, *m_details, *m_triggers;
    QToolButton* m_star;
    QStringList m_favorites;
    MetadataProvider m_metadata;
};
