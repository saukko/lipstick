/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#include "homeapplication.h"
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QTimer>
#include <QDesktopWidget>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QIcon>
#include <QX11Info>
#include <QDebug>
#include <QEvent>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>

#include "xtools/xeventlistener.h"
#include "xtools/xatomcache.h"
#include "xtools/xwindowmanager.h"
#include "xtools/homewindowmonitor.h"
#include "notifications/notificationmanager.h"
#include "notifications/notificationpreviewpresenter.h"
#include "components/windowmanager.h"
#include "components/windowinfo.h"
#include "lipsticksettings.h"
#include "lipstickdbusinterface.h"

// Define this if you'd like to see debug messages from the home app
#ifdef DEBUG_HOME
#define HOME_DEBUG(things) qDebug() << Q_FUNC_INFO << things
#else
#define HOME_DEBUG(things)
#endif

void HomeApplication::quitSignalHandler(int)
{
    qApp->quit();
}

HomeApplication::HomeApplication(int &argc, char **argv, const QString &qmlPath)
    : QApplication(argc, argv)
    , xEventListeners()
    , toBeRemovedEventListeners()
    , iteratorActiveForEventListenerContainer(false)
    , xDamageEventBase(0)
    , xDamageErrorBase(0)
    , _mainWindowInstance(0)
    , _qmlPath(qmlPath)
    , originalSigIntHandler(signal(SIGINT, quitSignalHandler))
    , originalSigTermHandler(signal(SIGTERM, quitSignalHandler))
{
    setApplicationName("Lipstick");
    // TODO: autogenerate this from tags
    setApplicationVersion(VERSION);

    XDamageQueryExtension(QX11Info::display(), &xDamageEventBase, &xDamageErrorBase);

    // launch a timer for sending a dbus-signal upstart when basic construct is done
    QTimer::singleShot(0, this, SLOT(sendStartupNotifications()));

    // Initialize the notification manager;
    NotificationManager::instance();
    new NotificationPreviewPresenter(this);

    // Initialize the home window monitor
    HomeWindowMonitor::instance();

    new LipstickDBusInterface(this);
    QDBusConnection::sessionBus().registerService("org.nemomobile.lipstick");
    if (!QDBusConnection::sessionBus().registerObject("/request", this))
        qWarning("lipstick: CAN'T REGISTER DBUS");
}

HomeApplication::~HomeApplication()
{
    delete _mainWindowInstance;

    signal(SIGINT, originalSigIntHandler);
    signal(SIGTERM, originalSigTermHandler);
}

HomeApplication *HomeApplication::instance()
{
    return qobject_cast<HomeApplication *>(QApplication::instance());
}

void HomeApplication::addXEventListener(XEventListener *listener)
{
    if (listener != NULL && !xEventListeners.contains(listener)) {
        xEventListeners.append(listener);
    }
}

void HomeApplication::removeXEventListener(XEventListener *listener)
{
    if (iteratorActiveForEventListenerContainer) {
        toBeRemovedEventListeners.append(listener);
    } else {
        xEventListeners.removeOne(listener);
    }
}

void HomeApplication::sendStartupNotifications()
{
    static QDBusConnection systemBus = QDBusConnection::systemBus();
    QDBusMessage homeReadySignal =
        QDBusMessage::createSignal("/com/nokia/duihome",
                                   "com.nokia.duihome.readyNotifier",
                                   "ready");
    systemBus.send(homeReadySignal);

    // For device boot performance reasons initializing Home scene window must be done
    // only after ready signal is sent.
    mainWindowInstance()->showFullScreen();

    // Visibility change messages are required to make the appVisible() signal work
    XWindowAttributes attributes;
    XGetWindowAttributes(QX11Info::display(), _mainWindowInstance->winId(), &attributes);
    XSelectInput(QX11Info::display(), _mainWindowInstance->winId(), attributes.your_event_mask | VisibilityChangeMask);

    // Excluding it from the task bar
    XWindowManager::excludeFromTaskBar(_mainWindowInstance->winId());

    // Tell X that changes in the properties and the substructure of the root
    // window are interesting. These are used to get the list of windows and
    // for getting window close events.
    XSelectInput(QX11Info::display(), DefaultRootWindow(QX11Info::display()), PropertyChangeMask | SubstructureNotifyMask);
}

bool HomeApplication::x11EventFilter(XEvent *event)
{
    bool eventHandled = false;
    iteratorActiveForEventListenerContainer = true;

    if (event->type == xDamageEventBase + XDamageNotify) {
        HOME_DEBUG("Processing damage event");
        XDamageNotifyEvent *xevent = (XDamageNotifyEvent *) event;

        // xevent->more would inform us if there is more events for the
        // rendering operation. but there isn't interface to pass the
        // information to damageEvent.
        emit damageEvent(xevent->damage, xevent->area.x, xevent->area.y, xevent->area.width, xevent->area.height);
        eventHandled = true;
    }

    foreach (XEventListener* listener, xEventListeners) {
        if (!toBeRemovedEventListeners.contains(listener)) {
            if (listener->handleXEvent(*event)) {
                eventHandled = true;
            }
        }
    }
    iteratorActiveForEventListenerContainer = false;

    // Remove now any event listeners that got removed while going through the event listeners
    foreach (XEventListener* listener, toBeRemovedEventListeners) {
        xEventListeners.removeOne(listener);
    }
    toBeRemovedEventListeners.clear();

    if (!eventHandled) {
        eventHandled = QApplication::x11EventFilter(event);
    }

    return eventHandled;
}

const QString &HomeApplication::qmlPath() const
{
    return _qmlPath;
}

void HomeApplication::setQmlPath(const QString &path)
{
    _qmlPath = path;

    if (_mainWindowInstance)
        _mainWindowInstance->setSource(path);
}

QDeclarativeView *HomeApplication::mainWindowInstance()
{
    if (_mainWindowInstance)
        return _mainWindowInstance;

    _mainWindowInstance = new QDeclarativeView();
    _mainWindowInstance->setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);

    // Setting optimalization flags
    _mainWindowInstance->setOptimizationFlag(QGraphicsView::DontSavePainterState);
    _mainWindowInstance->setResizeMode(QDeclarativeView::SizeRootObjectToView);
    _mainWindowInstance->setAutoFillBackground(false);
    _mainWindowInstance->setAttribute(Qt::WA_OpaquePaintEvent);
    _mainWindowInstance->setAttribute(Qt::WA_NoSystemBackground);
    _mainWindowInstance->viewport()->setAutoFillBackground(false);
    _mainWindowInstance->viewport()->setAttribute(Qt::WA_OpaquePaintEvent);
    _mainWindowInstance->viewport()->setAttribute(Qt::WA_NoSystemBackground);

    // Setting up the context and engine things
    QObject::connect(_mainWindowInstance->engine(), SIGNAL(quit()), QApplication::instance(), SLOT(quit()));
    _mainWindowInstance->rootContext()->setContextProperty("initialSize", QApplication::desktop()->screenGeometry(_mainWindowInstance).size());
    _mainWindowInstance->rootContext()->setContextProperty("windowManager", new WindowManager(this));
    _mainWindowInstance->rootContext()->setContextProperty("LipstickSettings", LipstickSettings::instance());

    // Setting the source, if present
    if (!_qmlPath.isEmpty())
        _mainWindowInstance->setSource(_qmlPath);

    return _mainWindowInstance;
}
