// SPDX-License-Identifier: GPL-3.0-or-later
#include "PersonalFonts.h"
#include <QDirIterator>
#include <QFontDatabase>
#include <QSet>

void PersonalFonts::load(const QString& directory) {
    static QSet<QString> loaded;
    if (loaded.contains(directory))
        return;
    loaded.insert(directory);
    QDirIterator files(directory, { "*.ttf", "*.ttc", "*.otf" }, QDir::Files,
        QDirIterator::Subdirectories);
    while (files.hasNext())
        QFontDatabase::addApplicationFont(files.next());
}
