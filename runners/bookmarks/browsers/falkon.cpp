/*
 *   Copyright 2007 Glenn Ergeerts <glenn.ergeerts@telenet.be>
 *   Copyright 2012 Glenn Ergeerts <marco.gulino@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "falkon.h"
#include <QDebug>
#include "bookmarksrunner_defs.h"
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <KSharedConfig>
#include <KConfigGroup>
#include "favicon.h"


Falkon::Falkon(QObject* parent): QObject(parent), m_favicon(new FallbackFavicon(this))
{
}

QList<BookmarkMatch> Falkon::match(const QString& term, bool addEverything )
{
    QList<BookmarkMatch> matches;
    for(const auto &bookmark : qAsConst(m_falkonBookmarkEntries)) {
        const QString url = bookmark.value(QStringLiteral("url")).toString();
        BookmarkMatch bookmarkMatch(m_favicon->iconFor(url), term, bookmark.value(QStringLiteral("name")).toString(), url);
        bookmarkMatch.addTo(matches, addEverything);
    }
    return matches;
}

void Falkon::prepare()
{
    m_falkonBookmarkEntries = readProfileBookmarks(getStartupProfileDir());
}

void Falkon::teardown()
{
    m_falkonBookmarkEntries.clear();
}

QString Falkon::getStartupProfileDir()
{
    const QString profilesIni = QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("/falkon/profiles/profiles.ini"));
    const QString startupProfile =KSharedConfig::openConfig(profilesIni)->group("Profiles").readEntry("startProfile");
    return QFileInfo(profilesIni).dir().absoluteFilePath(startupProfile);
}

QList<QJsonObject> Falkon::readProfileBookmarks(const QString &profilePath)
{
    const QString fileName = QDir(profilePath).filePath(QStringLiteral("bookmarks.json"));
    if (!QFileInfo::exists(fileName)) {
        return {};
    }
    QFile bookmarksFile(fileName);
    if (!bookmarksFile.open(QFile::ReadOnly)) {
        return {};
    }

    QJsonDocument doc = QJsonDocument::fromJson(bookmarksFile.readAll());
    if (!doc.isObject()) {
        return {};
    }

    const QJsonObject roots = doc.object().value(QLatin1String("roots")).toObject();
    const QStringList bookmarksDisplayLocations = roots.keys();
    QList<QJsonObject> bookmarks;
    for (const auto &key : bookmarksDisplayLocations) {
        const auto children = roots.value(key).toObject().value(QLatin1String("children")).toArray();
        for(const auto bookmark : children) {
            const QJsonObject bookmarkObj = bookmark.toObject();
            if (bookmarkObj.value(QLatin1String("type")).toString() == QLatin1String("url")) {
                bookmarks << bookmarkObj;
            }
        }
    }

    return bookmarks;
}

