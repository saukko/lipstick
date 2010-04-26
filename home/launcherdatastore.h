/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of mhome.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#ifndef LAUNCHERBUTTONDATASTORE_H
#define LAUNCHERBUTTONDATASTORE_H

#include <QSharedPointer>
#include <QHash>
#include <QStringList>
#include <QFileSystemWatcher>

class MDataStore;
class LauncherPage;
class LauncherButton;
class MDesktopEntry;
class MDataStore;

/*!
 * \class LauncherDataStore
 * \brief LauncherDataStore provides a way to store .desktop entry file
 * related data such as the location of the .desktop entry in the user
 * interface.
 *
 * The data is stored as key-value pairs. Key is entry file path relative to root.
 * (eg. "usr/share/applications/deskentry.desktop").
 * The value is user specific data.
 *
 * The LauncherDataStore uses a MDataStore as a backend to actually store the 
 * the data. The ownership of the MDataStore is transferred to the LauncherDataStore.
 *
 */
class LauncherDataStore : public QObject
{
    Q_OBJECT

public:
    /*!
     * Constructs LauncherDataStore. The ownership of the MDataStore is transferred
     * to this LauncherDataStore.
     *
     * \param dataStore The backend data store. 
     */
    LauncherDataStore(MDataStore* dataStore);

    /*!
     * Destroys LauncherDataStore
     */
    virtual ~LauncherDataStore();

    /*!
     * Returns the data for all desktop entries stored in the data store.
     */
    QHash<QString, QVariant> dataForAllDesktopEntries();

    /*!
     * Updates the data for a desktop entry in the data store.
     *
     * \param entryPath the path of the desktop entry to update
     * \param data the data to update the desktop entry with
     * \return \c true if the update was successful, \c false otherwise
     */
    bool updateDataForDesktopEntry(const QString &entryPath, const QVariant &data);

signals:
    /*!
     * \brief A signal for informing that the contents of the data store have changed.
     */
    void dataStoreChanged();

private slots:
    /*!
     * Updates the contents of the data store from the desktop entries.
     * New .desktop files are added to the data store with no associated data.
     * .desktop files that do not exist anymore are removed from the data store.
     * The dataStoreChanged() signal is emitted after changes are made.
     */
    void updateDataFromDesktopEntryFiles();

private:
    /*!
     * Checks if desktop entry is valid.
     *
     * \param entry Desktop entry to be validated
     * \param acceptedTypes List of accepted entry types
     * \return is desktop entry valid
     */
    bool isDesktopEntryValid(const MDesktopEntry &entry, const QStringList &acceptedTypes);

    /*!
      * This helper method gets a key from entry path by adding key prefix to the path.
      *
      * Due QSettings MFileDataStore removes preceding forward slash from key.
      * To overcome this we need to use a key prefix in the key.
      *
      * \param entryPath The absolute entry path of the desktop entry (eg. "/usr/share/applications/deskentry.desktop").
      * \return The key for the entry.
      */
    QString entryPathToKey(QString entryPath);

    /*!
      * This helper method gets entry path from key by removing key prefix.
      *
      * Due QSettings MFileDataStore removes preceding forward slash from key.
      * To overcome this we need to use a key prefix in the key.
      *
      * \param key The key as key prefix and entry path (eg. "KEY_PREFIX/usr/share/applications/deskentry.desktop").
      * \return The entry path.
      */
    QString keyToEntryPath(QString key);

    //! A file system watcher for the desktop entry file directory
    QFileSystemWatcher watcher;

    //! List of file types to support for desktop entries.
    QStringList supportedDesktopEntryFileTypes;

    //! The actual data store where the data is stored.
    MDataStore *store;
};

#endif
