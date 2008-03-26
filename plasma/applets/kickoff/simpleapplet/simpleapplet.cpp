/*
    Copyright 2007 Robert Knight <robertknight@gmail.com>

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

// Own
#include "simpleapplet/simpleapplet.h"
#include "simpleapplet/menuview.h"

// Qt
#include <QLabel>
#include <QComboBox>
#include <QGridLayout>
#include <QGraphicsView>
#include <QMetaObject>
#include <QMetaEnum>
#include <QPointer>

// KDE
#include <KIcon>
#include <KDialog>
#include <KMenu>

// Plasma
#include <plasma/layouts/boxlayout.h>
#include <plasma/widgets/icon.h>
#include <plasma/containment.h>

// Local
#include "core/itemhandlers.h"
#include "core/models.h"
#include "core/applicationmodel.h"
#include "core/favoritesmodel.h"
#include "core/systemmodel.h"
#include "core/recentlyusedmodel.h"
#include "core/leavemodel.h"
#include "core/urlitemlauncher.h"

class MenuLauncherApplet::Private
{
public:
        KMenu *menuview;
        Plasma::Icon *icon;
        QPointer<Kickoff::UrlItemLauncher> launcher;

        MenuLauncherApplet::ViewType viewtype;
        MenuLauncherApplet::FormatType formattype;

        KDialog *dialog;
        QComboBox *viewComboBox;

        QAction* switcher;
        Private() : menuview(0), launcher(0), dialog(0), switcher(0) {}
        ~Private() { delete dialog; delete menuview; }

        void addItem(QComboBox* combo, const QString& caption, int index) {
            combo->addItem(caption, index);
        }

        void setCurrentItem(QComboBox* combo, int currentIndex) {
            for(int i = combo->count() - 1; i >= 0; --i) {
                if( combo->itemData(i).toInt() == currentIndex ) {
                    combo->setCurrentIndex(i);
                    return;
                }
            }
            if( combo->count() > 0 )
                combo->setCurrentIndex(0);
        }

        Kickoff::MenuView *createMenuView(QAbstractItemModel *model = 0) {
            Kickoff::MenuView *view = new Kickoff::MenuView(menuview);
            view->setFormatType( (Kickoff::MenuView::FormatType) formattype );
            if( model ) {
                model->setParent(view); //re-parent
                view->setModel(model);
            }
            return view;
        }

        void addMenu(Kickoff::MenuView *view, bool mergeFirstLevel) {
            QList<QAction*> actions = view->actions();
            foreach(QAction *action, actions) {
                if( action->menu() && mergeFirstLevel ) {
                    QMetaObject::invokeMethod(action->menu(),"aboutToShow"); //fetch the children
                    if( actions.count() > 1 )
                        menuview->addTitle(action->text());
                    foreach(QAction *a, action->menu()->actions())
                        menuview->addAction(a);
                }
                else {
                    menuview->addAction(action);
                }
            }
        }
};

MenuLauncherApplet::MenuLauncherApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent,args),
      d(new Private)
{
    setHasConfigurationInterface(true);

    d->icon = new Plasma::Icon(QString(), this);
    d->icon->setFlag(ItemIsMovable, false);
    connect(d->icon, SIGNAL(pressed(bool)), this, SLOT(toggleMenu(bool)));

    d->viewtype = Combined;
    d->formattype = NameDescription;
}

MenuLauncherApplet::~MenuLauncherApplet()
{
    delete d;
}

void MenuLauncherApplet::init()
{
    KConfigGroup cg = config();

    d->icon->setIcon(KIcon(cg.readEntry("icon","start-here-kde")));
    //setMinimumContentSize(d->icon->iconSize()); //setSize(d->icon->iconSize())

    {
        QMetaEnum e = metaObject()->enumerator(metaObject()->indexOfEnumerator("ViewType"));
        QByteArray ba = cg.readEntry("view", QByteArray(e.valueToKey(d->viewtype)));
        d->viewtype = (MenuLauncherApplet::ViewType) e.keyToValue(ba);
    }
    {
        QMetaEnum e = metaObject()->enumerator(metaObject()->indexOfEnumerator("FormatType"));
        QByteArray ba = cg.readEntry("format", QByteArray(e.valueToKey(d->formattype)));
        d->formattype = (MenuLauncherApplet::FormatType) e.keyToValue(ba);
    }

    Kickoff::UrlItemLauncher::addGlobalHandler(Kickoff::UrlItemLauncher::ExtensionHandler,"desktop",new Kickoff::ServiceItemHandler);
    Kickoff::UrlItemLauncher::addGlobalHandler(Kickoff::UrlItemLauncher::ProtocolHandler, "leave", new Kickoff::LeaveItemHandler);
}
    
QSizeF MenuLauncherApplet::sizeHint() const
{
    //ensure a square size in the panel
    return QSizeF(size().height(), size().height());
}

void MenuLauncherApplet::constraintsUpdated(Plasma::Constraints constraints)
{
    if (constraints & Plasma::FormFactorConstraint) {
        if (formFactor() == Plasma::Planar ||
            formFactor() == Plasma::MediaCenter) {
            setMinimumContentSize(d->icon->sizeFromIconSize(IconSize(KIconLoader::Desktop)));
        } else {
            setMinimumContentSize(d->icon->sizeFromIconSize(IconSize(KIconLoader::Panel)));
        }
    }

    if (constraints & Plasma::SizeConstraint) {
        d->icon->resize(contentSize());
    }
}

void MenuLauncherApplet::switchMenuStyle()
{
    if (containment()) {
        containment()->addApplet("launcher", QVariantList(), 0, geometry());
        destroy();
    }
}

void MenuLauncherApplet::showConfigurationInterface()
{
    if (! d->dialog) {
        d->dialog = new KDialog();
        d->dialog->setCaption( i18nc("@title:window","Configure Menu") );
        d->dialog->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );
        connect(d->dialog, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
        connect(d->dialog, SIGNAL(okClicked()), this, SLOT(configAccepted()));

        QWidget *p = d->dialog->mainWidget();
        QGridLayout *l = new QGridLayout(p);
        p->setLayout(l);

        l->addWidget(new QLabel(i18n("View:"), p), 0, 0);
        d->viewComboBox = new QComboBox(p);
        d->addItem(d->viewComboBox, i18n("Combined"), MenuLauncherApplet::Combined);
        d->addItem(d->viewComboBox, i18n("Favorites"), MenuLauncherApplet::Favorites);
        d->addItem(d->viewComboBox, i18n("Applications"), MenuLauncherApplet::Applications);
        d->addItem(d->viewComboBox, i18n("Computer"), MenuLauncherApplet::Computer);
        d->addItem(d->viewComboBox, i18n("Recently Used"), MenuLauncherApplet::RecentlyUsed);
        d->addItem(d->viewComboBox, i18n("Leave"), MenuLauncherApplet::Leave);
        l->addWidget(d->viewComboBox, 0, 1);

        l->setColumnStretch(1,1);
    }

    d->setCurrentItem(d->viewComboBox, d->viewtype);
    d->dialog->show();
}

void MenuLauncherApplet::configAccepted()
{
    bool needssaving = false;
    KConfigGroup cg = config();

    int vt = d->viewComboBox->itemData(d->viewComboBox->currentIndex()).toInt();
    if( vt != d->viewtype ) {
        d->viewtype = (MenuLauncherApplet::ViewType) vt;
        needssaving = true;

        QMetaEnum e = metaObject()->enumerator(metaObject()->indexOfEnumerator("ViewType"));
        cg.writeEntry("view", QByteArray(e.valueToKey(d->viewtype)));
    }

    if( needssaving ) {
        emit configNeedsSaving();

        delete d->menuview;
        d->menuview = 0;
    }
}

Qt::Orientations MenuLauncherApplet::expandingDirections() const
{
    return 0;
}

void MenuLauncherApplet::toggleMenu(bool pressed)
{
    if (!pressed) {
        return;
    }

    if (!d->menuview) {
        d->menuview = new KMenu();
        connect(d->menuview,SIGNAL(triggered(QAction*)),this,SLOT(actionTriggered(QAction*)));
        connect(d->menuview,SIGNAL(aboutToHide()),d->icon,SLOT(setUnpressed()));

        switch( d->viewtype ) {
            case Combined: {
                ApplicationModel *appModel = new ApplicationModel();
                appModel->setDuplicatePolicy(ApplicationModel::ShowLatestOnlyPolicy);
                Kickoff::MenuView *appview = d->createMenuView(appModel);
                d->addMenu(appview, false);

                d->menuview->addSeparator();
                Kickoff::MenuView *favview = d->createMenuView(new Kickoff::FavoritesModel(d->menuview));
                d->addMenu(favview, false);

                d->menuview->addSeparator();
                QAction *switchaction = d->menuview->addAction(KIcon("system-switch-user"),i18n("Switch User"));
                switchaction->setData(QUrl("leave:/switch"));
                QAction *lockaction = d->menuview->addAction(KIcon("system-lock-screen"),i18n("Lock"));
                lockaction->setData(QUrl("leave:/lock"));
                QAction *logoutaction = d->menuview->addAction(KIcon("system-log-out"),i18n("Logout"));
                logoutaction->setData(QUrl("leave:/logout"));
            } break;
            case Favorites: {
                Kickoff::MenuView *favview = d->createMenuView(new Kickoff::FavoritesModel(d->menuview));
                d->addMenu(favview, true);
            } break;
            case Applications: {
                ApplicationModel *appModel = new ApplicationModel();
                appModel->setDuplicatePolicy(ApplicationModel::ShowLatestOnlyPolicy);
                Kickoff::MenuView *appview = d->createMenuView(appModel);
                d->addMenu(appview, false);
            } break;
            case Computer: {
                Kickoff::MenuView *systemview = d->createMenuView(new Kickoff::SystemModel());
                d->addMenu(systemview, true);
            } break;
            case RecentlyUsed: {
                Kickoff::MenuView *recentlyview = d->createMenuView(new Kickoff::RecentlyUsedModel());
                d->addMenu(recentlyview, true);
            } break;
            case Leave: {
                Kickoff::MenuView *leaveview = d->createMenuView(new Kickoff::LeaveModel(d->menuview));
                d->addMenu(leaveview, true);
            } break;
        }
    }

    QGraphicsView *viewWidget = view();
    QPoint globalPos;
    if (viewWidget) {
        const QPoint viewPos = viewWidget->mapFromScene(scenePos());
        globalPos = viewWidget->mapToGlobal(viewPos);
        int ypos = globalPos.ry() - d->menuview->sizeHint().height();
        if( ypos < 0 ) {
            const QRect size = mapToView(viewWidget, contentRect());
            ypos = globalPos.ry() + size.height();
        }
        globalPos.ry() = ypos;
    }

    d->menuview->setAttribute(Qt::WA_DeleteOnClose);
    d->menuview->popup(globalPos);
    d->icon->setPressed();
}

void MenuLauncherApplet::actionTriggered(QAction *action)
{
    if (action->data().type() == QVariant::Url) {
        QUrl url = action->data().toUrl();
        if (url.scheme() == "leave") {
            if ( ! d->launcher ) {
                d->launcher = new Kickoff::UrlItemLauncher(d->menuview);
            }
            d->launcher->openUrl(url.toString());
        }
    }
    else {
        for(QWidget* w = action->parentWidget(); w; w = w->parentWidget()) {
            if (Kickoff::MenuView *view = dynamic_cast<Kickoff::MenuView*>(w)) {
                view->actionTriggered(action);
                break;
            }
        }
    }
}

#include "simpleapplet.moc"
