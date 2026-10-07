// SPDX-License-Identifier: GPL-3.0-or-later
#include "native/BaronPanel.h"
#include "native/Localization.h"
#include "native/PersonalFonts.h"
#include "native/OrientationLayouts.h"
#include "native/InpaintWidget.h"
#include <QSettings>
#include <kis_selection_mask.h>
#include <kis_selection_manager.h>
#include <kis_filter_mask.h>
#include <kis_transform_mask.h>
#include <cmath>
#include <algorithm>
#include <QMainWindow>
#include <QTimer>
#include <QEvent>
#include <QApplication>
#include <KisDocument.h>
#include <KisView.h>
#include <KisViewManager.h>
#include <KoCanvasObserverBase.h>
#include <KoColorSpaceRegistry.h>
#include <KoDockFactoryBase.h>
#include <KoDockRegistry.h>
#include <KoResourcePaths.h>
#include <QDockWidget>
#include <QJsonDocument>
#include <QPointer>
#include <QScopeGuard>
#include <QUuid>
#include <commands/kis_node_property_list_command.h>
#include <cstring>
#include <kis_annotation.h>
#include <kis_canvas2.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_image_manager.h>
#include <kis_filter_strategy.h>
#include <kis_node_commands_adapter.h>
#include <kis_node_manager.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <kis_transaction.h>
#include <KoCompositeOpRegistry.h>
#include <QJsonArray>
#include <kis_pixel_selection.h>
#include <kis_selection.h>
#include <kis_transparency_mask.h>
#include <kpluginfactory.h>
#include <kundo2command.h>

class BaronDocker : public QDockWidget, public KoCanvasObserverBase, public CanvasHost {
    Q_OBJECT
public:
    BaronDocker() {
        BaronLocalization::install();
        PersonalFonts::load(KoResourcePaths::getApplicationRoot() + "/share/baron-fonts");
        setWindowTitle(tr("AI Diffusion · Baron Edition"));
        setWidget(new BaronPanel(this, this));
#ifdef Q_OS_ANDROID
        QTimer::singleShot(0, this, [this] {
            OrientationLayouts::install(qobject_cast<QMainWindow*>(window()));
        });
#endif
    }
    QString observerName() override { return "BaronOrchestrionDocker"; }
    bool event(QEvent* event) override {
        const auto result = QDockWidget::event(event);
#ifdef Q_OS_ANDROID
        if (event->type() == QEvent::ParentChange || event->type() == QEvent::Show)
            QTimer::singleShot(0, this, [this] { OrientationLayouts::install(qobject_cast<QMainWindow*>(window())); });
#endif
        return result;
    }
    ~BaronDocker() override {
        if (auto panel = qobject_cast<BaronPanel*>(widget()))
            panel->flushDocumentState();
        clearPreview();
    }
    void setCanvas(KoCanvasBase* canvas) override {
        if (m_canvas == dynamic_cast<KisCanvas2*>(canvas))
            return;
        if (auto panel = qobject_cast<BaronPanel*>(widget()))
            panel->flushDocumentState();
        if (m_canvas != dynamic_cast<KisCanvas2*>(canvas))
            clearPreview();
        if(m_canvas&&m_canvas->viewManager())disconnect(m_canvas->viewManager()->selectionManager(),nullptr,this,nullptr);
        m_canvas = dynamic_cast<KisCanvas2*>(canvas);
        if(m_canvas&&m_canvas->viewManager())connect(m_canvas->viewManager()->selectionManager(),&KisSelectionManager::selectionChanged,this,[this] {
            if(auto panel=qobject_cast<BaronPanel*>(widget()))panel->canvasSelectionChanged();
        });
        if (auto panel = qobject_cast<BaronPanel*>(widget()))
            panel->documentChanged();
    }
    void unsetCanvas() override {
        if (auto panel = qobject_cast<BaronPanel*>(widget()))
            panel->flushDocumentState();
        clearPreview();
        m_canvas.clear();
        if (auto panel = qobject_cast<BaronPanel*>(widget()))
            panel->documentChanged();
    }
    QString documentId() const override {
        if (!canvasReady())
            return {};
        auto id = annotation("ai_diffusion/document_id");
        auto document = m_canvas->viewManager()->document();
        if (id.isEmpty() || (m_documents.value(QString::fromUtf8(id))
                && m_documents.value(QString::fromUtf8(id)) != document)) {
            id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8();
            const_cast<BaronDocker*>(this)->setAnnotation("ai_diffusion/document_id", id);
        }
        m_documents[QString::fromUtf8(id)] = document;
        return QString::fromUtf8(id);
    }
    QByteArray annotation(const QString& key) const override {
        if (!canvasReady())
            return {};
        const auto value = m_canvas->image()->annotation(key);
        return value ? value->annotation() : QByteArray();
    }
    void setAnnotation(const QString& key, const QByteArray& bytes) override {
        if (!canvasReady() || annotation(key) == bytes)
            return;
        if (bytes.isEmpty())
            m_canvas->image()->removeAnnotation(key);
        else
            m_canvas->image()->addAnnotation(new KisAnnotation(key, "AI Diffusion Plugin", bytes));
        if (auto document = m_canvas->viewManager()->document())
            document->setModified(true);
    }
    void setDocumentAnnotation(const QString& id, const QString& key, const QByteArray& bytes) override {
        auto document = m_documents.value(id);
        if (!document || !document->image())
            return;
        auto image = document->image();
        auto old = image->annotation(key);
        if ((old ? old->annotation() : QByteArray()) == bytes)
            return;
        if (bytes.isEmpty())
            image->removeAnnotation(key);
        else
            image->addAnnotation(new KisAnnotation(key, "AI Diffusion Plugin", bytes));
        document->setModified(true);
    }
    QString restoreTarget(const QRect& area, const QImage& mask) override {
        if (!canvasReady() || area.isEmpty())
            return {};
        KisImageSP image = m_canvas->image();
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QList<KisNodeWSP> nodes;
        for (auto node = image->rootLayer()->firstChild(); node; node = node->nextSibling())
            if (node->visible() && qobject_cast<KisLayer*>(node.data())
                && node.data() != m_previewLayer.data())
                nodes.append(KisNodeWSP(node));
        m_targets[id] = { m_canvas, image, image->size(), area, mask, nodes };
        return id;
    }
    QJsonObject documentState() const override {
        if (!canvasReady())
            return {};
        const auto state = m_canvas->image()->annotation("baron-native-state");
        if (!state || state->annotation().size() > 16 * 1024 * 1024)
            return {};
        const auto data = QJsonDocument::fromJson(state->annotation()).object();
        return data["schema"].toInt() == 1 ? data : QJsonObject();
    }
    void saveDocumentState(const QJsonObject& state) override {
        if (!canvasReady())
            return;
        const auto bytes = QJsonDocument(state).toJson(QJsonDocument::Compact);
        const auto old = m_canvas->image()->annotation("baron-native-state");
        if (old && old->annotation() == bytes)
            return;
        m_canvas->image()->addAnnotation(
            new KisAnnotation("baron-native-state", "Baron generation settings", bytes));
        if (auto document = m_canvas->viewManager()->document())
            document->setModified(true);
    }
    bool hasSelection() const override {
        if(!canvasReady())return false;
        const auto selection=m_canvas->viewManager()->selection();return selection&&!selection->selectedExactRect().isEmpty();
    }
    QRect imageBounds(bool selected) const override {
        if (!canvasReady())
            return {};
        auto bounds = m_canvas->image()->bounds();
        const auto selection = m_canvas->viewManager()->selection();
        if (selected && selection && !selection->selectedExactRect().isEmpty())
            bounds = selection->selectedExactRect().intersected(bounds);
        return bounds;
    }
    CanvasSnapshot capture(bool includeMask, QString* error) override { return captureInternal(includeMask, {}, error); }
    CanvasSnapshot captureWithContext(const QJsonObject& options, QString* error) override { return captureInternal(true, options, error); }
    CanvasSnapshot captureInternal(bool includeMask, const QJsonObject& options, QString* error) {
        if (!canvasReady()) {
            *error = tr("Open a Krita document first");
            return {};
        }
        KisImageSP image = m_canvas->image();
        auto preview = m_previewLayer;
        const bool hasPreview = preview && m_previewImage == image.data();
        const bool previewVisible = preview && preview->visible();
        const KisNodeList previewHidden = m_previewHidden;
        QList<bool> hiddenVisibility;
        for (auto node : previewHidden) hiddenVisibility.append(node->visible());
        if (hasPreview) {
            image->barrierLock();
            preview->setVisible(false);
            for (auto node : previewHidden) node->setVisible(true);
            image->unlock();
            image->refreshGraphAsync(image->rootLayer(), image->bounds());
        }
        const auto restorePreview = qScopeGuard([image, preview, previewVisible, previewHidden, hiddenVisibility, hasPreview]() mutable {
            if (!hasPreview || !preview->parent()) return;
            image->barrierLock();
            preview->setVisible(previewVisible);
            for (int i = 0; i < previewHidden.size(); ++i) {
                auto node = previewHidden[i];
                if (node->parent()) node->setVisible(hiddenVisibility[i]);
            }
            image->unlock();
            image->refreshGraphAsync(image->rootLayer(), image->bounds());
        });
        image->waitForDone();
        CanvasSnapshot result;
        result.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        result.bounds = image->bounds();
        const auto selection = m_canvas->viewManager()->selection();
        QRect outputBounds;
        if (includeMask && selection && !selection->selectedExactRect().isEmpty()) {
            const auto original=selection->selectedExactRect().intersected(image->bounds());
            bool allSelected=original==image->bounds();
            if(allSelected&&qint64(original.width())*original.height()<=16777216) {
                QByteArray samples(original.width()*original.height(),0);
                selection->projection()->readBytes(reinterpret_cast<quint8*>(samples.data()),original.x(),original.y(),original.width(),original.height());
                allSelected=std::all_of(samples.cbegin(),samples.cend(),[](char value){return quint8(value)==255;});
            }
            if (!allSelected) {
                outputBounds=original;
                if (!options.isEmpty()) {
                    const QSettings settings("BaronEdition","Orchestrion");
                    const auto arch=options["arch"].toString().toLower();
                    const int multiple=InpaintWidget::diffusionMultiple(arch);
                    const int pad=int(std::hypot(original.width(),original.height())*settings.value("selectionPadding",6).toInt()/100.0);
                    outputBounds=InpaintWidget::clamped(InpaintWidget::padded(original,pad,256,multiple),image->bounds());
                    QRect layerBounds;
                    if(options["context"]=="layer_bounds") {
                        const auto id=options["context_layer_id"].toString();
                        std::function<void(KisNodeSP)> find=[&](KisNodeSP parent){for(auto node=parent->firstChild();node;node=node->nextSibling()){if(node->uuid().toString()==id&&node->projection())layerBounds=node->projection()->exactBounds();find(node);}};
                        find(image->rootLayer());
                    }
                    result.bounds=InpaintWidget::contextBounds(image->bounds(),outputBounds,options,layerBounds);
                } else result.bounds=outputBounds;
                if(options["mode"]=="replace_background"&&options["strength"].toDouble(1)==1)result.bounds=outputBounds=image->bounds();
                result.resultBounds=outputBounds;
            }
        }
        if (result.bounds.isEmpty()
            || qint64(result.bounds.width()) * result.bounds.height() > 16777216) {
            *error = tr("This canvas exceeds 16 megapixels");
            return {};
        }
        result.image = image->projection()->convertToQImage(
            KoColorSpaceRegistry::instance()->rgb8()->profile(), result.bounds);
        if (includeMask) {
            if (!outputBounds.isEmpty()) {
                result.mask = QImage(result.bounds.size(), QImage::Format_Grayscale8);
                QByteArray bytes(result.mask.width() * result.mask.height(), 0);
                selection->projection()->readBytes(reinterpret_cast<quint8*>(bytes.data()),
                    result.bounds.x(), result.bounds.y(), result.mask.width(),
                    result.mask.height());
                for (int y = 0; y < result.mask.height(); ++y)
                    memcpy(result.mask.scanLine(y), bytes.constData() + y * result.mask.width(),result.mask.width());
                if(options["mode"]=="replace_background"&&options["strength"].toDouble(1)==1)
                    for(int y=0;y<result.mask.height();++y){auto row=result.mask.scanLine(y);for(int x=0;x<result.mask.width();++x)row[x]=255-row[x];}
            }
        }
        m_target = m_canvas;
        m_targetImage = image;
        m_targetId = result.id;
        m_targetSize = image->size();
        result.resultMask=result.mask.isNull()?QImage():result.mask.copy(outputBounds.translated(-result.bounds.topLeft()));
        m_targetBounds = outputBounds.isEmpty()?result.bounds:outputBounds;
        m_selectionMask = result.resultMask;
        m_originalNodes.clear();
        for (auto node = image->rootLayer()->firstChild(); node; node = node->nextSibling())
            if (node->visible() && qobject_cast<KisLayer*>(node.data()))
                m_originalNodes.append(node);
        QList<KisNodeWSP> nodes;
        for (auto node : m_originalNodes)
            nodes.append(KisNodeWSP(node));
        m_targets[result.id] = { m_canvas, image, m_targetSize, m_targetBounds, m_selectionMask, nodes };
        m_targetOrder.append(result.id);
        if (m_targetOrder.size() > 128)
            m_targets.remove(m_targetOrder.takeFirst());
        return result;
    }
    bool target(const QString& id, QString* error) {
        if (!m_targets.contains(id)) {
            *error = tr("The original document is closed. Open it and generate again.");
            return false;
        }
        const auto record = m_targets.value(id);
        m_target = record.canvas;
        m_targetImage = record.image;
        m_targetSize = record.size;
        m_targetBounds = record.bounds;
        m_selectionMask = record.mask;
        m_originalNodes.clear();
        for (auto weak : record.nodes) {
            KisNodeSP node = weak;
            if (node)
                m_originalNodes.append(node);
        }
        m_targetId = id;
        if (!m_target || !m_target->imageView() || !m_target->viewManager() || !m_targetImage
            || m_target->image() != m_targetImage || m_target != m_canvas) {
            *error = tr("Switch to the original document to preview or apply this result.");
            return false;
        }
        if (m_targetImage->size() != m_targetSize) {
            *error = tr("The canvas size changed. Generate again before applying this result.");
            return false;
        }
        return true;
    }
    bool scaleTarget(const QString& id, QSize size, QString* error) override {
        if (!size.isValid() || qint64(size.width()) * size.height() > 67108864) {
            *error = tr("Upscale exceeds 64 megapixels");
            return false;
        }
        clearPreview();
        if (!target(id, error)) return false;
        if (size != m_targetImage->size()) {
            m_targetImage->scaleImage(size, m_targetImage->xRes(), m_targetImage->yRes(),
                KisFilterStrategyRegistry::instance()->get("Bilinear"));
            m_targetImage->waitForDone();
        }
        m_targetSize = size;
        m_targetBounds = QRect(QPoint(), size);
        m_selectionMask = {};
        auto& record = m_targets[id];
        record.size = size;
        record.bounds = m_targetBounds;
        record.mask = {};
        return true;
    }
    QJsonArray layers() const override {
        QJsonArray result;
        if (!canvasReady())
            return result;
        const auto active = m_canvas->viewManager()->nodeManager()->activeNode();
        std::function<void(KisNodeSP, int)> visit = [&](KisNodeSP parent, int depth) {
            for (auto node = parent->lastChild(); node; node = node->prevSibling()) {
                if (node.data() == m_previewLayer.data())
                    continue;
                const bool mask=qobject_cast<KisSelectionMask*>(node.data())||qobject_cast<KisTransparencyMask*>(node.data())||qobject_cast<KisFilterMask*>(node.data())||qobject_cast<KisTransformMask*>(node.data());
                if (qobject_cast<KisLayer*>(node.data())||mask)
                    result.append(QJsonObject { { "id", node->uuid().toString() },
                        { "name", QString(QString(depth * 2, ' ') + node->name()) },
                        { "active", node == active }, {"mask",mask} });
                visit(node, depth + 1);
            }
        };
        visit(m_canvas->image()->rootLayer(), 0);
        return result;
    }
    QString appliedLayerId() const override { return m_appliedLayerId; }
    QImage layerImage(const QString& id, const QRect& crop, QString* error) override {
        if (!canvasReady()) {
            *error = tr("Open a Krita document first");
            return {};
        }
        KisImageSP image = m_canvas->image();
        auto preview = m_previewLayer;
        const bool hide = id == "visible" && preview && m_previewImage == image.data();
        const bool previewVisible = preview && preview->visible();
        const KisNodeList original = m_previewHidden;
        QList<bool> visibility;
        for (auto node : original) visibility.append(node->visible());
        if (hide) {
            image->barrierLock();
            preview->setVisible(false);
            for (auto node : original) if (node->parent()) node->setVisible(true);
            image->unlock();
            image->refreshGraphAsync(image->rootLayer(), image->bounds());
        }
        const auto restore = qScopeGuard([image, preview, previewVisible, original, visibility, hide]() mutable {
            if (!hide || !preview->parent()) return;
            image->barrierLock();
            preview->setVisible(previewVisible);
            for (int i = 0; i < original.size(); ++i) {
                auto node = original[i];
                if (node->parent()) node->setVisible(visibility[i]);
            }
            image->unlock();
            image->refreshGraphAsync(image->rootLayer(), image->bounds());
        });
        image->waitForDone();
        if (id == "visible") {
            const auto bounds
                = crop.isEmpty() ? image->bounds() : crop.intersected(image->bounds());
            if (bounds.isEmpty() || qint64(bounds.width()) * bounds.height() > 16777216) {
                *error = tr("The control layer is empty or too large.");
                return {};
            }
            return image->projection()->convertToQImage(
                KoColorSpaceRegistry::instance()->rgb8()->profile(), bounds);
        }
        KisNodeSP found;
        std::function<void(KisNodeSP)> find = [&](KisNodeSP parent) {
            for (auto node = parent->firstChild(); node; node = node->nextSibling()) {
                if (node->uuid().toString() == id && node.data() != m_previewLayer.data())
                    found = node;
                find(node);
            }
        };
        find(image->rootLayer());
        if (!found || !found->projection()) {
            *error = tr("The linked layer was removed. Choose another layer.");
            return {};
        }
        const auto bounds = crop.isEmpty()
            ? found->projection()->exactBounds().intersected(image->bounds())
            : crop;
        if (bounds.isEmpty() || qint64(bounds.width()) * bounds.height() > 16777216) {
            *error = tr("The control layer is empty or too large.");
            return {};
        }
        return found->projection()->convertToQImage(
            KoColorSpaceRegistry::instance()->rgb8()->profile(), bounds);
    }
    QImage masked(const QImage& result) const {
        if (m_selectionMask.isNull())
            return result;
        QImage image = result.convertToFormat(QImage::Format_ARGB32);
        const auto mask = m_selectionMask.scaled(result.size());
        for (int y = 0; y < image.height(); ++y) {
            auto line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x)
                line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]),
                    qAlpha(line[x]) * qGray(mask.pixel(x, y)) / 255);
        }
        return image;
    }
    void hidePreview() override {
        if (!m_previewLayer || !m_previewImage) return;
        KisImageSP image = m_previewImage;
        image->barrierLock();
        m_previewLayer->setVisible(false);
        for (auto node : m_previewHidden)
            if (node->parent()) node->setVisible(true);
        image->unlock();
        image->refreshGraphAsync(image->rootLayer(), image->bounds());
    }
    void clearPreview() override {
        if (m_previewLayer && m_previewImage) {
            KisImageSP image = m_previewImage;
            image->barrierLock();
            image->removeNode(m_previewLayer);
            for (auto node : m_previewHidden)
                if (node->parent())
                    node->setVisible(true);
            image->unlock();
            image->refreshGraphAsync(image->rootLayer(), image->bounds());
        }
        m_previewLayer.clear();
        m_previewImage = KisImageWSP();
        m_previewHidden.clear();
    }
    bool preview(
        const QString& id, const QImage& result, const QImage& mask, QString* error) override {
        if (!target(id, error))
            return false;
        KisImageSP image = m_targetImage;
        if (!m_previewLayer || m_previewImage != image.data() || !m_previewLayer->parent()) {
            clearPreview();
            m_previewLayer = new KisPaintLayer(image, tr("AI Preview — temporary"), 255);
        }
        auto pixels = masked(result);
        if (!mask.isNull()) {
            pixels = pixels.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < pixels.height(); ++y) {
                auto line = reinterpret_cast<QRgb*>(pixels.scanLine(y));
                for (int x = 0; x < pixels.width(); ++x)
                    line[x] = qRgba(
                        qRed(line[x]), qGreen(line[x]), qBlue(line[x]), qGray(mask.pixel(x, y)));
            }
        }
        image->barrierLock();
        for (auto node : m_previewHidden)
            if (node->parent()) node->setVisible(true);
        m_previewHidden = mask.isNull() ? KisNodeList() : m_originalNodes;
        for (auto node : m_previewHidden)
            if (node->parent()) node->setVisible(false);
        m_previewLayer->paintDevice()->clear();
        m_previewLayer->paintDevice()->setX(0);
        m_previewLayer->paintDevice()->setY(0);
        m_previewLayer->paintDevice()->convertFromQImage(
            pixels, KoColorSpaceRegistry::instance()->rgb8()->profile());
        m_previewLayer->paintDevice()->setX(m_targetBounds.x());
        m_previewLayer->paintDevice()->setY(m_targetBounds.y());
        m_previewLayer->setUserLocked(true);
        m_previewLayer->setVisible(true);
        if (!m_previewLayer->parent())
            image->addNode(m_previewLayer, image->rootLayer(), image->rootLayer()->lastChild());
        else if (image->rootLayer()->lastChild().data() != m_previewLayer.data())
            image->moveNode(m_previewLayer, image->rootLayer(), image->rootLayer()->lastChild());
        image->unlock();
        m_previewImage = image;
        image->refreshGraphAsync(image->rootLayer(), image->bounds());
        return true;
    }
    bool apply(const QString& id, const QImage& result, const QImage& mask, const QString& name,
        QString* error) override {
        return applyConfigured(id, result, mask, name, {}, error);
    }
    bool applyConfigured(const QString& id, const QImage& result, const QImage& mask,
        const QString& name, const QJsonObject& options, QString* error) override {
        clearPreview();
        if (!target(id, error))
            return false;
        if (!m_target || !m_targetImage || m_target->image() != m_targetImage || id != m_targetId) {
            *error = tr("The original document is closed. Open it and generate again.");
            return false;
        }
        if (m_targetImage->size() != m_targetSize) {
            *error = tr("The canvas size changed. Generate again before applying this result.");
            return false;
        }
        KisImageSP image = m_targetImage;
        auto manager = m_target->viewManager();
        const auto behavior = options["apply"].toString("layer");
        const auto regionBehavior = options["region_apply"].toString("none");
        const auto regions = options["regions"].toArray();
        const auto active = manager->nodeManager()->activeNode();
        if (regions.isEmpty() || regionBehavior == "none" || !mask.isNull()) {
            if (behavior == "replace" && (!qobject_cast<const KisPaintLayer*>(active.data()) || !editable(active))) {
                *error = tr("Choose an unlocked paint layer to replace its contents.");
                return false;
            }
        }
        if (m_targetBounds == image->bounds()
            && (result.width() > image->width() || result.height() > image->height()))
            manager->imageManager()->resizeCurrentImage(result.width(), result.height(), 0, 0);
        KisNodeCommandsAdapter commands(manager);
        if (!regions.isEmpty() && regionBehavior != "none" && mask.isNull()) {
            auto links = QJsonDocument::fromJson(annotation("baron-region-links")).object();
            struct Region { QJsonObject data; KisNodeSP node; QImage mask; QString key; };
            QList<Region> resolved;
            for (const auto& value : regions) {
                const auto data = value.toObject();
                const auto layerId = data["layer"].toString();
                const auto key = layerId == "selection" ? "region:" + data["id"].toString() : layerId;
                auto node = findNode(links[key].toString());
                if (!node) node = findNode(layerId);
                const auto regionMask = QImage::fromData(QByteArray::fromBase64(data["mask"].toString().toLatin1()));
                if (regionMask.isNull() || regionMask.size() != result.size() || (node && !editable(node))) {
                    *error = tr("A region mask is invalid or its layer is locked. No result was applied.");
                    return false;
                }
                resolved.append({ data, node, regionMask, key });
            }
            commands.beginMacro(kundo2_i18n("Apply Orchestrion regions"));
            for (const auto& region : resolved) {
                if (regionBehavior == "replace" && qobject_cast<const KisPaintLayer*>(region.node.data())) {
                    replacePaint(region.node, withAlpha(result, region.mask), commands);
                    m_appliedLayerId = region.node->uuid().toString();
                    continue;
                }
                KisNodeSP group = region.node;
                if (!qobject_cast<KisGroupLayer*>(group.data())) {
                    group = new KisGroupLayer(image, region.node ? region.node->name() : region.data["layer_title"].toString(name), 255);
                    const auto parent = region.node ? region.node->parent() : KisNodeSP(image->rootLayer());
                    commands.addNode(group, parent, region.node ? region.node : parent->lastChild());
                    if (region.node) commands.moveNode(region.node, group, KisNodeSP());
                }
                const bool separateMask = regionBehavior == "transparency_mask";
                auto layer = new KisPaintLayer(image, region.data["prompt"].toString(name), 255);
                layer->paintDevice()->convertFromQImage(masked(separateMask ? result : withAlpha(result, region.mask)),
                    KoColorSpaceRegistry::instance()->rgb8()->profile());
                layer->paintDevice()->setX(m_targetBounds.x());
                layer->paintDevice()->setY(m_targetBounds.y());
                if (regionBehavior != "no_hide" && !separateMask && m_selectionMask.isNull()) {
                    for (auto node = group->firstChild(); node; node = node->nextSibling()) {
                        if (!qobject_cast<KisLayer*>(node.data())) continue;
                        auto properties = node->sectionModelProperties();
                        properties[0].state = false;
                        commands.addExtraCommand(new KisNodePropertyListCommand(node, properties));
                    }
                }
                if (separateMask) {
                    bool hasMask = false;
                    for (auto node = group->firstChild(); node; node = node->nextSibling())
                        hasMask |= qobject_cast<KisTransparencyMask*>(node.data()) != nullptr;
                    if (!hasMask) {
                        KisTransparencyMaskSP transparency = new KisTransparencyMask(image, tr("Transparency Mask"));
                        transparency->initSelection(qobject_cast<KisLayer*>(group.data()));
                        writeSelection(transparency->selection(), region.mask);
                        commands.addNode(transparency, group, group->lastChild());
                    }
                }
                commands.addNode(KisNodeSP(layer), group, group->lastChild());
                links[region.key] = group->uuid().toString();
                m_appliedLayerId = layer->uuid().toString();
            }
            commands.endMacro();
            setAnnotation("baron-region-links", QJsonDocument(links).toJson(QJsonDocument::Compact));
            m_targetSize = image->size();
            m_targets[id].size = m_targetSize;
            return true;
        }
        commands.beginMacro(kundo2_i18n("Apply Orchestrion result"));
        if (behavior == "replace") {
            replacePaint(active, mask.isNull() ? result : withAlpha(result, mask), commands);
            commands.endMacro();
            m_appliedLayerId = active->uuid().toString();
            m_targetSize = image->size();
            m_targets[id].size = m_targetSize;
            return true;
        }
        auto layer = new KisPaintLayer(image, name, 255);
        layer->paintDevice()->convertFromQImage(masked(result), KoColorSpaceRegistry::instance()->rgb8()->profile());
        layer->paintDevice()->setX(m_targetBounds.x());
        layer->paintDevice()->setY(m_targetBounds.y());
        const auto parent = behavior == "layer_active" && active && active->parent() ? active->parent() : KisNodeSP(image->rootLayer());
        commands.addNode(KisNodeSP(layer), parent, behavior == "layer_active" && active && active->parent() ? active : parent->lastChild());
        if (!mask.isNull()) {
            KisTransparencyMaskSP transparency
                = new KisTransparencyMask(image, tr("Character mask — editable"));
            transparency->initSelection(KisLayerSP(layer));
            const auto grayscale = mask.convertToFormat(QImage::Format_Grayscale8);
            QByteArray bytes(grayscale.width() * grayscale.height(), 0);
            for (int y = 0; y < grayscale.height(); ++y)
                memcpy(bytes.data() + y * grayscale.width(), grayscale.constScanLine(y),
                    grayscale.width());
            transparency->selection()->pixelSelection()->writeBytes(
                reinterpret_cast<const quint8*>(bytes.constData()), m_targetBounds.x(),
                m_targetBounds.y(), grayscale.width(), grayscale.height());
            commands.addNode(transparency, KisNodeSP(layer), KisNodeSP());
            for (const auto& node : m_originalNodes) {
                if (!node->parent())
                    continue;
                auto properties = node->sectionModelProperties();
                properties[0].state = false;
                commands.addExtraCommand(new KisNodePropertyListCommand(node, properties));
            }
        }
        commands.endMacro();
        m_appliedLayerId = layer->uuid().toString();
        m_targetSize = image->size();
        m_targets[id].size = m_targetSize;
        return true;
    }

private:
    static bool editable(KisNodeSP node) {
        for (auto current = node; current; current = current->parent())
            if (current->userLocked()) return false;
        return true;
    }
    KisNodeSP findNode(const QString& id) const {
        if (id.isEmpty() || !m_targetImage) return {};
        QList<KisNodeSP> pending { m_targetImage->rootLayer() };
        while (!pending.isEmpty()) {
            const auto node = pending.takeLast();
            if (node->uuid().toString() == id) return node;
            for (auto child = node->firstChild(); child; child = child->nextSibling()) pending.append(child);
        }
        return {};
    }
    QImage withAlpha(const QImage& image, const QImage& mask) const {
        auto result = image.convertToFormat(QImage::Format_ARGB32);
        const auto alpha = mask.convertToFormat(QImage::Format_Grayscale8);
        for (int y = 0; y < result.height(); ++y) {
            auto line = reinterpret_cast<QRgb*>(result.scanLine(y));
            const auto gray = alpha.constScanLine(y);
            for (int x = 0; x < result.width(); ++x)
                line[x] = qRgba(qRed(line[x]), qGreen(line[x]), qBlue(line[x]), qAlpha(line[x]) * gray[x] / 255);
        }
        return result;
    }
    void writeSelection(KisSelectionSP selection, const QImage& mask) const {
        const auto gray = mask.convertToFormat(QImage::Format_Grayscale8);
        QByteArray bytes(gray.width() * gray.height(), 0);
        for (int y = 0; y < gray.height(); ++y) memcpy(bytes.data() + y * gray.width(), gray.constScanLine(y), gray.width());
        selection->pixelSelection()->writeBytes(reinterpret_cast<const quint8*>(bytes.constData()),
            m_targetBounds.x(), m_targetBounds.y(), gray.width(), gray.height());
    }
    void replacePaint(KisNodeSP node, const QImage& result, KisNodeCommandsAdapter& commands) {
        const auto destination = node->paintDevice();
        KisPaintDeviceSP source = new KisPaintDevice(destination->colorSpace());
        source->convertFromQImage(result, KoColorSpaceRegistry::instance()->rgb8()->profile());
        KisTransaction transaction(kundo2_i18n("Apply Orchestrion result"), destination);
        KisPainter painter(destination);
        painter.setCompositeOpId(COMPOSITE_COPY);
        if (!m_selectionMask.isNull()) {
            KisSelectionSP selection = new KisSelection;
            writeSelection(selection, m_selectionMask);
            painter.setSelection(selection);
        }
        painter.bitBlt(m_targetBounds.x(), m_targetBounds.y(), source, 0, 0, result.width(), result.height());
        painter.end();
        commands.addExtraCommand(transaction.endAndTake());
        node->setDirty();
    }
    bool canvasReady() const {
        return m_canvas && m_canvas->imageView() && m_canvas->viewManager() && m_canvas->image();
    }
    QPointer<KisCanvas2> m_canvas, m_target;
    mutable QMap<QString, QPointer<KisDocument>> m_documents;
    KisImageWSP m_targetImage;
    QString m_targetId;
    QString m_appliedLayerId;
    QSize m_targetSize;
    KisNodeList m_originalNodes;
    QRect m_targetBounds;
    QImage m_selectionMask;
    struct Target {
        QPointer<KisCanvas2> canvas;
        KisImageWSP image;
        QSize size;
        QRect bounds;
        QImage mask;
        QList<KisNodeWSP> nodes;
    };
    QMap<QString, Target> m_targets;
    QStringList m_targetOrder;
    KisPaintLayerSP m_previewLayer;
    KisImageWSP m_previewImage;
    KisNodeList m_previewHidden;
};

class BaronDockerFactory : public KoDockFactoryBase {
public:
    QString id() const override { return "BaronOrchestrionDocker"; }
    Qt::DockWidgetArea defaultDockWidgetArea() const { return Qt::RightDockWidgetArea; }
    DockPosition defaultDockPosition() const override { return DockMinimized; }
    QDockWidget* createDockWidget() override {
        auto dock = new BaronDocker();
        dock->setObjectName(id());
        return dock;
    }
};
class BaronPlugin : public QObject {
    Q_OBJECT
public:
    BaronPlugin(QObject* parent, const QVariantList&)
        : QObject(parent) {
        KoDockRegistry::instance()->add(new BaronDockerFactory());
#ifdef Q_OS_ANDROID
        qApp->installEventFilter(this);
        QTimer::singleShot(0, this, [] {
            for (auto widget : QApplication::topLevelWidgets())
                OrientationLayouts::install(qobject_cast<QMainWindow*>(widget));
        });
#endif
    }
#ifdef Q_OS_ANDROID
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::Show) {
            const QPointer<QMainWindow> window(qobject_cast<QMainWindow*>(object));
            if (window) QTimer::singleShot(0, this, [window] { if (window) OrientationLayouts::install(window); });
        }
        return QObject::eventFilter(object, event);
    }
#endif
};
K_PLUGIN_FACTORY_WITH_JSON(BaronFactory, "krita_barondocker.json", registerPlugin<BaronPlugin>();)
#include "BaronDocker.moc"
