// SPDX-License-Identifier: GPL-3.0-or-later
#include "PromptLogic.h"
#include <QFile>
#include <QJsonDocument>

static void initializePromptResources() { Q_INIT_RESOURCE(prompt); }
PromptLogic::PromptLogic() {
    initializePromptResources();
    QFile file(":/baron/prompt/logic.js");
    if (!file.open(QIODevice::ReadOnly)) { m_error = "Missing prompt logic"; return; }
    const auto evaluated = m_engine.evaluate(QString::fromUtf8(file.readAll()), "prompt/logic.js");
    if (evaluated.isError()) { m_error = evaluated.toString(); return; }
    m_functions = m_engine.globalObject().property("PromptLogic");
    m_engine.evaluate(R"JS(
        PromptLogic.withOptions = function(name, args) {
            var options = args[args.length - 1];
            if (options && options.dictionary) {
                var dictionary = options.dictionary;
                options.lookup = function(key) { return dictionary[key] || null; };
            }
            if (options && Array.isArray(options.triggerWords)) options.triggerWords = new Set(options.triggerWords);
            return PromptLogic[name].apply(null, args);
        };
        PromptLogic.typoForCandidates = function(key, tags) {
            return PromptLogic.suggestTag(key, PromptLogic.buildIndex(tags));
        };
    )JS");
}
QJsonValue PromptLogic::call(const QString& function, const QJsonArray& arguments) {
    if (!m_error.isEmpty()) return {};
    QJSValueList args;
    if (function == "organizePrompt" || function == "moveSegmentLogically" || function == "categoryStepTarget") {
        auto wrapper = m_functions.property("withOptions");
        const auto result = wrapper.call({function, m_engine.toScriptValue(arguments.toVariantList())});
        if (result.isError()) { m_error = result.toString(); return {}; }
        return result.isNull() || result.isUndefined() ? QJsonValue() : QJsonValue::fromVariant(result.toVariant());
    }
    for (const auto& argument : arguments) args.append(m_engine.toScriptValue(argument.toVariant()));
    auto fn = m_functions.property(function);
    const auto result = fn.call(args);
    if (result.isError()) { m_error = result.toString(); return {}; }
    return result.isNull() || result.isUndefined() ? QJsonValue() : QJsonValue::fromVariant(result.toVariant());
}
QJsonArray PromptLogic::segments(const QString& text) {
    return call("segmentPrompt", {text}).toArray();
}
