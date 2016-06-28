/*
 * This file is part of the KDE Baloo Project
 * Copyright (C) 2014-2015  Vishesh Handa <vhanda@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "queryparser.h"
#include "enginequery.h"

#include <QTextBoundaryFinder>
#include <QStringList>
#include <QVector>
#if HAVE_KJIEBA
#include <KJieba/KJieba_Interface>
#endif

using namespace Baloo;

QueryParser::QueryParser()
    : m_autoExpandSize(3)
{
}

namespace {
    bool containsSpace(const QString& string) {
        Q_FOREACH (const QChar& ch, string) {
            if (ch.isSpace())
                return true;
        }

        return false;
    }
}

EngineQuery QueryParser::parseQuery(const QString& text_, const QString& prefix)
{
    Q_ASSERT(!text_.isEmpty());

    QString text(text_);
    text.replace('_', ' ');

    QVector<EngineQuery> queries;
    QVector<EngineQuery> phraseQueries;

    int start = 0;
    int end = 0;
    int position = 0;

    bool inDoubleQuotes = false;
    bool inSingleQuotes = false;
    bool inPhrase = false;

    QTextBoundaryFinder bf(QTextBoundaryFinder::Word, text);
    for (; bf.position() != -1; bf.toNextBoundary()) {
        if (bf.boundaryReasons() & QTextBoundaryFinder::StartOfItem) {
            //
            // Check the previous delimiter
            int pos = bf.position();
            if (pos != end) {
                QString delim = text.mid(end, pos-end);
                if (delim.contains(QLatin1Char('"'))) {
                    if (inDoubleQuotes) {
                        queries << EngineQuery(phraseQueries, EngineQuery::Phrase);
                        phraseQueries.clear();
                        inDoubleQuotes = false;
                    }
                    else {
                        inDoubleQuotes = true;
                    }
                }
                else if (delim.contains(QLatin1Char('\''))) {
                    if (inSingleQuotes) {
                        queries << EngineQuery(phraseQueries, EngineQuery::Phrase);
                        phraseQueries.clear();
                        inSingleQuotes = false;
                    }
                    else {
                        inSingleQuotes = true;
                    }
                }
                else if (!containsSpace(delim)) {
                    if (!inPhrase && !queries.isEmpty()) {
                        EngineQuery q = queries.takeLast();
                        q.setOp(EngineQuery::Equal);
                        phraseQueries << q;
                    }
                    inPhrase = true;
                }
                else if (inPhrase && !phraseQueries.isEmpty()) {
                    queries << EngineQuery(phraseQueries, EngineQuery::Phrase);
                    phraseQueries.clear();
                    inPhrase = false;
                }
            }

            start = bf.position();
            continue;
        }
        else if (bf.boundaryReasons() & QTextBoundaryFinder::EndOfItem) {
            end = bf.position();

            QString str = text.mid(start, end - start);

            // Get the string ready for saving
            str = str.toLower();

            // Remove all accents
            const QString denormalized = str.normalized(QString::NormalizationForm_KD);
            QString cleanString;
            Q_FOREACH (const QChar& ch, denormalized) {
                auto cat = ch.category();
                if (cat != QChar::Mark_NonSpacing && cat != QChar::Mark_SpacingCombining && cat != QChar::Mark_Enclosing) {
                    cleanString.append(ch);
                }
            }

            str = cleanString.normalized(QString::NormalizationForm_KC);

            const QString term = prefix + str;
            const QByteArray arr = term.toUtf8();

            position++;
            if (inDoubleQuotes || inSingleQuotes || inPhrase) {
                phraseQueries << EngineQuery(arr, position);
            }
            else {
                if (m_autoExpandSize && arr.size() >= m_autoExpandSize) {
                    queries << EngineQuery(arr, EngineQuery::StartsWith, position);
                } else {
                    queries << EngineQuery(arr, position);
                }
            }
        }
    }

    if (inPhrase) {
        queries << EngineQuery(phraseQueries, EngineQuery::Phrase);
        phraseQueries.clear();
        inPhrase = false;
    }

    if (!phraseQueries.isEmpty()) {
        for (EngineQuery& q : phraseQueries) {
            if (m_autoExpandSize && q.term().size() >= m_autoExpandSize) {
                q.setOp(EngineQuery::StartsWith);
            } else {
                q.setOp(EngineQuery::Equal);
            }
        }
        queries << phraseQueries;
        phraseQueries.clear();
    }
    //detect text contains CJKV or not.
    //if contain CJKV, every CJKV character should be a term.
    //according to http://stackoverflow.com/questions/1366068/whats-the-complete-range-for-chinese-characters-in-unicode
    //Block                                   Range       Comment
    //CJK Unified Ideographs                  4E00-9FFF   Common
    //CJK Unified Ideographs Extension A      3400-4DFF   Rare
    //CJK Unified Ideographs Extension B      20000-2A6DF Rare, historic
    //CJK Compatibility Ideographs            F900-FAFF   Duplicates, unifiable variants, corporate characters
    //CJK Compatibility Ideographs Supplement 2F800-2FA1F Unifiable variants
    QString cjkString;
    int nCount = text.count();
    for (int i = 0; i < nCount; i++) {
        QChar cha = text.at(i);
        uint uni = cha.unicode();
        if ((uni >= 0x4E00 && uni <= 0x9FFF)    ||
            (uni >= 0x3400 && uni <= 0x4DFF)    ||
            (uni >= 0x20000 && uni <= 0x2A6DF)  ||
            (uni >= 0xF900 && uni <= 0xFAFF)    ||
            (uni >= 0x2F800 && uni <= 0x2FA1F)) {
            queries << EngineQuery(QString(cha).toUtf8(), EngineQuery::StartsWith);
            cjkString += QString(cha);
        }
    }

    if (!cjkString.isEmpty()) {
#if HAVE_KJIEBA
        KJieba::KJiebaInterface *interface = new KJieba::KJiebaInterface;
        QStringList words = interface->query(cjkString);
        Q_FOREACH (QString word, words)
           queries << EngineQuery(word.toUtf8(), EngineQuery::StartsWith);
        delete interface;
        interface = NULL;
#endif
    }

    if (queries.size() == 1) {
        return queries.first();
    }
    return EngineQuery(queries, EngineQuery::And);
}

void QueryParser::setAutoExapandSize(int size)
{
    m_autoExpandSize = size;
}
