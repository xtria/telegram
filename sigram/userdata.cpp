/*
    Copyright (C) 2014 Sialan Labs
    http://labs.sialan.org

    Sigram is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Sigram is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "userdata.h"
#include "telegramgui.h"
#include "telegram/strcuts.h"
#include "telegram_macros.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QFile>
#include <QDateTime>
#include <QSettings>
#include <QHash>

class SecretChatDBClass
{
public:
    int id;
    int userId;
    QString title;
};

class UserDataPrivate
{
public:
    QSqlDatabase db;
    QString path;

    QHash<int,DialogClass> dialogs;
    QHash<int,UserClass> contacts;
    QHash<int,QString> photos;
    QHash<int,SecretChatDBClass> secretChats;

    QHash<int,bool> mutes;
    QHash<int,bool> favorites;
};

UserData::UserData(QObject *parent) :
    QObject(parent)
{
    p = new UserDataPrivate;
    p->path = HOME_PATH  + "/userdata.db";

    if( !TelegramGui::settings()->value("initialize/userdata_db",false).toBool() )
        QFile::copy(TelegramGui::resourcesPath()+"/database/userdata.db",p->path);

    TelegramGui::settings()->setValue("initialize/userdata_db",true);
    QFile(p->path).setPermissions(QFileDevice::WriteOwner|QFileDevice::WriteGroup|QFileDevice::ReadUser|QFileDevice::ReadGroup);

    p->db = QSqlDatabase::addDatabase("QSQLITE",USERDATAS_DB_CONNECTION);
    p->db.setDatabaseName(p->path);

    reconnect();
}

void UserData::disconnect()
{
    p->db.close();
}

void UserData::reconnect()
{
    p->db.open();
    update_db();
    init_buffer();
}

void UserData::addMute(int id)
{
    QSqlQuery mute_query(p->db);
    mute_query.prepare("INSERT OR REPLACE INTO mutes (id,mute) VALUES (:id,:mute)");
    mute_query.bindValue(":id",id);
    mute_query.bindValue(":mute",1);
    mute_query.exec();

    p->mutes.insert(id,true);
}

void UserData::removeMute(int id)
{
    QSqlQuery query(p->db);
    query.prepare("DELETE FROM mutes WHERE id=:id");
    query.bindValue(":id", id);
    query.exec();

    p->mutes.remove(id);
}

QList<int> UserData::mutes() const
{
    QList<int> res;
    QHashIterator<int,bool> i(p->mutes);
    while( i.hasNext() )
    {
        i.next();
        if( i.value() )
            res << i.key();
    }

    return res;
}

bool UserData::isMuted(int id)
{
    return p->mutes.value(id);
}

void UserData::addFavorite(int id)
{
    QSqlQuery mute_query(p->db);
    mute_query.prepare("INSERT OR REPLACE INTO favorites (id,favorite) VALUES (:id,:fave)");
    mute_query.bindValue(":id",id);
    mute_query.bindValue(":fave",1);
    mute_query.exec();

    p->favorites.insert(id,true);
}

void UserData::removeFavorite(int id)
{
    QSqlQuery query(p->db);
    query.prepare("DELETE FROM favorites WHERE id=:id");
    query.bindValue(":id", id);
    query.exec();

    p->favorites.remove(id);
}

QList<int> UserData::favorites() const
{
    QList<int> res;
    QHashIterator<int,bool> i(p->favorites);
    while( i.hasNext() )
    {
        i.next();
        if( i.value() )
            res << i.key();
    }

    return res;
}

bool UserData::isFavorited(int id)
{
    return p->favorites.value(id);
}

void UserData::addSecretChat(int id, int userId, const QString &title)
{
    QSqlQuery mute_query(p->db);
    mute_query.prepare("INSERT OR REPLACE INTO SecretChats (id,userId,title) VALUES (:id,:uid,:ttl)");
    mute_query.bindValue(":id" ,id);
    mute_query.bindValue(":uid",userId);
    mute_query.bindValue(":ttl",title);
    mute_query.exec();

    SecretChatDBClass sc;
    sc.id = id;
    sc.userId = userId;
    sc.title = title;

    p->secretChats[id] = sc;
}

void UserData::removeSecretChat(int id)
{
    QSqlQuery query(p->db);
    query.prepare("DELETE FROM SecretChats WHERE id=:id");
    query.bindValue(":id", id);
    query.exec();

    p->secretChats.remove(id);
}

int UserData::secretChatUserId(int id)
{
    return p->secretChats[id].userId;
}

QString UserData::secretChatTitle(int id)
{
    return p->secretChats[id].title;
}

QList<int> UserData::secretChats()
{
    return p->secretChats.keys();
}

void UserData::init_buffer()
{
    QSqlQuery user_query(p->db);
    user_query.prepare("SELECT id, isChat, unread, ldate, lastMsg, photo, title, firstname, lastname, phone FROM dialogs");
    user_query.exec();

    while( user_query.next() )
    {
        const QSqlRecord & record = user_query.record();
        for( int i=0; i<record.count(); i++ )
        {
            qint64 id = record.value(0).toLongLong();
            int is_chat = record.value(1).toInt();
            int unread = record.value(2).toInt();
            qint64 ldate = record.value(3).toInt();
            QString lastMsg = record.value(4).toString();
            QString photo = record.value(5).toString();
            QString title = record.value(6).toString();
            QString fname = record.value(7).toString();
            QString lname = record.value(8).toString();
            QString phone = record.value(9).toString();

            UserClass user;
            user.username = title;
            user.firstname = fname;
            user.lastname = lname;
            user.phone = phone;
            user.state = Enums::Offline;

            p->contacts.insert( id, user );

            if( is_chat == 0 )
            {
                DialogClass dialog;
                dialog.is_chat = false;
                dialog.msgLast = lastMsg;
                dialog.unread = unread;
                dialog.msgDate = QDateTime::fromMSecsSinceEpoch(ldate);
                dialog.userClass = user;

                p->dialogs.insert( id, dialog );
            }
            else
            if( is_chat == 1 )
            {
                DialogClass dialog;
                dialog.is_chat = true;
                dialog.msgLast = lastMsg;
                dialog.unread = unread;
                dialog.msgDate = QDateTime::fromMSecsSinceEpoch(ldate);
                dialog.chatClass.chat_id = id;
                dialog.chatClass.title = title;

                p->dialogs.insert( id, dialog );
            }

            p->photos.insert( id, photo );
        }
    }

    QSqlQuery mute_query(p->db);
    mute_query.prepare("SELECT id, mute FROM mutes");
    mute_query.exec();

    while( mute_query.next() )
    {
        const QSqlRecord & record = mute_query.record();
        p->mutes.insert( record.value(0).toInt(), record.value(1).toInt() );
    }

    QSqlQuery faves_query(p->db);
    faves_query.prepare("SELECT id, favorite FROM favorites");
    faves_query.exec();

    while( faves_query.next() )
    {
        const QSqlRecord & record = faves_query.record();
        p->favorites.insert( record.value(0).toInt(), record.value(1).toInt() );
    }

    QSqlQuery schat_query(p->db);
    schat_query.prepare("SELECT id, userId, title FROM secretChats");
    schat_query.exec();

    while( schat_query.next() )
    {
        const QSqlRecord & record = schat_query.record();
        SecretChatDBClass sc;
        sc.id = record.value(0).toInt();
        sc.userId = record.value(1).toInt();
        sc.title = record.value(2).toString();

        p->secretChats[sc.id] = sc;
    }
}

void UserData::update_db()
{
    QSqlQuery version_query(p->db);
    version_query.prepare("SELECT gvalue FROM general WHERE gkey=:gkey");
    version_query.bindValue(":gkey","version");
    version_query.exec();

    QString version = 0;
    if( version_query.next() )
    {
        const QSqlRecord & record = version_query.record();
        version = record.value(0).toString();
    }

    if( version < "1" )
    {
        QSqlQuery query(p->db);
        query.prepare("CREATE TABLE IF NOT EXISTS General ("
                      "gkey TEXT NOT NULL,"
                      "gvalue TEXT NOT NULL,"
                      "PRIMARY KEY (gkey));");
        query.exec();
        query.prepare("CREATE TABLE IF NOT EXISTS SecretChats ("
                      "id BIGINT NOT NULL,"
                      "userId BIGINT NOT NULL DEFAULT 0,"
                      "title TEXT NOT NULL,"
                      "PRIMARY KEY (id));");
        query.exec();
        query.prepare("INSERT OR REPLACE INTO General (gkey,gvalue) VALUES (\"version\",\"1\")");
        query.exec();
    }
    if( version < "2" )
    {
        QSqlQuery query(p->db);
        query.prepare("CREATE TABLE IF NOT EXISTS Favorites ("
                      "id BIGINT NOT NULL,"
                      "favorite TINYINT(1) NOT NULL DEFAULT 0,"
                      "PRIMARY KEY (id));");
        query.exec();
        query.prepare("INSERT OR REPLACE INTO General (gkey,gvalue) VALUES (\"version\",\"2\")");
        query.exec();
    }
}

UserData::~UserData()
{
    delete p;
}
