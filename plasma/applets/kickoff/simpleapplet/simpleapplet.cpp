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
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QtDebug>

// KDE
#include <KIcon>
#include <KDialog>

// Plasma
#include <plasma/layouts/boxlayout.h>
#include <plasma/widgets/icon.h>
#include <plasma/containment.h>

// Local
#include "core/itemhandlers.h"
#include "core/models.h"
#include "core/applicationmodel.h"
#include "core/favoritesmodel.h"
#include "core/leavemodel.h"

class MenuLauncherApplet::Private
{
public:
        QMenu *menuview;
        Plasma::Icon *icon;
        Kickoff::MenuView *appview;
        Kickoff::MenuView* favview;

        bool showFavorites;
        bool showLeaveSwitchUser;
        bool showLeaveLock;
        bool showLeaveLogout;

        KDialog *dialog;
        QCheckBox *showFavCheckBox;
        QCheckBox *showSwitchUserCheckBox;
        QCheckBox *showLockCheckBox;
        QCheckBox *showLogoutCheckBox;

        Private() : menuview(0), appview(0), favview(0), dialog(0) {}
        ~Private() { delete dialog; delete menuview; delete appview; delete favview; }
};

MenuLauncherApplet::MenuLauncherApplet(QObject *parent, const QVariantList &args)
    : Plasma::Applet(parent,args),
      d(new Private)
{
    setHasConfigurationInterface(true);

    Plasma::HBoxLayout *layout = new Plasma::HBoxLayout(this);
    layout->setMargin(0);

    d->icon = new Plasma::Icon(QString(), this);
    d->icon->setFlag(ItemIsMovable, false);
    connect(d->icon, SIGNAL(pressed(bool, QGraphicsSceneMouseEvent*)), this, SLOT(toggleMenu(bool,QGraphicsSceneMouseEvent*)));

    d->showFavorites = true;
    d->showLeaveSwitchUser = true;
    d->showLeaveLock = true;
    d->showLeaveLogout = true;
}

MenuLauncherApplet::~MenuLauncherApplet()
{
    delete d;
}

void MenuLauncherApplet::init()
{
    KConfigGroup cg = config();

    d->icon->setIcon(KIcon(cg.readEntry("icon","start-here")));
    //setMinimumContentSize(d->icon->iconSize()); //setSize(d->icon->iconSize())

    d->showFavorites = cg.readEntry("showFavorites",d->showFavorites);
    d->showLeaveSwitchUser = cg.readEntry("showLeaveSwitchUser",d->showLeaveSwitchUser);
    d->showLeaveLock = cg.readEntry("showLeaveLock",d->showLeaveLock);
    d->showLeaveLogout = cg.readEntry("showLeaveLogout",d->showLeaveLogout);

    Kickoff::UrlItemLauncher::addGlobalHandler(Kickoff::UrlItemLauncher::ExtensionHandler,"desktop",new Kickoff::ServiceItemHandler);
    Kickoff::UrlItemLauncher::addGlobalHandler(Kickoff::UrlItemLauncher::ProtocolHandler, "leave", new Kickoff::LeaveItemHandler);
}

QSizeF MenuLauncherApplet::contentSizeHint() const
{
    return QSizeF(48,48);
}

Qt::Orientations MenuLauncherApplet::expandingDirections() const
{
    return 0;
}

void MenuLauncherApplet::showConfigurationInterface()
{
    if (! d->dialog) {
        d->dialog = new KDialog();
        d->dialog->setCaption( i18nc("@title:window","Configure Menu") );
        d->dialog->setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply );
        connect(d->dialog, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
        connect(d->dialog, SIGNAL(okClicked()), this, SLOT(configAccepted()));
    
        QVBoxLayout *layout = new QVBoxLayout(d->dialog->mainWidget());
        d->dialog->mainWidget()->setLayout(layout);

        d->showFavCheckBox = new QCheckBox(i18n("Favorites"), d->dialog->mainWidget());
        d->showFavCheckBox->setCheckState(d->showFavorites ? Qt::Checked : Qt::Unchecked);
        layout->addWidget(d->showFavCheckBox);
    
        d->showSwitchUserCheckBox = new QCheckBox(i18n("Switch User"), d->dialog->mainWidget());
        d->showSwitchUserCheckBox->setCheckState(d->showLeaveSwitchUser ? Qt::Checked : Qt::Unchecked);
        layout->addWidget(d->showSwitchUserCheckBox);
    
        d->showLockCheckBox = new QCheckBox(i18n("Lock"), d->dialog->mainWidget());
        d->showLockCheckBox->setCheckState(d->showLeaveLock ? Qt::Checked : Qt::Unchecked);
        layout->addWidget(d->showLockCheckBox);
    
        d->showLogoutCheckBox = new QCheckBox(i18n("Logout"), d->dialog->mainWidget());
        d->showLogoutCheckBox->setCheckState(d->showLeaveLogout ? Qt::Checked : Qt::Unchecked);
        layout->addWidget(d->showLogoutCheckBox);
    }
    d->dialog->show();
}

void MenuLauncherApplet::configAccepted()
{
    d->showFavorites = d->showFavCheckBox->checkState() == Qt::Checked;
    d->showLeaveSwitchUser = d->showSwitchUserCheckBox->checkState() == Qt::Checked;
    d->showLeaveLock = d->showLockCheckBox->checkState() == Qt::Checked;
    d->showLeaveLogout = d->showLogoutCheckBox->checkState() == Qt::Checked;

    KConfigGroup cg = config();
    cg.writeEntry("showFavorites", d->showFavorites);
    cg.writeEntry("showLeaveSwitchUser", d->showLeaveSwitchUser);
    cg.writeEntry("showLeaveLock", d->showLeaveLock);
    cg.writeEntry("showLeaveLogout", d->showLeaveLogout);
    cg.config()->sync();

    delete d->menuview;
    d->menuview = 0;
}

void MenuLauncherApplet::toggleMenu(bool pressed, QGraphicsSceneMouseEvent *event)
{
    if (!pressed) {
        return;
    }

    if (!d->menuview) {
        d->menuview = new QMenu();
        connect(d->menuview,SIGNAL(triggered(QAction*)),this,SLOT(actionTriggered(QAction*)));

        if(!d->appview) {
            d->appview = new Kickoff::MenuView();
            ApplicationModel *appModel = new ApplicationModel(d->appview);
            appModel->setDuplicatePolicy(ApplicationModel::ShowLatestOnlyPolicy);
            d->appview->setModel(appModel);
        }
        foreach (QAction *action, d->appview->actions())
            d->menuview->addAction(action);
        connect(d->menuview,SIGNAL(triggered(QAction*)),d->appview,SLOT(actionTriggered(QAction*)));

        if (d->showFavorites) {
            if (!d->favview) {
                d->favview = new Kickoff::MenuView();
                Kickoff::FavoritesModel* favmodel = new Kickoff::FavoritesModel(d->favview);
                d->favview->setModel(favmodel);
            }
            d->menuview->addSeparator();
            foreach (QAction *action, d->favview->actions())
                d->menuview->addAction(action);
            connect(d->menuview,SIGNAL(triggered(QAction*)),d->favview,SLOT(actionTriggered(QAction*)));
        }

        if (d->showLeaveSwitchUser || d->showLeaveLock || d->showLeaveLogout) {
            d->menuview->addSeparator();
            if (d->showLeaveSwitchUser) {
                QAction *lockaction = d->menuview->addAction(KIcon("system-switch-user"),i18n("Switch User"));
                lockaction->setData(QUrl("leave:/switch"));
            }
            if (d->showLeaveLock) {
                QAction *lockaction = d->menuview->addAction(KIcon("system-lock-screen"),i18n("Lock"));
                lockaction->setData(QUrl("leave:/lock"));
            }
            if (d->showLeaveLogout) {
                QAction *logoutaction = d->menuview->addAction(KIcon("system-log-out"),i18n("Logout"));
                logoutaction->setData(QUrl("leave:/logout"));
            }
        }
    }

    const QPointF scenePos = mapToScene(boundingRect().topLeft());
    QWidget *viewWidget = event->widget() ? event->widget()->parentWidget() : 0;
    QGraphicsView *view = qobject_cast<QGraphicsView*>(viewWidget);
    QPoint globalPos;
    if (view) {
        const QPoint viewPos = view->mapFromScene(scenePos);
        globalPos = view->mapToGlobal(viewPos);
        int ypos = globalPos.ry() - d->menuview->sizeHint().height();
        if( ypos < 0 ) {
            const QRect size = mapToView(view, boundingRect());
            ypos = globalPos.ry() + size.height();
        }
        globalPos.ry() = ypos;
    }

    d->menuview->setAttribute(Qt::WA_DeleteOnClose);
    d->menuview->popup(globalPos);
}

void MenuLauncherApplet::actionTriggered(QAction *action)
{
    if (action->data().type() == QVariant::Url) {
        QUrl url = action->data().toUrl();
        if (url.scheme() == "leave") {
            Kickoff::UrlItemLauncher *launcher = d->appview ? d->appview->launcher() : 0;
            if (launcher)
                launcher->openUrl(url.toString());
        }
    }
}

#include "simpleapplet.moc"
