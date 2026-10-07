// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QAbstractButton>
#include <QComboBox>
#include <QJsonArray>
#include <QSettings>
#include <QVBoxLayout>

class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)
public:
    explicit ToggleSwitch(QWidget* parent = nullptr, const QString& on = {}, const QString& off = {});
    QSize sizeHint() const override;
    qreal position() const { return m_position; }
    void setPosition(qreal value) { m_position = value; update(); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    qreal m_position = 0;
    QString m_on, m_off;
};

class InterfaceSettings : public QWidget {
    Q_OBJECT
public:
    explicit InterfaceSettings(QWidget* parent = nullptr);
    void reset();
    void setTranslationLanguages(const QJsonArray& languages);
Q_SIGNALS:
    void changed(const QString& key, const QVariant& value);
private:
    void build();
    void save(const QString& key, const QVariant& value);
    QWidget* row(const QString& title, const QString& description, QWidget* control);
    QComboBox* combo(const QString& key, const QString& title, const QString& description,
        const QList<QPair<QString, QString>>& choices, const QString& defaultValue);
    ToggleSwitch* toggle(const QString& key, const QString& title, const QString& description,
        bool defaultValue, bool showHide = false);
    void spin(const QString& key, const QString& title, const QString& description,
        int low, int high, int defaultValue);
    void rebuildTags();
    QVBoxLayout* m_rows;
    QWidget* m_content;
    QWidget* m_tags;
    QComboBox *m_translation, *m_format;
    ToggleSwitch* m_metadata;
    QStringList m_keys;
};
