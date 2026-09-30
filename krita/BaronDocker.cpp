// SPDX-License-Identifier: GPL-3.0-or-later
#include "native/BaronPanel.h"
#include "native/Localization.h"
#include <KisViewManager.h>
#include <KoCanvasObserverBase.h>
#include <KoColorSpaceRegistry.h>
#include <KoDockFactoryBase.h>
#include <KoDockRegistry.h>
#include <QDockWidget>
#include <QPointer>
#include <QUuid>
#include <commands/kis_node_property_list_command.h>
#include <cstring>
#include <kis_canvas2.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_image_manager.h>
#include <kis_node_commands_adapter.h>
#include <kis_node_manager.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
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
        setWindowTitle(tr("AI Diffusion · Baron Edition"));
        setWidget(new BaronPanel(this, this));
    }
    QString observerName() override { return "BaronOrchestrionDocker"; }
    void setCanvas(KoCanvasBase* canvas) override { m_canvas = dynamic_cast<KisCanvas2*>(canvas); }
    void unsetCanvas() override { m_canvas.clear(); }
    CanvasSnapshot capture(bool includeMask, QString* error) override {
        if (!m_canvas || !m_canvas->image()) {
            *error = tr("Open a Krita document first");
            return {};
        }
        KisImageSP image = m_canvas->image();
        image->waitForDone();
        if (qint64(image->width()) * image->height() > 16777216) {
            *error = tr("This canvas exceeds 16 megapixels");
            return {};
        }
        CanvasSnapshot result;
        result.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        result.image = image->projection()->convertToQImage(
            KoColorSpaceRegistry::instance()->rgb8()->profile(), image->bounds());
        if (includeMask) {
            const auto selection = m_canvas->viewManager()->selection();
            if (selection && !selection->selectedExactRect().isEmpty()) {
                result.mask = QImage(image->size(), QImage::Format_Grayscale8);
                QByteArray bytes(image->width() * image->height(), 0);
                selection->projection()->readBytes(
                    reinterpret_cast<quint8*>(bytes.data()), 0, 0, image->width(), image->height());
                for (int y = 0; y < image->height(); ++y)
                    memcpy(result.mask.scanLine(y), bytes.constData() + y * image->width(),
                        image->width());
            }
        }
        m_target = m_canvas;
        m_targetImage = image;
        m_targetId = result.id;
        m_targetSize = image->size();
        m_originalNodes.clear();
        for (auto node = image->rootLayer()->firstChild(); node; node = node->nextSibling())
            if (node->visible() && qobject_cast<KisLayer*>(node.data()))
                m_originalNodes.append(node);
        return result;
    }
    bool apply(const QString& id, const QImage& result, const QImage& mask, const QString& name,
        QString* error) override {
        if (!m_target || !m_targetImage || m_target->image() != m_targetImage || id != m_targetId) {
            *error = tr("The original document is closed. Open it and generate again.");
            return false;
        }
        if (m_targetImage->size() != m_targetSize) {
            *error = tr("The canvas size changed. Generate again before applying this result.");
            return false;
        }
        KisImageSP image = m_targetImage;
        auto layer = new KisPaintLayer(image, name, 255);
        layer->paintDevice()->convertFromQImage(
            result, KoColorSpaceRegistry::instance()->rgb8()->profile());
        auto manager = m_target->viewManager();
        if (result.width() > image->width() || result.height() > image->height())
            manager->imageManager()->resizeCurrentImage(result.width(), result.height(), 0, 0);
        KisNodeCommandsAdapter commands(manager);
        commands.beginMacro(kundo2_i18n("Apply Orchestrion result"));
        commands.addNode(KisNodeSP(layer), image->rootLayer(), image->rootLayer()->lastChild());
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
                reinterpret_cast<const quint8*>(bytes.constData()), 0, 0, grayscale.width(),
                grayscale.height());
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
        m_targetSize = image->size();
        return true;
    }

private:
    QPointer<KisCanvas2> m_canvas, m_target;
    KisImageWSP m_targetImage;
    QString m_targetId;
    QSize m_targetSize;
    KisNodeList m_originalNodes;
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
    }
};
K_PLUGIN_FACTORY_WITH_JSON(BaronFactory, "krita_barondocker.json", registerPlugin<BaronPlugin>();)
#include "BaronDocker.moc"
