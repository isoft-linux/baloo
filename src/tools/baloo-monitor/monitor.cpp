/*
 * This file is part of the KDE Baloo Project
 * Copyright (C) 2015  Pinak Ahuja <pinak.ahuja@gmail.com>
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

#include "monitor.h"

#include "database.h"
#include "transaction.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusConnectionInterface>
#include <QDebug>
#include <QStandardPaths>
#include <QDBusServiceWatcher>
#include <QProcess>

using namespace BalooMonitor;
Monitor::Monitor(QObject *parent)
    : QObject(parent)
    , m_bus(QDBusConnection::sessionBus())
    , m_url(QStringLiteral("Idle"))
    , m_balooInterface(0)
    , m_filesIndexed(0)
{
    QString balooService = QStringLiteral("org.kde.baloo");
    m_bus.connect("", "/contentindexer", balooService, "startedWithFile", this, SLOT(newFile(QString)));
    if (m_bus.interface()->isServiceRegistered(QLatin1String("org.kde.baloo"))) {
        // baloo is already running
        balooStarted(balooService);
    } else {
        m_balooRunning = false;
        QDBusServiceWatcher *balooWatcher = new QDBusServiceWatcher(balooService,
                                                                    m_bus, QDBusServiceWatcher::WatchForRegistration);
        connect(balooWatcher, &QDBusServiceWatcher::serviceRegistered, this, &Monitor::balooStarted);
    }
}

void Monitor::newFile(QString url)
{
    if (m_totalFiles == 0) {
        fetchTotalFiles();
    }
    m_url = url;
    if (++m_filesIndexed == m_totalFiles) {
        m_url = QStringLiteral("Done");
    }
    Q_EMIT newFileIndexed();
}

QString Monitor::suspendState() const
{
    return m_suspended ?  QStringLiteral("Resume") : QStringLiteral("Suspend");
}

void Monitor::toggleSuspendState()
{
    Q_ASSERT(m_balooInterface != 0);

    if (m_suspended) {
        m_balooInterface->resume();
        m_suspended = false;
    } else {
        m_balooInterface->suspend();
        m_suspended = true;
    }
    Q_EMIT suspendStateChanged();
}

void Monitor::balooStarted(const QString& service)
{
    /*
     * TODO: send a dbus signal to baloo to let it know montitor is running,
     * use that signal in baloo to enable exporting urls being indexed
     */
    if (service != QStringLiteral("org.kde.baloo")) {
        return;
    }
    m_balooRunning = true;

    m_balooInterface = new org::kde::baloo(QStringLiteral("org.kde.baloo"),
                                            QStringLiteral("/indexer"),
                                            m_bus, this);

    m_suspended = m_balooInterface->isSuspended();
    fetchTotalFiles();
    Q_EMIT balooStateChanged();
    Q_EMIT suspendStateChanged();
}

void Monitor::fetchTotalFiles()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/baloo");
    Baloo::Database db(path);
    db.open(Baloo::Database::OpenDatabase);
    Baloo::Transaction tr(&db, Baloo::Transaction::ReadOnly);
    m_totalFiles = tr.size();
    m_filesIndexed = tr.size() - tr.phaseOneSize();
    Q_EMIT totalFilesChanged();
}

void Monitor::startBaloo()
{
    const QString exe = QStandardPaths::findExecutable(QLatin1String("baloo_file"));
    QProcess::startDetached(exe);
}
