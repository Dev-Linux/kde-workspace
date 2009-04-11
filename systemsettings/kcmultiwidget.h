/*
   Copyright (c) 2000 Matthias Elter <elter@kde.org>
   Copyright (c) 2003 Daniel Molkentin <molkentin@kde.org>
   Copyright (c) 2003 Matthias Kretz <kretz@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.

*/

#ifndef KCMULTIWIDGET_H
#define KCMULTIWIDGET_H

// Qt
#include <QtCore/QHash>
#include <QtCore/QList>

// KDE
#include <kservice.h>
#include <kpagewidget.h>

class QKeyEvent;
class KDialogButtonBox;
class KPushButton;
class KCModuleProxy;
class KCModuleInfo;

/**
 * @short A method that offers a KDialog containing arbitrary Control Modules.
 */
class /*KUTILS_EXPORT*/ KCMultiWidget : public QWidget
{
Q_OBJECT

public:
    /**
     * Constructs a new KCMultiWidget
     *
     * @param parent The parent widget
     **/
    KCMultiWidget( QWidget *parent=0 );

    /**
     * Destructor
     **/
    virtual ~KCMultiWidget();

    /**
     * Add a module.
     *
     * @param moduleinfo Pass a KCModuleInfo object which will be
     *                   used for creating the module. It will be added
     *                   to the list of modules the dialog will show.
     **/
    void addModule(const KCModuleInfo& moduleinfo);

    /**
     * @return the current module that is being shown.
     */
    KCModuleProxy * currentModule();

    /**
     * Ask if it is ok to close the dialog
     */
    bool queryClose();

signals:
    /**
     * Emitted after all KCModules have been told to save their configuration.
     *
     * The applyClicked signal is emitted before the configuration is saved.
     */
    void configCommitted();

    /**
     * Emitted after the KCModules have been told to save their configuration.
     * It is emitted once for every instance the KCMs that were changed belong
     * to.
     *
     * You can make use of this if you have more than one component in your
     * application. instanceName tells you the instance that has to reload its
     * configuration.
     *
     * The applyClicked signal is emitted before the configuration is saved.
     *
     * @param instanceName The name of the instance that needs to reload its
     *                     configuration.
     */
    void configCommitted( const QByteArray & instanceName );

    /**
     * Emitted right before a module is shown.
     * Useful for updating caption, help, etc
     *
     * @param moduleinfo The module that is about to be shown.
     * @sense 3.4
     */
    void aboutToShow( KCModuleProxy *moduleProxy );
    
    /**
     * Signal to emulate former KDialog behaviour
     * Emitted when quitting manually (Esc key)
     */
    void requestClose();
    
protected slots:
    /**
     * This slot is called when the user presses the "Default" Button.
     * You can reimplement it if needed.
     *
     * @note Make sure you call the original implementation.
     **/
    virtual void slotDefault();

    /**
     * This slot is called when the user presses the "Reset" Button.
     * You can reimplement it if needed.
     *
     * @note Make sure you call the original implementation.
     */
    virtual void slotReset();

    /**
     * This slot is called when the user presses the "Apply" Button.
     * You can reimplement it if needed.
     *
     * @note Make sure you call the original implementation.
     **/
    virtual void slotApply();

    /**
     * This slot is called when the user presses the "Help" Button.
     * It reads the DocPath field of the currently selected KControl
     * module's .desktop file to find the path to the documentation,
     * which it then attempts to load.
     *
     * You can reimplement this slot if needed.
     *
     * @note Make sure you call the original implementation.
     **/
    virtual void slotHelp();

private slots:

    void slotAboutToShow(QWidget *);

    void slotAboutToShow(KPageWidgetItem* current, KPageWidgetItem* before);

    void clientChanged(bool state);

// Currently unused. Whenever root mode comes back
#if 0
    /**
     * Called when entering root mode, and disables
     * the Admin Mode button such that the user doesn't do it
     * twice.
     */
    void disableRModeButton();

    /**
     * Called when the current module exits from root
     * mode. Enables the Administrator Mode button, again.
     */
    void rootExit();
#endif

private:
    void setupButtonBox();

    void keyPressEvent(QKeyEvent *);
    
    void apply(KCModuleProxy *module);
    void reset(KCModuleProxy *module);
    void defaults(KCModuleProxy *module);
    
    bool queryClose(KCModuleProxy *);

    struct CreatedModule
    {
        KCModuleProxy * kcm;
        KService::Ptr service;
        bool adminmode;
    };
    typedef QList<CreatedModule> ModuleList;
    ModuleList m_modules;

    QHash<KCModuleProxy*,QStringList> moduleParentComponents;
    QString _docPath;

    KPageWidget         *   m_pageWidget;
    KDialogButtonBox    *   m_buttonBox;
    
    KPushButton         *   m_helpButton;
    KPushButton         *   m_resetButton;
    KPushButton         *   m_defaultsButton;
    KPushButton         *   m_applyButton;
    
    class KCMultiWidgetPrivate;
    KCMultiWidgetPrivate *d;
};

#endif //KCMULTIWIDGET_H
