// SPDX-License-Identifier: GPL-3.0-or-later
#include "TagModel.h"
#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

TagModel* TagModel::shared() {
    static QPointer<TagModel> instance;
    if (!instance) instance = new TagModel(qApp);
    return instance;
}
int TagModel::rowCount(const QModelIndex& parent) const { return parent.isValid()?0:m_tags.size(); }
QVariant TagModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()||index.row()<0||index.row()>=m_tags.size())return {};
    const auto& tag=m_tags[index.row()];
    if(role==Qt::DisplayRole||role==Qt::EditRole)return tag.text;
    if(role==Qt::ToolTipRole||role==Qt::UserRole)
        return QString(tag.source+" · "+QString::number(tag.count));
    if (role == Qt::UserRole + 1) {
        const auto count = tag.count > 1000000 ? QString::number(tag.count / 1000000.0, 'f', 0) + "m"
            : tag.count > 1000 ? QString::number(tag.count / 1000.0, 'f', 0) + "k"
            : QString::number(tag.count);
        return QString(tag.source + " " + count);
    }
    if(role==Qt::BackgroundRole) {
        const QColor tint=tag.category==1?QColor(Qt::red):tag.category==3?QColor(Qt::yellow):
            tag.category==4?QColor(Qt::green):tag.category==5?QColor(Qt::cyan):QColor(Qt::blue);
        const auto base=qApp->palette().color(QPalette::Base);
        return QColor(qRound(base.red()*.8+tint.red()*.2),qRound(base.green()*.8+tint.green()*.2),qRound(base.blue()*.8+tint.blue()*.2));
    }
    return {};
}
void TagModel::reload(const QStringList& datasets, bool force) {
    if(datasets==m_datasets && !force)return;
    QVector<Tag> tags;
    for(auto source:datasets) {
        if (source.contains('/') || source.contains('\\') || source == "." || source == "..") continue;
        QFile file(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/tags/" + source + ".csv");
        if (!file.exists()) {
            const auto directory = QFileInfo(file).dir();
            for (const auto& name : directory.entryList(QDir::Files)) {
                if (QFileInfo(name).completeBaseName() == source
                    && QFileInfo(name).suffix().compare("csv", Qt::CaseInsensitive) == 0) {
                    file.setFileName(directory.filePath(name));
                    break;
                }
            }
        }
        if (!file.exists()) file.setFileName(":/baron/tags/"+source+".csv");
        if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) continue;
        QTextStream stream(&file);stream.setCodec("UTF-8");
        while(!stream.atEnd()) {
            const auto line=stream.readLine();QStringList cells;QString cell;bool quoted=false;
            for(int i=0;i<line.size();++i) {
                const auto c=line[i];
                if(c=='"') {
                    if(quoted&&i+1<line.size()&&line[i+1]=='"'){cell+='"';++i;}
                    else quoted=!quoted;
                } else if(c==','&&!quoted) {
                    cells.append(cell);cell.clear();if(cells.size()==3)break;
                } else cell+=c;
            }
            if(cells.size()<3)cells.append(cell);
            if(cells.size()<3)continue;
            bool typeOk=false,countOk=false;const int type=cells[1].toInt(&typeOk);const auto count=cells[2].toLongLong(&countOk);
            if(typeOk&&countOk)tags.append({cells[0].replace('_',' '),source,type,count});
        }
    }
    std::stable_sort(tags.begin(),tags.end(),[](const Tag& a,const Tag& b){return a.count>b.count;});
    QSet<QString> seen;QVector<Tag> unique;unique.reserve(tags.size());
    for(auto& tag:tags)if(!seen.contains(tag.text)){seen.insert(tag.text);unique.append(std::move(tag));}
    beginResetModel();m_datasets=datasets;m_tags=std::move(unique);
    m_lookup.clear();
    m_prefixes.clear(); m_similar.clear();
    for (int i = 0; i < m_tags.size(); ++i) {
        m_lookup[m_tags[i].text.toLower().replace(' ', '_')] = i;
        m_prefixes[m_tags[i].text.left(2).toLower()].append(i);
    }
    endResetModel();
}

QJsonObject TagModel::lookup(QString key) const {
    key = key.toLower().replace(' ', '_');
    const int i = m_lookup.value(key, -1);
    if (i < 0) return {};
    const auto& tag = m_tags[i];
    const QString category = tag.category == 1 ? "artist" : tag.category == 3 ? "copyright"
        : tag.category == 4 ? "character" : tag.category == 5 && tag.source.startsWith("e621") ? "species"
        : tag.category == 5 ? "meta" : "general";
    return {{"tag", tag.text}, {"category", category}, {"count", double(tag.count)}, {"source", tag.source}};
}
QJsonArray TagModel::typoCandidates(QString key) const {
    QJsonArray result;
    key = key.toLower().replace('_', ' ');
    for (int i : m_prefixes.value(key.left(2))) {
        const auto& tag = m_tags[i];
        result.append(QJsonObject{{"tag", tag.text.toLower()}, {"count", double(tag.count)}});
    }
    return result;
}
QStringList TagModel::similar(QString key) const {
    key = key.toLower().replace('_', ' ');
    if (m_similar.contains(key)) return m_similar.value(key);
    QString head;
    for (const auto& word : key.split(' ')) if (word.size() >= 3) head = word;
    QStringList result;
    if (head.isEmpty()) return result;
    const QRegularExpression match("(^| )" + QRegularExpression::escape(head));
    for (const auto& tag : m_tags) {
        const auto text = tag.text.toLower();
        if (text != key && text.size() <= 40 && match.match(text).hasMatch()) result.append(text);
        if (result.size() == 8) break;
    }
    if (m_similar.size() >= 100) m_similar.clear();
    m_similar[key] = result;
    return result;
}
