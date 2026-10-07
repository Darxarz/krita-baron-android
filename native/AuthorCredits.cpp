// SPDX-License-Identifier: GPL-3.0-or-later
#include "AuthorCredits.h"
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

AuthorCredits::AuthorCredits(QWidget* parent) : QWidget(parent) {
    setObjectName("originalAuthorCredits");
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 12, 0, 0);
    auto title = new QLabel("Krita AI Diffusion", this);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.6);
    title->setFont(font);
    layout->addWidget(title);
    for (const auto& text : { tr("Original author: Acly and contributors"),
             tr("Native Android port and modifications: Darxarz — Baron Edition"),
             tr("Krita: the Krita development team and contributors"),
             tr("This is an independent modified port. Acly and the Krita team do not maintain this edition.") }) {
        auto label = new QLabel(text, this);
        label->setWordWrap(true);
        label->setTextFormat(Qt::PlainText);
        layout->addWidget(label);
    }
    layout->addWidget(new QLabel(tr("Documentation and Support"), this));
    auto links = new QHBoxLayout;
    auto documents = new QLabel(
        "<a href='https://www.interstice.cloud'>Website</a><br><br>"
        "<a href='https://docs.interstice.cloud'>Handbook: Guides and Tips</a><br><br>"
        "<a href='https://github.com/Acly/krita-ai-diffusion'>GitHub</a>", this);
    documents->setObjectName("originalDocumentationLinks");
    auto contact = new QLabel(
        "<a href='https://github.com/Acly/krita-ai-diffusion/issues'>Issues</a><br><br>"
        "<a href='https://github.com/Acly/krita-ai-diffusion/discussions'>Discussions</a><br><br>"
        "<a href='https://discord.gg/pWyzHfHHhU'>Discord</a>", this);
    contact->setObjectName("originalCommunityLinks");
    for (auto label : { documents, contact }) {
        label->setOpenExternalLinks(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        links->addWidget(label, 1);
    }
    layout->addLayout(links);
    auto video = new QLabel("<a href='https://youtu.be/Ly6USRwTHe0'>" + tr("Original plugin video") + "</a>", this);
    video->setOpenExternalLinks(true);
    layout->addWidget(video);
    auto modifications = new QLabel(
        "<a href='https://github.com/Darxarz/krita-baron-android'>Baron Edition: native port source</a><br><br>"
        "<a href='https://github.com/Darxarz/krita-ai-diffusion-baron-edition'>Baron Edition: Python plugin modifications</a><br><br>"
        "<a href='https://orchestrion.su'>Orchestrion</a><br><br>"
        "<a href='https://www.gnu.org/licenses/gpl-3.0.html'>GNU GPL v3 or later</a>", this);
    modifications->setOpenExternalLinks(true);
    modifications->setObjectName("modifierSourceLinks");
    layout->addWidget(modifications);
}
