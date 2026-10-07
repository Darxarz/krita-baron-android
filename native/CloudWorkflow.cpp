// SPDX-License-Identifier: GPL-3.0-or-later
// Native translation of Acly's WorkflowInput serialization and resolution preparation.
#include "CloudWorkflow.h"
#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QtMath>
#include <cmath>

namespace {
int rounded(double value) { return int(std::nearbyint(value)); }
QSize scaled(QSize size, double factor) { return { qMax(1, rounded(size.width() * factor)), qMax(1, rounded(size.height() * factor)) }; }
QSize multiple(QSize size, int value) {
    return { ((size.width() + value - 1) / value) * value, ((size.height() + value - 1) / value) * value };
}
double pixels(QSize size) { return double(size.width()) * size.height(); }
QJsonArray extent(QSize size) { return { size.width(), size.height() }; }
struct Images {
    QByteArray blob;
    QJsonArray offsets;
    QString* error;
    QImage decode(const QJsonValue& value) {
        if (!value.isString() || value.toString().size() > 32 * 1024 * 1024) { *error = "Invalid input image"; return {}; }
        QBuffer input; input.setData(QByteArray::fromBase64(value.toString().toLatin1())); input.open(QIODevice::ReadOnly);
        QImageReader reader(&input);
        if (!reader.size().isValid() || pixels(reader.size()) > 16777216) { *error = "Invalid input image size"; return {}; }
        auto result = reader.read();
        if (result.isNull()) *error = "Could not decode input image";
        return result;
    }
    int append(const QImage& image) {
        if (image.isNull()) { *error = "Missing input image"; return -1; }
        QByteArray bytes; QBuffer output(&bytes); output.open(QIODevice::WriteOnly);
        if (!image.save(&output, "PNG") || blob.size() + bytes.size() > 64 * 1024 * 1024) { *error = "Input images are too large"; return -1; }
        offsets.append(blob.size()); blob.append(bytes); return offsets.size() - 1;
    }
};
QString merge(const QString& prompt, const QString& style) {
    if (style.contains("{prompt}")) { QString result = style; result.replace("{prompt}", prompt); return result; }
    if (style.isEmpty()) return prompt;
    return prompt.isEmpty() ? style : QString(prompt + ", " + style);
}
}

QJsonObject CloudWorkflow::prepare(const QJsonObject& input, const QJsonObject& resources, QString* error) {
    if (input["prompt_mode"] == "a1111" || input["style_options"].toObject()["prompt_mode"] == "a1111") {
        *error = "A1111 prompt syntax requires a ComfyUI server with ComfyUI_smZNodes; Interstice does not provide these nodes";
        return {};
    }
    error->clear();
    const QSize target(input["width"].toInt(), input["height"].toInt());
    if (!target.isValid() || target.width() > 8192 || target.height() > 8192 || pixels(target) > 16777216) {
        *error = "Invalid canvas size"; return {};
    }
    const auto mode = input["mode"].toString();
    const auto upscaleOptions = input["upscale_options"].toObject();
    const bool tiledUpscale = mode == "upscale" && upscaleOptions["use_diffusion"].toBool();
    const bool controlImage = input["operation"] == "control_image";
    if (mode == "background") { *error = "Background separation is not provided by this Interstice client yet"; return {}; }
    if (!QStringList { "generate", "edit", "upscale" }.contains(mode)) { *error = "Unknown workspace"; return {}; }
    const auto model = input["model"].toString();
    const auto checkpoint = resources["checkpoints"].toObject()[model].toObject();
    if (checkpoint.isEmpty() && (mode != "upscale" || tiledUpscale) && !controlImage) { *error = "This model is not available on Interstice"; return {}; }
    const auto arch = checkpoint["arch"].toString("sdxl");
    const bool xl = QStringList { "sdxl", "illu", "illu_v" }.contains(arch);
    const bool instruction = QStringList { "flux_k", "flux2_4b", "flux2_9b", "qwen_e", "qwen_e_p", "qwen2", "krea2" }.contains(arch);
    const bool editModel = QStringList { "flux_k", "qwen_e", "qwen_e_p", "qwen_l" }.contains(arch);
    const int mult = arch == "qwen2" ? 32 : 16;
    const auto options = input["style_options"].toObject();
    const bool selected = input["mask"].isString() && !input["mask"].toString().isEmpty();
    const auto kind = controlImage ? "control_image" : mode == "upscale" ? tiledUpscale ? "upscale_tiled" : "upscale_simple"
        : mode == "generate" ? selected ? "inpaint" : "generate" : selected ? "refine_region" : "refine";
    QJsonObject work { { "kind", kind }, { "batch_count", qBound(1, input["batch"].toInt(1), 4) } };
    Images images { {}, {}, error };
    QSize initial = target, desired = target, sourceExtent = target, output = target;
    QImage source;
    QImage original;
    if (mode != "generate" || selected || controlImage) {
        source = images.decode(input["image"]);
        if (source.size() != target) { *error = "Canvas size does not match its input image"; return {}; }
        original = source;
    }
    if (mode == "upscale") {
        output = desired = scaled(target, qBound(1.0, input["scale"].toDouble(2), 4.0));
        if (pixels(output) > 67108864) { *error = "Upscale exceeds 64 megapixels"; return {}; }
        const auto upscalers = resources["upscalers"].toArray();
        auto upscaler = upscaleOptions["model"].toString();
        if (upscaler.isEmpty()) upscaler = upscalers.isEmpty() ? QString() : upscalers.first().toString();
        if ((!tiledUpscale || input["scale"].toDouble(2) > 1) && !upscalers.contains(upscaler)) {
            *error = "No matching upscaler is available on Interstice"; return {};
        }
        work["upscale"] = QJsonObject { { "model", tiledUpscale && input["scale"].toDouble(2) <= 1 ? QString() : upscaler },
            { "tile_overlap", upscaleOptions["tile_overlap_mode"] == "custom" ? qBound(0, upscaleOptions["tile_overlap"].toInt(48), 128) : -1 } };
        work["batch_count"] = 1;
        if (tiledUpscale) {
            initial = multiple(output, mult);
            int tile = options["preferred_resolution"].toInt();
            if (tile <= 0) tile = arch == "sd15" ? 800 : 1024;
            tile = qMax(tile, qMax(output.width(), output.height()) / 12);
            desired = multiple(QSize(tile - 128, tile - 128), mult);
        }
    } else if (!controlImage) {
        desired = scaled(target, qBound(.1, input["resolution_multiplier"].toDouble(1), 2.0));
        const int limit = qBound(1, input["max_pixel_count"].toInt(6), 8) * 1000000;
        if (pixels(desired) > int(limit * 1.05)) desired = scaled(desired, std::sqrt(limit / pixels(desired)));
        int minSize = arch == "sd15" ? 512 : xl ? 640 : arch == "sd3" ? 512 : 256;
        int maxSize = arch == "sd15" ? 768 : xl ? 1280 : arch == "sd3" ? 1536 : 2048;
        double minPixels = arch == "sd15" ? 262144 : xl ? 640000 : 262144;
        double maxPixels = arch == "sd15" ? 393216 : xl ? 1048576 : arch == "sd3" ? 2359296 : 4194304;
        const int preferred = options["preferred_resolution"].toInt();
        if (preferred > 0) {
            const int offset = ((rounded(preferred * .2) + mult - 1) / mult) * mult;
            minSize = preferred - offset; maxSize = preferred + offset; minPixels = maxPixels = double(preferred) * preferred;
        }
        const double low = std::sqrt(minPixels / pixels(desired)), high = std::sqrt(maxPixels / pixels(desired));
        const bool instructionEdit = mode == "edit" && input["instruction_edit"].toBool()
            && (arch == "qwen2" || arch == "krea2");
        const bool downscale = !editModel && (mode == "generate" || (selected && !instructionEdit && input["strength"].toDouble(1) >= .7));
        if (downscale && high < .9 && (desired.width() > maxSize || desired.height() > maxSize)) {
            sourceExtent = initial = multiple(scaled(desired, high), mult); desired = multiple(desired, mult);
            if (!source.isNull()) source = source.scaled(initial, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        } else if (low > 1 && desired.width() < minSize && desired.height() < minSize) {
            const bool nativeSize = target.width() >= minSize && target.height() >= minSize && target.width() <= maxSize && target.height() <= maxSize;
            initial = desired = multiple(nativeSize ? target : scaled(desired, low), mult);
        } else initial = desired = multiple(desired, mult);
        if (pixels(target) > pixels(desired)) {
            sourceExtent = desired;
            if (!source.isNull()) source = source.scaled(desired, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    }
    QJsonObject imageInput { { "extent", QJsonObject { { "input", extent(sourceExtent) }, { "initial", extent(initial) },
        { "desired", extent(desired) }, { "target", extent(output) } } } };
    if (!source.isNull()) imageInput["initial_image"] = images.append(source);
    if (selected) {
        auto mask = images.decode(input["mask"]);
        if (mask.size() != target) { *error = "Selection size does not match the canvas"; return {}; }
        imageInput["hires_mask"] = images.append(mask.convertToFormat(QImage::Format_Grayscale8));
        if (initial != target && !original.isNull()) imageInput["hires_image"] = images.append(original);
        const auto custom=input["inpaint_options"].toObject();
        auto bounds=input["selection_bounds"].toArray();
        if(bounds.isEmpty())bounds=QJsonArray{0,0,target.width(),target.height()};
        if(bounds.size()!=4||bounds[0].toInt()<0||bounds[1].toInt()<0||bounds[2].toInt()<=0||bounds[3].toInt()<=0
            ||bounds[0].toInt()+bounds[2].toInt()>target.width()||bounds[1].toInt()+bounds[3].toInt()>target.height()) {
            *error="Selection bounds exceed the context image";return {};
        }
        QString inpaintMode=custom["mode"].toString("custom"),fill=custom["fill"].toString("none");
        if(inpaintMode=="automatic")inpaintMode=custom["resolved_mode"].toString(bounds[2].toInt()>=target.width()||bounds[3].toInt()>=target.height()?"expand":"fill");
        bool seamless=custom["use_inpaint_model"].toBool(true),focus=custom["use_condition_mask"].toBool();
        if(inpaintMode!="custom") {
            fill=inpaintMode=="fill"?"blur":inpaintMode=="expand"?"border":inpaintMode=="add_object"?"neutral":inpaintMode=="remove_object"?"inpaint":"replace";
            const double strength=input["strength"].toDouble(1);
            seamless=arch=="sd15"?strength>.5:xl?strength>.8:QStringList{"flux","zimage","anima"}.contains(arch)&&strength==1;
            focus=arch=="sd15"&&inpaintMode=="add_object"&&!input["prompt"].toString().isEmpty()&&input["controls"].toArray().isEmpty();
            if(editModel){inpaintMode="custom";fill="none";}
        }
        if(mode=="edit"&&input["instruction_edit"].toBool())fill="none";
        work["inpaint"] = QJsonObject {{"mode",inpaintMode},{"fill",fill},{"target_bounds",bounds},
            {"use_inpaint_model",!custom.isEmpty()&&seamless},{"use_condition_mask",focus},
            {"grow",custom["grow"].toInt()},{"feather",custom["feather"].toInt()},{"blend",custom["blend"].toInt()}};
        if (mode == "generate") work["crop_upscale_extent"] = extent(desired);
    }
    work["images"] = imageInput;
    if (controlImage) work["control_mode"] = input["control_mode"];
    if ((mode != "upscale" || tiledUpscale) && !controlImage) {
        const bool usePrompt = !tiledUpscale || upscaleOptions["use_prompt"].toBool();
        QString positive = merge(usePrompt ? input["prompt"].toString()
            : editModel ? "Enhance image quality. Preserve original content." : "4k uhd", options["style_prompt"].toString());
        QString negative = merge(usePrompt ? input["negative"].toString() : QString(), options["negative_prompt"].toString());
        QJsonArray loras = input["loras"].toArray();
        const auto available = resources["loras"].toArray();
        auto extract = [&](QString& text) {
            auto matches = QRegularExpression("<lora:([^:<>]+)(?::([^:<>]*))?>", QRegularExpression::CaseInsensitiveOption).globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                const QString name = match.captured(1).replace('\\', '/');
                QString resolved;
                for (auto value : available) {
                    QString candidate = value.toString().replace('\\', '/');
                    if (candidate.endsWith(".safetensors")) candidate.chop(12);
                    if (candidate.compare(name, Qt::CaseInsensitive) == 0) { resolved = value.toString(); break; }
                }
                bool valid = true;
                const double strength = match.captured(2).isEmpty() ? 1 : match.captured(2).toDouble(&valid);
                if (resolved.isEmpty() || !valid || !std::isfinite(strength)) { *error = "A prompt LoRA is not available on Interstice"; return; }
                loras.append(QJsonObject { { "name", resolved }, { "strength", strength } });
            }
            text.remove(QRegularExpression("<lora:[^<>]+>", QRegularExpression::CaseInsensitiveOption));
            text = text.trimmed();
        };
        extract(positive); extract(negative);
        for (auto value : loras) if (!available.contains(value.toObject()["name"])) { *error = "A selected LoRA is not available on Interstice"; return {}; }
        QJsonObject models { { "checkpoint", model }, { "version", arch }, { "loras", loras },
            { "clip_skip", options["clip_skip"].toInt() } };
        if (options["vae"].isString() && options["vae"] != "Checkpoint Default") models["vae"] = options["vae"];
        models["v_prediction_zsnr"] = options["v_prediction_zsnr"].toBool();
        const int steps = qBound(1, input["steps"].toInt(20), 1000);
        const double strength = tiledUpscale ? editModel ? 1 : qBound(.01, upscaleOptions["strength"].toDouble(.3), 1.0)
            : qBound(.01, input["strength"].toDouble(1), 1.0);
        int total = steps, start = rounded(steps * (1 - strength));
        if (strength < 1 && total - start < 4) { total = int(std::floor(4 / strength)); start = total - 4; }
        QString sampler = input["sampler"].toString(), scheduler = input["scheduler"].toString();
        if (sampler.isEmpty()) sampler = arch.startsWith("flux") || arch.startsWith("qwen") ? "euler" : "dpmpp_2m";
        if (scheduler.isEmpty()) scheduler = sampler == "euler" ? "simple" : "karras";
        work["models"] = models;
        work["sampling"] = QJsonObject { { "sampler", sampler }, { "scheduler", scheduler },
            { "cfg_scale", input["cfg"].toDouble(4) }, { "total_steps", total }, { "start_step", start }, { "seed", input["seed"] } };
        QJsonArray controls;
        auto appendControl = [&](QJsonObject control) {
            auto image = images.decode(control["image"]);
            const auto type = control["mode"].toString("reference");
            const bool reference = QStringList { "reference", "style", "composition", "face" }.contains(type);
            if (reference && !instruction) image = image.scaled(224, 224, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            else if (reference && image.height() > target.height()) image = image.scaled(
                qMax(1, image.width() * target.height() / image.height()), target.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            else if (!reference) image = image.scaled(desired, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (QStringList { "scribble", "line_art", "soft_edge", "canny_edge", "stencil", "segmentation" }.contains(type)) {
                QImage opaque(image.size(), QImage::Format_ARGB32); opaque.fill(Qt::white);
                QPainter painter(&opaque); painter.drawImage(0, 0, image); painter.end(); image = opaque;
            }
            return QJsonObject { { "mode", type }, { "image", images.append(image) }, { "strength", control["strength"].toDouble(1) },
                { "range", QJsonArray { control["start"].toDouble(), control["end"].toDouble(1) } } };
        };
        if (tiledUpscale && !editModel && upscaleOptions["unblur_strength"].toDouble(.5) > 0) {
            const auto models = resources["resources"].toObject();
            if (!models["controlnet-blur-" + arch].isNull() && models.contains("controlnet-blur-" + arch))
                controls.append(QJsonObject { { "mode", "blur" }, { "strength", upscaleOptions["unblur_strength"].toDouble(.5) },
                    { "range", QJsonArray { 0, 1 } } });
        }
        if (usePrompt) for (auto image : input["references"].toArray()) controls.append(appendControl({ { "image", image } }));
        QMap<QString, QJsonArray> regionalControls;
        for (auto value : usePrompt ? input["controls"].toArray() : QJsonArray()) {
            const auto control = value.toObject();
            if (control["region"].toString().isEmpty()) controls.append(appendControl(control));
            else regionalControls[control["region"].toString()].append(appendControl(control));
        }
        if (controls.size() > 4) { *error = "Interstice supports at most four global control layers"; return {}; }
        QJsonArray regions;
        QSet<QString> seen;
        for (auto value : usePrompt ? input["regions"].toArray() : QJsonArray()) {
            if (!(arch == "sd15" || xl || arch == "anima")) { *error = "This architecture does not support regional prompts"; return {}; }
            const auto region = value.toObject();
            const auto id = region["id"].toString();
            if (id.isEmpty() || seen.contains(id)) { *error = "Invalid or duplicate region ID"; return {}; }
            seen.insert(id);
            auto mask = images.decode(region["mask"]).convertToFormat(QImage::Format_Grayscale8);
            if (mask.size() != target) { *error = "Region mask must match the selected canvas area"; return {}; }
            regions.append(QJsonObject { { "mask", images.append(mask) }, { "bounds", QJsonArray { 0, 0, target.width(), target.height() } },
                { "positive", region["prompt"] }, { "control", regionalControls.take(id) } });
        }
        if (!regionalControls.isEmpty()) { *error = "The control layer references a missing region"; return {}; }
        if (!regions.isEmpty()) {
            QImage background(target, QImage::Format_Grayscale8); background.fill(255);
            regions.prepend(QJsonObject { { "mask", images.append(background) }, { "bounds", QJsonArray { 0, 0, target.width(), target.height() } },
                { "positive", input["prompt"] } });
        }
        work["conditioning"] = QJsonObject { { "positive", positive }, { "negative", negative },
            { "control", controls }, { "regions", regions }, { "edit_reference", mode == "edit" && input["instruction_edit"].toBool() } };
    }
    if (!images.offsets.isEmpty()) work["image_data"] = QJsonObject { { "base64", QString::fromLatin1(images.blob.toBase64()) }, { "offsets", images.offsets } };
    return error->isEmpty() ? work : QJsonObject();
}
QJsonObject CloudWorkflow::metadata(const QJsonObject& work) {
    const auto conditioning = work["conditioning"].toObject();
    return { { "prompt_final", conditioning["positive"] }, { "negative_prompt_final", conditioning["negative"] } };
}
