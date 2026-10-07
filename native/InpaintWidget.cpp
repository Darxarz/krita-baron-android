// SPDX-License-Identifier: GPL-3.0-or-later
#include "InpaintWidget.h"
#include "PluginUi.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QtMath>
InpaintWidget::InpaintWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("customInpaint");
    auto layout = new QHBoxLayout(this); layout->setContentsMargins(0, 0, 0, 0); layout->setSpacing(6);
    m_seamless = new QCheckBox(tr("Seamless"), this); m_seamless->setObjectName("inpaintSeamless"); m_seamless->setChecked(true);
    m_seamless->setToolTip(tr("Generate content which blends into the surroundings"));
    m_focus = new QCheckBox(tr("Focus"), this); m_focus->setObjectName("inpaintFocus");
    m_focus->setToolTip(tr("Use the text prompt to describe the selected region rather than the context area / Use only one regional prompt"));
    m_edit = new QCheckBox(tr("Edit"), this); m_edit->setObjectName("inpaintEdit");
    m_edit->setToolTip(tr("Edit canvas with text instructions"));
    m_fill = new QComboBox(this); m_fill->setObjectName("inpaintFill");
    for (const auto& entry : QList<QPair<QString,QString>> {{"none",tr("None")},{"neutral",tr("Neutral")},{"blur",tr("Blur")},{"border",tr("Border")},{"inpaint",tr("Inpaint")}})
        m_fill->addItem(PluginUi::icon(entry.first == "none" ? "fill-empty" : "fill", this), entry.second, entry.first);
    m_fill->setCurrentIndex(1); m_fill->setToolTip(tr("Pre-fill the selected region before diffusion"));
    m_context = new QComboBox(this); m_context->setObjectName("inpaintContext");
    m_context->addItem(PluginUi::icon("context-automatic",this),tr("Automatic Context"),"automatic");
    m_context->addItem(PluginUi::icon("context-mask",this),tr("Selection Bounds"),"mask_bounds");
    m_context->addItem(PluginUi::icon("context-image",this),tr("Entire Image"),"entire_image");
    m_context->setMinimumContentsLength(20);
    m_context->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    const QString flatStyle="QComboBox { border: none; background-color: transparent; padding: 1px 12px 1px 2px; }";
    m_fill->setStyleSheet(flatStyle);m_context->setStyleSheet(flatStyle);
    m_context->setToolTip(tr("Part of the image around the selection which is used as context."));
    for (auto check : {m_seamless,m_focus,m_edit}) { layout->addWidget(check); connect(check,&QCheckBox::toggled,this,&InpaintWidget::changed); }
    layout->addWidget(m_fill,1); layout->addWidget(m_context,1);
    connect(m_edit,&QCheckBox::toggled,this,&InpaintWidget::editChanged);
    for (auto combo : {m_fill,m_context}) connect(combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&InpaintWidget::changed);
}
QJsonObject InpaintWidget::state() const {
    const auto context=m_context->currentData().toString();
    return {{"use_inpaint_model",m_seamless->isChecked()},{"use_condition_mask",m_focus->isChecked()},
        {"fill",m_fill->currentData().toString()},{"context",context.startsWith("layer:") ? "layer_bounds" : context},
        {"context_layer_id",context.startsWith("layer:") ? context.mid(6) : QString()}, {"edit",m_edit->isChecked()}};
}
void InpaintWidget::restore(const QJsonObject& state) {
    QSignalBlocker a(m_seamless),b(m_focus),c(m_edit),d(m_fill),e(m_context);
    m_seamless->setChecked(state["use_inpaint_model"].toBool(true)); m_focus->setChecked(state["use_condition_mask"].toBool());
    m_edit->setChecked(state["edit"].toBool());
    m_fill->setCurrentIndex(qMax(0,m_fill->findData(state["fill"].toString("neutral"))));
    auto context=state["context"].toString("automatic");
    if(context=="layer_bounds")context="layer:"+state["context_layer_id"].toString();
    m_context->setProperty("savedContext",context);
    m_context->setCurrentIndex(qMax(0,m_context->findData(context)));
}
void InpaintWidget::setLayers(const QJsonArray& layers) {
    auto context=m_context->property("savedContext").toString();
    if(context.isEmpty())context=m_context->currentData().toString();
    QSignalBlocker guard(m_context);
    while(m_context->count()>3)m_context->removeItem(m_context->count()-1);
    for(auto value:layers){auto layer=value.toObject();if(layer["mask"].toBool())m_context->addItem(PluginUi::icon("context-layer",this),layer["name"].toString(),QString("layer:"+layer["id"].toString()));}
    m_context->setCurrentIndex(qMax(0,m_context->findData(context)));m_context->setProperty("savedContext",QVariant());
}
void InpaintWidget::setCapabilities(const QString& arch,double strength,bool editing) {
    const auto family=arch.toLower();
    const bool xl=QStringList{"sdxl","illu","illu_v"}.contains(family)||family.contains("illustrious")||family.contains("pony")||family.contains("noob")||family.contains("sdxl");
    m_seamless->setEnabled(xl||QStringList{"sd15","flux","zimage","qwen","anima"}.contains(arch));
    m_focus->setVisible(arch=="sd15"||xl);m_fill->setEnabled(strength==1&&!editing);
    const bool canEdit=QStringList{"flux_k","flux2_4b","flux2_9b","qwen_e","qwen_e_p","qwen2","krea2"}.contains(family)||family.contains("qwen")||family.contains("krea")||family.contains("flux2");
    m_edit->setEnabled(canEdit);QSignalBlocker guard(m_edit);m_edit->setChecked(editing);
}
int InpaintWidget::diffusionMultiple(const QString& arch) {
    return arch=="qwen2"||arch=="qwen21"?32:16;
}
QRect InpaintWidget::padded(QRect area,int padding,int minSize,int multiple,bool square) {
    int px=padding,py=padding;
    if(square&&area.width()>area.height())px=qMax(px/2,px-(area.width()-area.height())/2);
    else if(square&&area.height()>area.width())py=qMax(py/2,py-(area.height()-area.width())/2);
    const int w=((qMax(area.width()+2*px,minSize)+multiple-1)/multiple)*multiple;
    const int h=((qMax(area.height()+2*py,minSize)+multiple-1)/multiple)*multiple;
    return {area.x()-(w-area.width())/2,area.y()-(h-area.height())/2,w,h};
}
QRect InpaintWidget::clamped(QRect area,QRect canvas) {
    int w=qMin(area.width(),canvas.width()),h=qMin(area.height(),canvas.height());
    return {qBound(canvas.x(),area.x(),canvas.x()+canvas.width()-w),qBound(canvas.y(),area.y(),canvas.y()+canvas.height()-h),w,h};
}
QRect InpaintWidget::contextBounds(QRect canvas,QRect mask,const QJsonObject& options,QRect layer) {
    if(mask.isEmpty())return canvas;
    if(options["mode"]=="custom") {
        const auto context=options["context"].toString();
        if(context=="mask_bounds")return mask;
        if(context=="entire_image")return canvas;
        if(context=="layer_bounds"&&!layer.isEmpty())return clamped(layer.united(mask),canvas);
    }
    if(options["strength"].toDouble(1)<1||options["edit"].toBool())return mask;
    const int padding=qMax(qMax(canvas.width(),canvas.height())/16,(mask.width()+mask.height())/4);
    return clamped(padded(mask,padding,512,8,true),canvas);
}
