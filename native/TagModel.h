// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QAbstractListModel>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>

class TagModel : public QAbstractListModel {
public:
    struct Tag { QString text, source; int category; qint64 count; };
    explicit TagModel(QObject* parent = nullptr) : QAbstractListModel(parent) { }
    static TagModel* shared();
    void reload(const QStringList& datasets, bool force = false);
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QJsonObject lookup(QString key) const;
    QJsonArray typoCandidates(QString key) const;
    QStringList similar(QString key) const;
private:
    QStringList m_datasets;
    QVector<Tag> m_tags;
    QHash<QString, int> m_lookup;
    QHash<QString, QVector<int>> m_prefixes;
    mutable QHash<QString, QStringList> m_similar;
};
