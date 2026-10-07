// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "JobQueue.h"
#include "HistoryStore.h"
#include "ModelThumbnails.h"
#include "OrchestrionClient.h"
#include "PluginUi.h"
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
#include <QRect>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

struct CanvasSnapshot {
    QImage image, mask;
    QString id;
    QRect bounds;
    QRect resultBounds;
    QImage resultMask;
};
class CanvasHost {
public:
    virtual ~CanvasHost() = default;
    virtual CanvasSnapshot capture(bool includeMask, QString* error) = 0;
    virtual CanvasSnapshot captureWithContext(const QJsonObject&, QString* error) { return capture(true, error); }
    virtual bool apply(const QString& id, const QImage& image, const QImage& mask,
        const QString& name, QString* error)
        = 0;
    virtual bool applyConfigured(const QString& id, const QImage& image, const QImage& mask,
        const QString& name, const QJsonObject&, QString* error) {
        return apply(id, image, mask, name, error);
    }
    virtual bool preview(const QString&, const QImage&, const QImage&, QString*) { return false; }
    virtual void clearPreview() { }
    virtual void hidePreview() { clearPreview(); }
    virtual QJsonArray layers() const { return {}; }
    virtual QImage layerImage(const QString&, const QRect&, QString*) { return {}; }
    virtual QString appliedLayerId() const { return {}; }
    virtual QString documentId() const { return {}; }
    virtual QRect imageBounds(bool) const { return {}; }
    virtual bool hasSelection() const { return imageBounds(true)!=imageBounds(false); }
    virtual QJsonObject documentState() const { return {}; }
    virtual void saveDocumentState(const QJsonObject&) { }
    virtual QByteArray annotation(const QString&) const { return {}; }
    virtual void setAnnotation(const QString&, const QByteArray&) { }
    virtual void setDocumentAnnotation(const QString& document, const QString& key, const QByteArray& bytes) {
        if (document == documentId())
            setAnnotation(key, bytes);
    }
    virtual QString restoreTarget(const QRect&, const QImage&) { return {}; }
    virtual bool scaleTarget(const QString&, QSize, QString*) { return false; }
};

class InpaintWidget;
class GuidancePanel;
class UpscaleWidget;
class ModelCatalog;
class QCheckBox;
class QDialog;
class QStackedWidget;
class QToolButton;

class BaronPanel : public QWidget {
    Q_OBJECT
public:
    explicit BaronPanel(CanvasHost* host, QWidget* parent = nullptr,
        PromptEditor::CompletionDisplay promptDisplay = PromptEditor::CompletionDisplay::Automatic);
    ~BaronPanel() override;
    static QByteArray png(const QImage& image);
    void documentChanged();
    void canvasSelectionChanged();
    void flushDocumentState();

protected:
    void changeEvent(QEvent* event) override;

private:
    QJsonObject input(bool pixels);
    void prepare(bool run);
    void submitBatch(
        const QJsonObject& data, QJsonObject context, const QImage& source, bool front);
    void showModels(const QJsonObject& data);
    void addReferences();
    void showImage(const QByteArray& bytes, int index);
    void finishImages(const HistoryStore::Prepared& result);
    void receiveResult(const QJsonObject& context, const HistoryStore::Prepared& result);
    void saveSettings();
    void updateMode();
    InpaintWidget* m_inpaint = nullptr;
    QString m_inpaintMode = "automatic";
    void capturePromptBank();
    void restorePromptBank();
    void captureStyleBank();
    void restoreStyleBank(const QString& previous);
    QJsonObject m_styleBanks;
    QString m_editSourceStyle;
    bool m_styleLoading = false;
    void updatePromptSyntax();
    QJsonObject m_promptBanks;
    QString m_promptBankMode = "generate";
    QComboBox* m_promptSyntax;
    QCheckBox* m_gpuNoise;
    QLineEdit* m_ensd;
    void openCatalog(const QString& kind);
    void filterCatalog();
    void loadThumbnails();
    void selectResult(QListWidgetItem* item, bool apply = false);
    void prepareControl(const QJsonObject& input);
    void historyAction(const QString& action);
    void updateHistoryItem(QListWidgetItem* item);
    void restoreGenerationSettings(const QJsonObject& data);
    QJsonObject generationSettings() const;
    void openSettings(int page = 0);
    void rebuildStyles();
    void selectStyle();
    void updateStyleFields();
    void saveStyle();
    void refreshPrice();
    void cancelJobs(int kind);
    void updateJobs();
    void restoreDocumentState();
    void restoreHistory(bool appendOnly = false);
    void persistHistory();
    QSharedPointer<HistoryStore> history(const QString& document);
    QMap<QString, QSharedPointer<HistoryStore>> m_histories;
    QString m_historyDocument, m_resultDocument;
    QRect m_captureBounds, m_resultBounds;
    QImage m_captureMask, m_resultSelection;
    JobQueue* m_jobs;
    QListWidget* m_jobList;
    QLabel* m_jobCount;
    QTimer* m_saveTimer = nullptr;
    bool m_restoring = false;
    bool m_uiReady = false;
    QString m_documentModel, m_resultControlId;
    QJsonObject m_resultMetadata;
    QDialog* m_settingsDialog;
    QStackedWidget* m_pages;
    QToolButton *m_workspace, *m_queueButton, *m_modeButton;
    QComboBox* m_styles;
    QList<QJsonObject> m_stylePresets;
    QString m_styleId;
    QTimer* m_priceTimer;
    OrchestrionClient* m_priceClient;
    bool m_priceBusy = false;
    QByteArray m_priceKey;
    QByteArray m_historyKey;
    CanvasHost* m_host;
    OrchestrionClient* m_client;
    QComboBox* m_connectionMode;
    QLineEdit *m_url, *m_filter, *m_seed;
    PromptEditor *m_prompt, *m_negative;
    QComboBox *m_mode, *m_model;
    QSpinBox *m_width, *m_height, *m_steps, *m_batch;
    QDoubleSpinBox *m_strength, *m_cfg, *m_scale, *m_fidelity;
    QDoubleSpinBox* m_upscaleFactor;
    UpscaleWidget* m_upscale;
    GuidancePanel* m_guidance;
    QComboBox *m_sampler, *m_scheduler, *m_queuePosition;
    QCheckBox* m_fixedSeed;
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
    QString m_previewResult;
    bool m_submitAfterPrepare = false;
    bool m_busy = false;
    bool m_replacingQueue = false;
    int m_expectedImages = 0;
    QMap<int, QImage> m_jobImages;
    QWidget* m_catalogView;
    ModelCatalog* m_catalogBrowser;
    QVBoxLayout* m_catalogContainer;
    ModelThumbnails* m_thumbnails;
    QSet<QString> m_loadedThumbnails;
    int m_thumbnailRequests = 0;
    int m_catalogGeneration = 0;
};
