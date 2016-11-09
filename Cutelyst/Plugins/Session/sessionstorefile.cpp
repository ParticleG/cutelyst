/*
 * Copyright (C) 2015-2016 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "sessionstorefile_p.h"

#include <Cutelyst/Context>

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QDataStream>
#include <QLoggingCategory>
#include <QCoreApplication>

using namespace Cutelyst;

Q_LOGGING_CATEGORY(C_SESSION_FILE, "cutelyst.plugin.sessionfile")

#define SESSION_STORE_FILE_SAVE "__session_store_file_save"
#define SESSION_STORE_FILE_DATA "__session_store_file_data"

SessionStoreFile::SessionStoreFile(QObject *parent) : SessionStore(parent)
  , d_ptr(new SessionStoreFilePrivate)
{

}

SessionStoreFile::~SessionStoreFile()
{
    delete d_ptr;
}

QVariant SessionStoreFile::getSessionData(Context *c, const QString &sid, const QString &key, const QVariant &defaultValue)
{
    Q_D(const SessionStoreFile);

    const QVariantHash data = SessionStoreFilePrivate::loadSessionData(c, sid);

    return data.value(key, defaultValue);
}

bool SessionStoreFile::storeSessionData(Context *c, const QString &sid, const QString &key, const QVariant &value)
{
    Q_D(const SessionStoreFile);

    QVariantHash data = SessionStoreFilePrivate::loadSessionData(c, sid);

    data.insert(key, value);
    c->setProperty(SESSION_STORE_FILE_DATA, data);
    c->setProperty(SESSION_STORE_FILE_SAVE, true);

    return true;
}

bool SessionStoreFile::deleteSessionData(Context *c, const QString &sid, const QString &key)
{
    Q_D(const SessionStoreFile);

    QVariantHash data = SessionStoreFilePrivate::loadSessionData(c, sid);

    data.remove(key);
    c->setProperty(SESSION_STORE_FILE_DATA, data);
    c->setProperty(SESSION_STORE_FILE_SAVE, true);

    return true;
}

bool SessionStoreFile::deleteExpiredSessions(Context *c, quint64 expires)
{
    Q_UNUSED(c)
    Q_UNUSED(expires)
    return true;
}

QVariantHash Cutelyst::SessionStoreFilePrivate::loadSessionData(Context *c, const QString &sid)
{
    QVariantHash data;
    const QVariant sessionVariant = c->property(SESSION_STORE_FILE_DATA);
    if (!sessionVariant.isNull()) {
        data = sessionVariant.toHash();
        return data;
    }

    const static QString root = QDir::tempPath()
            + QLatin1Char('/')
            + QCoreApplication::applicationName()
            + QLatin1String("/session/data");

    auto file = new QFile(root + QLatin1Char('/') + sid, c);
    if (!file->open(QIODevice::ReadWrite)) {
        if (!QDir().mkpath(root)) {
            qCWarning(C_SESSION_FILE) << "Failed to create path for session object" << root;
            return data;
        }

        if (!file->open(QIODevice::ReadWrite)) {
            return data;
        }
    }

    // Commit data when Context gets deleted
    QObject::connect(c, &Context::destroyed, [=] () {
        if (!c->property(SESSION_STORE_FILE_SAVE).toBool()) {
            return;
        }

        QVariantHash data = c->property(SESSION_STORE_FILE_DATA).toHash();

        if (data.isEmpty()) {
            QFile::remove(file->fileName());
        } else {
            QLockFile lock(file->fileName() + QLatin1String(".lock"));
            if (lock.lock()) {
                QDataStream in(file);

                if (file->pos()) {
                    file->seek(0);
                }

                in << data;

                if (file->pos() < file->size()) {
                    file->resize(file->pos());
                }

                file->flush();
                lock.unlock();
            }
        }
    });

    // Load data
    QLockFile lock(file->fileName() + QLatin1String(".lock"));
    if (lock.lock()) {
        QDataStream in(file);
        in >> data;
        lock.unlock();
    }

    c->setProperty(SESSION_STORE_FILE_DATA, data);

    return data;
}

#include "moc_sessionstorefile.cpp"
