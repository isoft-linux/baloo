/*
   This file is part of the KDE Baloo project.
 * Copyright (C) 2015  Vishesh Handa <vhanda@kde.org>
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

#include "../database.h"
#include "../document.h"

#include <QTest>
#include <QTemporaryDir>

using namespace Baloo;

class DatabaseTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void test();
};

void DatabaseTest::test()
{
    QTemporaryDir dir;

    Database db(dir.path());
    QCOMPARE(db.hasDocument(1), false);

    Document doc;
    doc.setId(1);
    doc.addTerm("a");
    doc.addTerm("ab");
    doc.addTerm("abc");
    doc.addTerm("power");

    db.addDocument(doc);
    QCOMPARE(db.hasDocument(1), true);
    QCOMPARE(db.document(1), doc);

    db.removeDocument(1);
    QCOMPARE(db.hasDocument(1), false);
    QCOMPARE(db.document(1), Document());
}

QTEST_MAIN(DatabaseTest)

#include "databasetest.moc"
