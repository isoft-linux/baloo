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

#ifndef BALOO_DATABASE_H
#define BALOO_DATABASE_H

#include <QString>
#include <lmdb.h>

namespace Baloo {

class Document;
class PostingDB;
class DocumentDB;

class Database
{
public:
    Database(const QString& path);
    ~Database();

    // FIXME: Return codes?
    void addDocument(const Document& doc);
    void removeDocument(uint id);

    bool hasDocument(uint id);
    Document document(uint id);

    void commit();
private:
    PostingDB* m_postingDB;
    DocumentDB* m_documentDB;

    MDB_env* m_env;
    MDB_txn* m_txn;
};
}

#endif // BALOO_DATABASE_H
