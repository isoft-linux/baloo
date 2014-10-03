/*
 * This file is part of the KDE Baloo Project
 * Copyright (C) 2013  Vishesh Handa <me@vhanda.in>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "filefetchjob.h"
#include "filemapping.h"
#include "db.h"
#include "file.h"
#include "file_p.h"
#include "searchstore.h"
#include "baloo_xattr_p.h"

#include <QTimer>
#include <QFile>

#include <QJsonDocument>
#include <QJsonObject>

#include <xapian.h>

#include <QDebug>

#include <sys/types.h>

using namespace Baloo;

class FileFetchJob::Private {
public:
    QList<File> m_files;

    void fetchUserMetadata(File& file);
};

FileFetchJob::FileFetchJob(const QString& url, QObject* parent)
    : KJob(parent)
    , d(new Private)
{
    File file(url);
    d->m_files << file;
}

FileFetchJob::FileFetchJob(const File& file, QObject* parent)
    : KJob(parent)
    , d(new Private)
{
    d->m_files << file;
}

FileFetchJob::FileFetchJob(const QStringList& urls, QObject* parent)
    : KJob(parent)
    , d(new Private)
{
    Q_FOREACH (const QString& url, urls)
        d->m_files << File(url);
}

FileFetchJob::~FileFetchJob()
{
    delete d;
}

void FileFetchJob::start()
{
    QTimer::singleShot(0, this, SLOT(doStart()));
}

void FileFetchJob::doStart()
{
    QScopedPointer<Xapian::Database> db;
    try {
        db.reset(new Xapian::Database(fileIndexDbPath()));
    }
    catch (const Xapian::Error& err) {
        setError(Error_CouldNotOpenDatabase);
        setErrorText(QLatin1String("Could not open database ") + fileIndexDbPath());
        emitResult();
        return;
    }

    QList<File>::iterator it = d->m_files.begin();
    for (; it != d->m_files.end(); ++it) {
        File& file = *it;
        const QString& url = file.url();
        if (url.size() && !QFile::exists(url)) {
            setError(Error_FileDoesNotExist);
            setErrorText(QLatin1String("File ") + url + QLatin1String(" does not exist"));
            emitResult();
            return;
        }

        if (file.id().size() && !file.id().startsWith("file")) {
            setError(Error_InvalidId);
            setErrorText(QLatin1String("Invalid Id ") + QString::fromUtf8(file.id()));
            emitResult();
            return;
        }

        FileMapping fileMap;
        fileMap.setId(deserialize("file", file.id()));
        fileMap.setUrl(file.url());

        if (fileMap.fetch(fileMappingDb())) {
            const int id = fileMap.id();
            file.setId(serialize("file", id));
            file.setUrl(fileMap.url());

            // Fetch data from Xapian
            try {
                Xapian::Document doc = db->get_document(id);

                std::string docData = doc.get_data();
                const QByteArray arr(docData.c_str(), docData.length());

                QJsonDocument jdoc = QJsonDocument::fromJson(arr);
                const QVariantMap varMap = jdoc.object().toVariantMap();

                file.d->propertyMap = KFileMetaData::toPropertyMap(varMap);
            }
            catch (const Xapian::DocNotFoundError&){
                // Send file for indexing to baloo_file
            }
            catch (const Xapian::InvalidArgumentError& err) {
                qWarning() << err.get_msg().c_str();
            }
            catch (const Xapian::Error& err) {
                qWarning() << "Xapian error of type" << err.get_type() << ":" << err.get_msg().c_str();
            }
        } else {
            qDebug() << "No file index information found" << url;
            // TODO: Send file for indexing!!
        }

        d->fetchUserMetadata(file);
        Q_EMIT fileReceived(file);
    }
    emitResult();
}

void FileFetchJob::Private::fetchUserMetadata(File& file)
{
    const QString url = file.url();

    QString rating;
    baloo_getxattr(url, QLatin1String("user.baloo.rating"), &rating);

    QString tags;
    baloo_getxattr(url, QLatin1String("user.xdg.tags"), &tags);

    QString comment;
    baloo_getxattr(url, QLatin1String("user.xdg.comment"), &comment);

    file.setRating(rating.toInt());
    file.setTags(tags.split(QLatin1Char(','), QString::SkipEmptyParts));
    file.setUserComment(comment);
}

File FileFetchJob::file() const
{
    if (d->m_files.size() >= 1)
        return d->m_files.first();
    else
        return File();
}

QList<File> FileFetchJob::files() const
{
    return d->m_files;
}


