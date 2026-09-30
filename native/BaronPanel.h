// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "OrchestrionClient.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

struct CanvasSnapshot {
    QImage image, mask;
    QString id;
};
class CanvasHost {
public:
    virtual ~CanvasHost() = default;
    virtual CanvasSnapshot capture(bool includeMask, QString* error) = 0;
    virtual bool apply(const QString& id, const QImage& image, const QImage& mask,
        const QString& name, QString* error)
        = 0;
};

class BaronPanel : public QWidget {
    Q_OBJECT
public:
    explicit BaronPanel(CanvasHost* host, QWidget* parent = nullptr);
    static QByteArray png(const QImage& image);

private:
    QJsonObject input(bool pixels);
    void prepare(bool run);
    void showModels(const QJsonObject& data);
    void addReferences();
    void showImage(const QByteArray& bytes, int index);
    void saveSettings();
    void updateMode();
    void openCatalog(const QString& kind);
    void filterCatalog();
    void loadThumbnails();
    CanvasHost* m_host;
    OrchestrionClient* m_client;
    QLineEdit *m_url, *m_filter, *m_seed;
    QPlainTextEdit *m_prompt, *m_negative;
    QComboBox *m_mode, *m_model;
    QSpinBox *m_width, *m_height, *m_steps, *m_batch;
    QDoubleSpinBox *m_strength, *m_cfg, *m_scale, *m_fidelity;
    QComboBox *m_referencePurpose, *m_referenceDetail;
    QComboBox* m_catalogKind;
    QLabel *m_account, *m_price, *m_status, *m_preview;
    QListWidget *m_gallery, *m_refs, *m_loras, *m_results;
    QPushButton *m_run, *m_apply, *m_quote, *m_resume;
    QProgressBar* m_progress;
    QJsonArray m_catalog;
    QList<QImage> m_referenceImages;
    QImage m_result, m_foreground, m_resultMask, m_sourceImage;
    QString m_targetId, m_jobMode, m_pendingJob;
    bool m_submitAfterPrepare = false;
    bool m_busy = false;
    int m_expectedImages = 0;
    QMap<int, QImage> m_jobImages;
    QWidget* m_catalogView;
    QVBoxLayout* m_catalogContainer;
    QNetworkAccessManager* m_thumbnails;
    QSet<QString> m_loadedThumbnails;
    int m_thumbnailRequests = 0;
};
