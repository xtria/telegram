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

#define NOTIFY_ACT_SHOW 0
#define NOTIFY_ACT_MUTE 1
#define NOTIFY_ACT_RMND 2

#define UNITY_ICON_PATH(NUM) "/tmp/sialan-telegram-client-trayicon" + QString::number(NUM) + ".png"

#include "telegramgui.h"
#include "unitysystemtray.h"
#include "telegram_macros.h"
#include "versionchecker.h"
#include "hashobject.h"
#include "qmlstaticobjecthandler.h"
#include "emojis.h"
#include "countries.h"
#include "setobject.h"
#include "userdata.h"
#include "telegram/telegram.h"
#ifdef Q_OS_LINUX
#include "notification.h"
#endif

#include <QtQml>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QVariant>
#include <QApplication>
#include <QTextDocument>
#include <QClipboard>
#include <QMimeData>
#include <QDebug>
#include <QHash>
#include <QFileDialog>
#include <QFile>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QImageReader>
#include <QScreen>
#include <QAction>
#include <QPointer>
#include <QSettings>
#include <QQuickItem>
#include <QDesktopServices>
#include <QQuickWindow>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QImageWriter>
#include <QMimeDatabase>
#include <QMimeType>

QPointer<QSettings> tg_settings;

class TelegramGuiPrivate
{
public:
#ifdef Q_OS_LINUX
    Notification *notify;
#endif

    UserData *userdata;
    Emojis *emojis;
    Countries *countries;

    QTextDocument *doc;

    QSystemTrayIcon *sysTray;
    UnitySystemTray *unityTray;
    VersionChecker *version_checker;

    QQmlApplicationEngine *engine;
    Telegram *tg;

    char *args;

    QQuickWindow *root;

    qreal chatListWidth;
    QString background;
    bool mute_all;
    bool firstTime;
    int love;

    int width;
    int height;
    bool visible;

    QTranslator *translator;
    QHash<QString,QVariant> languages;
    QHash<QString,QLocale> locales;
    QString language;

    QMimeDatabase mime_db;

    bool quit_state;
};

TelegramGui::TelegramGui(QObject *parent) :
    QObject(parent)
{
    p = new TelegramGuiPrivate;
    p->engine = 0;
    p->tg = 0;
    p->quit_state = false;
    p->args = QGuiApplication::arguments().first().toUtf8().data();
    p->doc = new QTextDocument(this);
    p->translator = new QTranslator(this);
    p->sysTray = 0;
    p->unityTray = 0;

    QDir().mkpath(HOME_PATH);
    QDir().mkpath(HOME_PATH + "/downloads");
    if( !tg_settings )
        tg_settings = new QSettings( HOME_PATH + "/telegram.conf", QSettings::IniFormat, this);

    p->background = tg_settings->value( "General/background", QString() ).toString();
    p->chatListWidth = tg_settings->value( "General/chatListWidth", 250 ).toDouble();
    p->firstTime = tg_settings->value( "General/firstTime", true ).toBool();
    p->width = tg_settings->value( "General/width", 1024 ).toInt();
    p->height = tg_settings->value( "General/height", 600 ).toInt();
    p->visible = tg_settings->value( "General/visible", true ).toBool();
    p->mute_all = tg_settings->value( "General/muteAll", false ).toBool();
    p->love = tg_settings->value( "General/love", 0 ).toInt();

#ifdef Q_OS_LINUX
    p->notify = new Notification(this);
    connect( p->notify, SIGNAL(notifyAction(uint,QString)), SLOT(notify_action(uint,QString)) );
#endif

    qmlRegisterType<Enums>("org.sialan.telegram", 1, 0, "Enums");
    qmlRegisterType<SetObject>("org.sialan.telegram", 1, 0, "SetObject");
    qmlRegisterType<HashObject>("org.sialan.telegram", 1, 0, "HashObject");
    qmlRegisterType<QmlStaticObjectHandler>("org.sialan.telegram", 1, 0, "StaticObjectHandler");

    init_languages();
    check_files();
}

QSettings *TelegramGui::settings()
{
    return tg_settings;
}

void TelegramGui::setMute(int id, bool stt)
{
    if( p->userdata->isMuted(id) == stt )
        return;

    if( stt )
        p->userdata->addMute(id);
    else
        p->userdata->removeMute(id);

    emit muted(id, stt);
}

bool TelegramGui::isMuted(int id) const
{
    return p->userdata->isMuted(id);
}

void TelegramGui::setFavorite(int id, bool stt)
{
    if( p->userdata->isFavorited(id) == stt )
        return;

    if( stt )
        p->userdata->addFavorite(id);
    else
        p->userdata->removeFavorite(id);

    emit favorited(id, stt);
}

bool TelegramGui::isFavorited(int id) const
{
    return p->userdata->isFavorited(id);
}

QString TelegramGui::aboutSialanStr() const
{
    return tr("Sialan Labs is a not-for-profit research and software development team launched in February 2014 focusing on development of products, technologies and solutions in order to publish them as open-source projects accessible to all people in the universe. Currently, we are focusing on design and development of software applications and tools which have direct connection with end users.") + "\n\n" +
            tr("By enabling innovative projects and distributing software to millions of users globally, the lab is working to accelerate the growth of high-impact open source software projects and promote an open source culture of accessibility and increased productivity around the world. The lab partners with industry leaders and policy makers to bring open source technologies to new sectors, including education, health and government.");
}

QString TelegramGui::version() const
{
    return VERSION;
}

QSize TelegramGui::screenSize() const
{
    const QList<QScreen*> & list = QGuiApplication::screens();
    if( list.isEmpty() )
        return QSize();

    return list.first()->size();
}

QPoint TelegramGui::mousePos() const
{
    return QCursor::pos();
}

QPoint TelegramGui::mapToGlobal(QQuickItem *item, const QPoint &pnt)
{
    return item->window()->mapToGlobal( item->mapToScene(pnt).toPoint() );
}

QPoint TelegramGui::mapToScene(QQuickItem *item, const QPoint &pnt)
{
    return item->mapToScene(pnt).toPoint();
}

QString TelegramGui::appPath() const
{
    return QCoreApplication::applicationDirPath() + "/";
}

QStringList TelegramGui::fonts() const
{
    return fontsOf(TelegramGui::resourcesPath() + "/fonts");
}

QStringList TelegramGui::fontsOf(const QString &path) const
{
    QStringList res;
    const QStringList & list = QDir(path).entryList(QStringList() << "*.ttf", QDir::Files);
    foreach( const QString & f, list )
        res << path + "/" + f;

    const QStringList & dirs = QDir(path).entryList(QDir::Dirs|QDir::NoDotAndDotDot);
    foreach( const QString & d, dirs )
        res << fontsOf( path + "/" + d );

    return res;
}

void TelegramGui::setBackground(const QString &path)
{
    if( path == p->background )
        return;

    p->background = path;
    tg_settings->setValue( "General/background", p->background );
    emit backgroundChanged();
}

QString TelegramGui::background() const
{
    return p->background;
}

void TelegramGui::setChatListWidth(qreal w)
{
    if( w == p->chatListWidth )
        return;

    p->chatListWidth = w;
    tg_settings->setValue( "General/chatListWidth", p->chatListWidth );
    emit chatListWidthChanged();
}

qreal TelegramGui::chatListWidth() const
{
    return p->chatListWidth;
}

void TelegramGui::setLove(int uid)
{
    if( uid == p->love )
        return;

    p->love = uid;
    tg_settings->setValue( "General/love", p->love );
    emit loveChanged();
}

int TelegramGui::love() const
{
    return p->love;
}

void TelegramGui::setMuteAll(bool state)
{
    if( state == p->mute_all )
        return;

    p->mute_all = state;
    tg_settings->setValue( "General/muteAll", p->mute_all );
    emit muteAllChanged();
}

bool TelegramGui::muteAll() const
{
    return p->mute_all;
}

void TelegramGui::setFirstTime(bool stt)
{
    if( stt == p->firstTime )
        return;

    p->firstTime = stt;
    tg_settings->setValue( "General/firstTime", p->firstTime );
    emit firstTimeChanged();
}

bool TelegramGui::firstTime() const
{
    return p->firstTime;
}

QString TelegramGui::getOpenFile()
{
    return QFileDialog::getOpenFileName();
}

QSize TelegramGui::imageSize(const QString &path)
{
    QImageReader img(path);
    return img.size();
}

qreal TelegramGui::htmlWidth(const QString &txt)
{
    p->doc->setHtml(txt);
    return p->doc->size().width() + 10;
}

QString TelegramGui::license() const
{
    QString res;

    QFile license_file( QCoreApplication::applicationDirPath() + "/license.txt" );
    if( !license_file.open(QFile::ReadOnly) )
        return res;

    res = license_file.readAll();
    return res;
}

QString TelegramGui::fixPhoneNumber(QString number)
{
    if( number.isEmpty() )
        return number;
    if( number[0] == '0' )
    {
        number.remove(0,1);
        number = p->countries->phoneCode(country()) + number;
    }
    if( number[0] != '+' )
        number = "+" + number;

    return number;
}

int TelegramGui::desktopSession()
{
    static int result = -1;
    if( result != -1 )
        return result;

#ifdef Q_OS_MAC
    result = Enums::Mac;
#else
#ifdef Q_OS_WIN
    result = Enums::Windows;
#else
    static QString *desktop_session = 0;
    if( !desktop_session )
        desktop_session = new QString( qgetenv("DESKTOP_SESSION") );

    if( desktop_session->contains("kde",Qt::CaseInsensitive) )
        result = Enums::Kde;
    else
    if( desktop_session->contains("ubuntu",Qt::CaseInsensitive) )
        result = Enums::Unity;
    else
    if( desktop_session->contains("gnome-fallback",Qt::CaseInsensitive) )
        result = Enums::GnomeFallBack;
    else
        result = Enums::Gnome;
#endif
#endif

    if( result == -1 )
        result = Enums::Unknown;

    return result;
}

const QString & TelegramGui::resourcesPath()
{
    static QString *result;
    if( !result )
    {
        result = new QString();

#ifdef Q_OS_MAC
        *result = QFileInfo(QCoreApplication::applicationDirPath() + "/../Resources/").filePath();
#else
        QString res = QCoreApplication::applicationDirPath();
        if( !QFile::exists(res) )
            res = QCoreApplication::applicationDirPath() + "../share/sigram";

        *result = QFileInfo(res).filePath();
#endif
    }

    return *result;
}

void TelegramGui::setWidth(int w)
{
    if( w == p->width )
        return;

    p->width = w;
    if( p->quit_state )
        return;

    tg_settings->setValue( "General/width", w );

    emit widthChanged();
}

int TelegramGui::width() const
{
    return p->width;
}

void TelegramGui::setHeight(int h)
{
    if( h == p->height )
        return;

    p->height = h;
    if( p->quit_state )
        return;

    tg_settings->setValue( "General/height", h );

    emit heightChanged();
}

int TelegramGui::height() const
{
    return p->height;
}

void TelegramGui::setVisible(bool v)
{
    if( v == p->visible )
        return;

    p->visible = v;
    if( p->quit_state )
        return;

    tg_settings->setValue( "General/visible", v );

    emit visibleChanged();
}

bool TelegramGui::visible() const
{
    return p->visible;
}

void TelegramGui::setCountry(const QString &cnt)
{
    if( country() == cnt.toLower() )
        return;

    tg_settings->setValue( "General/country", cnt.toLower() );
}

QString TelegramGui::country()
{
    return tg_settings->value( "General/country", QString() ).toString();
}

void TelegramGui::setLanguage(const QString &lang)
{
    if( language() == lang )
        return;

    QGuiApplication::removeTranslator(p->translator);
    p->translator->load(p->languages.value(lang).toString(),"languages");
    QGuiApplication::installTranslator(p->translator);
    p->language = lang;

    tg_settings->setValue( "General/language", lang );
    emit languageChanged();
}

QString TelegramGui::language() const
{
    return p->language;
}

QStringList TelegramGui::languages() const
{
    QStringList res = p->languages.keys();
    res.sort();
    return res;
}

Qt::LayoutDirection TelegramGui::directionOf(const QString &str)
{
    Qt::LayoutDirection res = Qt::LeftToRight;
    if( str.isEmpty() )
        return res;

    int ltr = 0;
    int rtl = 0;

    foreach( const QChar & ch, str )
    {
        QChar::Direction dir = ch.direction();
        switch( static_cast<int>(dir) )
        {
        case QChar::DirL:
        case QChar::DirLRE:
        case QChar::DirLRO:
        case QChar::DirEN:
            ltr++;
            break;

        case QChar::DirR:
        case QChar::DirRLE:
        case QChar::DirRLO:
        case QChar::DirAL:
            rtl++;
            break;
        }
    }

    if( ltr >= rtl )
        res = Qt::LeftToRight;
    else
        res = Qt::RightToLeft;

    return res;
}

void TelegramGui::setDonate(bool stt)
{
    if( donate() == stt )
        return;

    tg_settings->setValue( "General/donate", stt );
}

bool TelegramGui::donate()
{
    return tg_settings->value( "General/donate", false ).toBool();
}

void TelegramGui::setDonateViewShowed(bool stt)
{
    if( donateViewShowed() == stt )
        return;

    tg_settings->setValue( "General/donateViewShowed", stt );
}

bool TelegramGui::donateViewShowed()
{
    return tg_settings->value( "General/donateViewShowed", false ).toBool();
}

void TelegramGui::start()
{
    if( p->engine )
        return;

    p->tg = new Telegram(1,&(p->args));
    p->userdata = new UserData(this);
    p->emojis = new Emojis(this);
    p->version_checker = new VersionChecker(this);
    p->countries = new Countries(this);

    p->engine = new QQmlApplicationEngine(this);
    p->engine->rootContext()->setContextProperty( "Telegram", p->tg );
    p->engine->rootContext()->setContextProperty( "Gui", this );
    p->engine->rootContext()->setContextProperty( "UserData", p->userdata );
    p->engine->rootContext()->setContextProperty( "Emojis", p->emojis );
    p->engine->rootContext()->setContextProperty( "VersionChecker", p->version_checker );
    p->engine->rootContext()->setContextProperty( "Countries", p->countries );
    p->engine->load(QUrl(QStringLiteral("qrc:///main.qml")));

    p->root = static_cast<QQuickWindow*>(p->engine->rootObjects().first());

    if( desktopSession() == Enums::Unity || desktopSession() == Enums::GnomeFallBack )
    {
        QFile::copy(":/files/sys_tray.png",UNITY_ICON_PATH(0));

        p->unityTray = new UnitySystemTray( QCoreApplication::applicationName(), UNITY_ICON_PATH(0) );
        if( !p->unityTray->pntr() )
            QApplication::setQuitOnLastWindowClosed(true);
        p->unityTray->addMenu( tr("Show"), this, "show" );
        p->unityTray->addMenu( tr("Configure"), this, "configure" );
        p->unityTray->addMenu( tr("About"), this, "about" );
        p->unityTray->addMenu( tr("About Sialan"), this, "aboutSialan" );
        p->unityTray->addMenu( tr("License"), this, "showLicense" );
        p->unityTray->addMenu( tr("Donate"), this, "showDonate" );
        p->unityTray->addMenu( tr("Quit"), this, "quit" );
    }
    if( !p->unityTray || !p->unityTray->pntr() )
    {
        p->sysTray = new QSystemTrayIcon( QIcon(":/files/sys_tray.png"), this );
        p->sysTray->show();

        connect( p->sysTray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(systray_action(QSystemTrayIcon::ActivationReason)) );
    }
}

void TelegramGui::sendNotify(quint64 msg_id)
{
    if( p->mute_all )
        return;

    QStringList actions;
    if( desktopSession() != Enums::Unity )
    {
        actions << QString("%1:%2").arg(NOTIFY_ACT_SHOW).arg(msg_id) << tr("Show");
        actions << QString("%1:%2").arg(NOTIFY_ACT_MUTE).arg(msg_id) << tr("Mute");
//        actions << QString("%1:%2").arg(NOTIFY_ACT_RMND).arg(msg_id) << tr("Mute & Remind");
    }

    int to_id = p->tg->messageToId(msg_id);
    int from_id = p->tg->messageFromId(msg_id);
    if( from_id == p->tg->me() )
        return;
    if( isMuted(to_id) || isMuted(from_id) )
        return;

    QString title = p->tg->messageFromName(msg_id);
    QString icon = p->tg->getPhotoPath(from_id);
    QString body = p->tg->messageBody(msg_id);
    if( p->tg->dialogIsChat(to_id) )
        title += tr("at %1").arg(p->tg->dialogTitle(to_id));
    else
    if( p->tg->dialogIsChat(from_id) )
        title += tr("at %1").arg(p->tg->dialogTitle(from_id));

#ifdef Q_OS_LINUX
    p->notify->sendNotify( title, body, icon, 0, 3000, actions );
#endif
}

void TelegramGui::openFile(const QString &file)
{
    QDesktopServices::openUrl( QUrl(file) );
}

void TelegramGui::openUrl(const QUrl &url)
{
    QDesktopServices::openUrl( url );
}

void TelegramGui::copyFile(const QString &file)
{
    QStringList paths;
    paths << file;

    QList<QUrl> urls;
    QString data = "copy";

    foreach( const QString & p, paths ) {
        QUrl url = QUrl::fromLocalFile(p);
        urls << url;
        data += "\nfile://" + url.toLocalFile();
    }

    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls( urls );
    mimeData->setData( "x-special/gnome-copied-files", data.toUtf8() );

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void TelegramGui::saveFile(const QString &file)
{
    const QString & path = QFileDialog::getSaveFileName();
    if( path.isEmpty() )
        return;

    QFile::copy( file, path );
}

void TelegramGui::copyText(const QString &txt)
{
    QGuiApplication::clipboard()->setText( txt );
}

int TelegramGui::showMenu(const QStringList &list)
{
    QMenu menu;

    QList<QAction*> actions;
    foreach( const QString & l, list )
        actions << (l.isEmpty()? menu.addSeparator() : menu.addAction(l));

    menu.move( QCursor::pos() );
    QAction *res = menu.exec();

    return actions.indexOf(res);
}

void TelegramGui::notify_action(uint id, const QString &act)
{
    Q_UNUSED(id)
    const QStringList & splits = act.split(":");
    if( splits.count() != 2 )
        return;

    uint act_id = splits.at(0).toUInt();
    quint64 msg_id = splits.at(1).toULongLong();

    int to_id = p->tg->messageToId(msg_id);
    int from_id = p->tg->messageFromId(msg_id);
    int current = from_id;
    if( p->tg->dialogIsChat(to_id) )
        current = to_id;

    switch (act_id) {
    case NOTIFY_ACT_SHOW:
        p->root->setVisible( true );
        p->root->setProperty( "current", current );
        p->root->requestActivate();
#ifdef Q_OS_LINUX
        p->notify->closeNotification(id);
#endif
        break;

    case NOTIFY_ACT_MUTE:
        setMute( current, true );
#ifdef Q_OS_LINUX
        p->notify->closeNotification(id);
#endif
        break;

    case NOTIFY_ACT_RMND:
        break;
    }
}

void TelegramGui::systray_action(QSystemTrayIcon::ActivationReason act)
{
    switch( static_cast<int>(act) )
    {
    case QSystemTrayIcon::Trigger:
        if( p->root->isVisible() && p->root->isActive() )
            p->root->hide();
        else
        {
            p->root->setVisible( true );
            p->root->requestActivate();
        }
        break;

    case QSystemTrayIcon::Context:
        showContextMenu();
        break;
    }
}

QImage TelegramGui::generateIcon(const QImage &img, int count)
{
    QImage res = img;
    if( count == 0 )
        return img;

    QRect rct;
    rct.setX( img.width()/5 );
    rct.setWidth( 4*img.width()/5 );
    rct.setY( img.height()-rct.width() );
    rct.setHeight( rct.width() );

    QPainterPath path;
    path.addEllipse(rct);

    QPainter painter(&res);
    painter.setRenderHint( QPainter::Antialiasing , true );
    painter.fillPath( path, QColor("#ff0000") );
    painter.setPen("#333333");
    painter.drawPath( path );
    painter.setPen("#ffffff");
    painter.drawText( rct, Qt::AlignCenter | Qt::AlignHCenter, QString::number(count) );

    return res;
}

void TelegramGui::showContextMenu()
{
    QMenu menu;
    menu.move( QCursor::pos() );

    QAction *show_act = menu.addAction( tr("Show") );
    menu.addSeparator();
    QAction *conf_act = menu.addAction( tr("Configure") );
    QAction *abut_act = menu.addAction( tr("About") );
    QAction *sabt_act = menu.addAction( tr("About Sialan") );
    menu.addSeparator();
    QAction *lcns_act = menu.addAction( tr("License") );
    QAction *dnt_act  = menu.addAction( tr("Donate") );
    menu.addSeparator();
    QAction *exit_act = menu.addAction( tr("Exit") );
    QAction *res_act  = menu.exec();

    if( res_act == show_act )
        show();
    else
    if( res_act == conf_act )
        configure();
    else
    if( res_act == abut_act )
        about();
    else
    if( res_act == sabt_act )
        aboutSialan();
    else
    if( res_act == lcns_act )
        showLicense();
    else
    if( res_act == dnt_act && dnt_act != 0 )
        showDonate();
    else
    if( res_act == exit_act )
        quit();
}

void TelegramGui::configure()
{
    p->root->setVisible( true );
    p->root->requestActivate();
    QMetaObject::invokeMethod( p->root, "showMyConfigure" );
}

void TelegramGui::about()
{
    p->root->setVisible( true );
    p->root->requestActivate();
    p->root->setProperty( "about", !p->root->property("about").toBool() );
    p->root->setProperty( "focus", true );
}

void TelegramGui::aboutSialan()
{
    p->root->setVisible( true );
    p->root->requestActivate();
    p->root->setProperty( "aboutSialan", !p->root->property("aboutSialan").toBool() );
    p->root->setProperty( "focus", true );
}

void TelegramGui::showLicense()
{
    p->root->setVisible( true );
    p->root->requestActivate();
    QMetaObject::invokeMethod( p->root, "showLicense" );
}

void TelegramGui::showDonate()
{
    p->root->setVisible( true );
    p->root->requestActivate();
    QMetaObject::invokeMethod( p->root, "showDonate" );
}

void TelegramGui::quit()
{
    p->quit_state = true;

    connect( p->tg, SIGNAL(myStatusUpdated()), QCoreApplication::instance(), SLOT(quit()) );
    p->tg->setStatusOnline(false);

    QTimer::singleShot(3000, QCoreApplication::instance(), SLOT(quit()));
}

void TelegramGui::show()
{
    p->root->setVisible( true );
    p->root->setProperty( "aboutSialan", false );
    p->root->setProperty( "about", false );
    p->root->requestActivate();
}

void TelegramGui::logout()
{
    QStringList files = QDir(HOME_PATH).entryList(QDir::Files);
    foreach( const QString & f, files )
        QFile::remove(HOME_PATH+"/"+f);

    QProcess::startDetached( QCoreApplication::applicationFilePath() );
    quit();
}

void TelegramGui::setSysTrayCounter(int count)
{
    const QImage & img = generateIcon( QImage(":/files/sys_tray.png"), count );
    if( p->sysTray )
    {
        p->sysTray->setIcon( QPixmap::fromImage(img) );
    }
    else
    if( p->unityTray )
    {
        QString path = UNITY_ICON_PATH(count);
        QFile::remove(path);
        QImageWriter writer(path);
        writer.write(img);
        p->unityTray->setIcon(path);
    }
}

void TelegramGui::incomingAppMessage(const QString &msg)
{
    if( msg == "show" )
        show();
}

QVariant TelegramGui::call(QObject *obj, const QString &member, const QVariant &v0, const QVariant &v1, const QVariant &v2, const QVariant &v3, const QVariant &v4, const QVariant &v5, const QVariant &v6, const QVariant &v7, const QVariant &v8, const QVariant &v9)
{
    const QMetaObject *meta_obj = obj->metaObject();
    QMetaMethod meta_method;
    for( int i=0; i<meta_obj->methodCount(); i++ )
    {
        QMetaMethod mtd = meta_obj->method(i);
        if( mtd.name() == member )
            meta_method = mtd;
    }
    if( !meta_method.isValid() )
        return QVariant();

    QList<QByteArray> param_types = meta_method.parameterTypes();
    QList<QByteArray> param_names = meta_method.parameterNames();

    QString ret_type = meta_method.typeName();
    QList< QPair<QString,QString> > m_args;
    for( int i=0 ; i<param_types.count() ; i++ )
        m_args << QPair<QString,QString>( param_types.at(i) , param_names.at(i) );

    QVariantList vals;
        vals << v0 << v1 << v2 << v3 << v4 << v5 << v6 << v7 << v8 << v9;

    QVariantList tr_vals;

    QList< QPair<QString,const void*> > args;
    for( int i=0 ; i<vals.count() ; i++ )
    {
        if( i<m_args.count() )
        {
            QString type = m_args.at(i).first;

            if( type != vals.at(i).typeName() )
            {
                if( !vals[i].canConvert( QVariant::nameToType(type.toLatin1()) ) )
                    ;
                else
                    vals[i].convert( QVariant::nameToType(type.toLatin1()) );
            }

            args << QPair<QString,const void*>( type.toLatin1() , vals.at(i).data() );
            tr_vals << vals[i];
        }
        else
        {
            args << QPair<QString,const void*>( vals.at(i).typeName() , vals.at(i).data() );
        }
    }

    int type = QMetaType::type(ret_type.toLatin1());
    void *res = QMetaType::create( type );
    bool is_pointer = ret_type.contains('*');

    bool done = QMetaObject::invokeMethod( obj , member.toLatin1() , QGenericReturnArgument( ret_type.toLatin1() , (is_pointer)? &res : res ) ,
                                      QGenericArgument( args.at(0).first.toLatin1() , args.at(0).second ) ,
                                      QGenericArgument( args.at(1).first.toLatin1() , args.at(1).second ) ,
                                      QGenericArgument( args.at(2).first.toLatin1() , args.at(2).second ) ,
                                      QGenericArgument( args.at(3).first.toLatin1() , args.at(3).second ) ,
                                      QGenericArgument( args.at(4).first.toLatin1() , args.at(4).second ) ,
                                      QGenericArgument( args.at(5).first.toLatin1() , args.at(5).second ) ,
                                      QGenericArgument( args.at(6).first.toLatin1() , args.at(6).second ) ,
                                      QGenericArgument( args.at(7).first.toLatin1() , args.at(7).second ) ,
                                      QGenericArgument( args.at(8).first.toLatin1() , args.at(8).second ) ,
                                      QGenericArgument( args.at(9).first.toLatin1() , args.at(9).second ) );

    Q_UNUSED(done)

    QVariant result;
    if( type == QMetaType::Void )
        result = QVariant();
    else
    if( is_pointer )
        result = QVariant( type , &res );
    else
        result = QVariant( type , res );

    if( type == QMetaType::type("QVariant") )
        return result.value<QVariant>();
    else
        return result;
}

void TelegramGui::init_languages()
{
    QDir dir(LOCALES_PATH);
    QStringList languages = dir.entryList( QDir::Files );
    if( !languages.contains("lang-en.qm") )
        languages.prepend("lang-en.qm");

    for( int i=0 ; i<languages.size() ; i++ )
     {
         QString locale_str = languages[i];
             locale_str.truncate( locale_str.lastIndexOf('.') );
             locale_str.remove( 0, locale_str.indexOf('-') + 1 );

         QLocale locale(locale_str);

         QString  lang = QLocale::languageToString(locale.language());
         QVariant data = LOCALES_PATH + "/" + languages[i];

         p->languages.insert( lang, data );
         p->locales.insert( lang , locale );

         if( lang == tg_settings->value("General/language","English").toString() )
             setLanguage( lang );
    }
}

void TelegramGui::check_files()
{
    const QString & folder = HOME_PATH + "/downloads";
    const QStringList & files = QDir(folder).entryList(QDir::Files);
    foreach( const QString & f, files )
    {
        const QMimeType & t = p->mime_db.mimeTypeForFile(folder + "/" + f);
        const QString & tname = t.name();
        if( !tname.contains("audio",Qt::CaseInsensitive) &&
            !tname.contains("video",Qt::CaseInsensitive) &&
            !tname.contains("image",Qt::CaseInsensitive) )
            QFile::remove(folder + "/" + f);
    }
}

TelegramGui::~TelegramGui()
{
    delete p;
}
