//===========================================================================
//
// This file is part of the KDE project
//
// Copyright 2004 Chris Howells <howells@kde.org>

#include "lockprocess.h"
#include "autologout.h"

#include <kapplication.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kdialog.h>
#include <ksmserver_interface.h>

#include <QLayout>
#include <QMessageBox>
#include <QLabel>
#include <QStyle>
#include <QApplication>
#include <QDialog>
#include <QAbstractEventDispatcher>
#include <QtGui/QProgressBar>
#include <QtDBus/QtDBus>

#define COUNTDOWN 30

AutoLogout::AutoLogout(LockProcess *parent) : QDialog(parent, Qt::X11BypassWindowManagerHint)
{
    setObjectName("password dialog");
    frame = new QFrame(this);
    frame->setFrameStyle(QFrame::Panel | QFrame::Raised);
    frame->setLineWidth(2);

    QLabel *pixLabel = new QLabel( frame );
    pixLabel->setObjectName( "pixlabel" );
    pixLabel->setPixmap(DesktopIcon("application-exit"));

    QLabel *greetLabel = new QLabel(i18n("<qt><nobr><b>Automatic Log Out</b></nobr></qt>"), frame);
    QLabel *infoLabel = new QLabel(i18n("<qt>To prevent being logged out, resume using this session by moving the mouse or pressing a key.</qt>"), frame);

    mStatusLabel = new QLabel("<b> </b>", frame);
    mStatusLabel->setAlignment(Qt::AlignCenter);

    QLabel *mProgressLabel = new QLabel(i18n("Time Remaining:"), frame);
    mProgressRemaining = new QProgressBar(frame);
    mProgressRemaining->setTextVisible(false);

    QVBoxLayout *unlockDialogLayout = new QVBoxLayout( this );
    unlockDialogLayout->addWidget( frame );

    frameLayout = new QGridLayout(frame);
    frameLayout->setSpacing(KDialog::spacingHint());
    frameLayout->setMargin(KDialog::marginHint());
    frameLayout->addWidget(pixLabel, 0, 0, 3, 1, Qt::AlignCenter | Qt::AlignTop);
    frameLayout->addWidget(greetLabel, 0, 1);
    frameLayout->addWidget(mStatusLabel, 1, 1);
    frameLayout->addWidget(infoLabel, 2, 1);
    frameLayout->addWidget(mProgressLabel, 3, 1);
    frameLayout->addWidget(mProgressRemaining, 4, 1);

    // get the time remaining in seconds for the status label
    mRemaining = COUNTDOWN * 25;

    mProgressRemaining->setMaximum(COUNTDOWN * 25);

    updateInfo(mRemaining);

    mCountdownTimerId = startTimer(1000/25);

    connect(qApp, SIGNAL(activity()), SLOT(slotActivity()));
}

AutoLogout::~AutoLogout()
{
    hide();
}

void AutoLogout::updateInfo(int timeout)
{
    mStatusLabel->setText(i18np("<qt><nobr>You will be automatically logged out in 1 second</nobr></qt>",
                               "<qt><nobr>You will be automatically logged out in %1 seconds</nobr></qt>",
                               timeout / 25) );
    mProgressRemaining->setValue(timeout);
}

void AutoLogout::timerEvent(QTimerEvent *ev)
{
    if (ev->timerId() == mCountdownTimerId)
    {
        updateInfo(mRemaining);
        --mRemaining;
        if (mRemaining < 0)
        {
            killTimer(mCountdownTimerId);
            logout();
        }
    }
}

void AutoLogout::slotActivity()
{
    if (mRemaining >= 0)
        accept();
}

void AutoLogout::logout()
{
    QAbstractEventDispatcher::instance()->unregisterTimers(this);
    org::kde::KSMServerInterface ksmserver("org.kde.ksmserver", "/KSMServer", QDBusConnection::sessionBus());
    ksmserver.logout( 0, 0, 0 );
}

void AutoLogout::setVisible(bool visible)
{
    QDialog::setVisible(visible);

    if (visible)
        QApplication::flush();
}

#include "autologout.moc"
