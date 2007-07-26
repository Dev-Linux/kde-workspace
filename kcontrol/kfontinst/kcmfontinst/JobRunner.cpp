/*
 * KFontInst - KDE Font Installer
 *
 * Copyright 2003-2007 Craig Drummond <craig@kde.org>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "JobRunner.h"
#include "KfiConstants.h"
#include "Misc.h"
#include <kio/jobuidelegate.h>
#include <kio/skipdialog.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kpassworddialog.h>
#include <kdesu/su.h>
#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>
#include <QX11Info>
#include <QTimer>
#include <X11/Xlib.h>
#include <fixx11h.h>
#include <sys/resource.h>

namespace KFI
{

static KUrl toggle(const KUrl &orig, bool enable)
{
    KUrl url(orig);

    url.setFileName(enable
                 ? Misc::getFile(orig.path()).mid(1)
                 : QChar('.')+Misc::getFile(orig.path()));
    return url;
}

class CPasswordDialog : public KPasswordDialog
{
    public:

    CPasswordDialog(QWidget *parent)
        : KPasswordDialog(parent),
          itsSuProc(KFI_SYS_USER)
    {
        setCaption(i18n("Authorisation Required"));

        if(itsSuProc.useUsersOwnPassword())
            setPrompt(i18n("The requested action requires administrator privileges.\n"
                           "If you have these privileges, then please enter your password."));
        else
            setPrompt(i18n("The requested action requires administrator privileges.\n"
                           "Please enter the system administrator's password."));

        setPixmap(DesktopIcon("password"));
    }

    bool checkPassword()
    {
        switch (itsSuProc.checkInstall(password().toLocal8Bit()))
        {
            case -1:
                showErrorMessage(itsSuProc.useUsersOwnPassword()
                                    ? i18n("Insufficient privileges.")
                                    : i18n("Conversation with su failed."), UsernameError);
                return false;
            case 0:
                return true;
            case SuProcess::SuNotFound:
                showErrorMessage(i18n("<p>Could not launch '%1'.</p>"
                                      "<p>Make sure your PATH is set correctly.</p>",
                                      itsSuProc.useUsersOwnPassword() ? "sudo" : "su"), FatalError);
                return false;
            case SuProcess::SuNotAllowed:
                showErrorMessage(i18n("Insufficient privileges."), FatalError);
                return false;
            case SuProcess::SuIncorrectPassword:
                showErrorMessage(i18n("Incorrect password, please try again."), PasswordError);
                return false;
            default:
                showErrorMessage(i18n("Internal error: illegal return from "
                                      "SuProcess::checkInstall()"), FatalError);
                done(Rejected);
                return false;
        }
    }

    SuProcess itsSuProc;
};

CJobRunner::CJobRunner(QWidget *parent, int xid)
           : CActionDialog(parent),
             itsIt(itsUrls.end()),
             itsEnd(itsIt),
             itsAutoSkip(false),
             itsCancelClicked(false)
{
    // Set core dump size to 0 because we will have root's password in memory.
    struct rlimit rlim;
    rlim.rlim_cur=rlim.rlim_max=0;
    setrlimit(RLIMIT_CORE, &rlim);

    setModal(true);
    setButtons(KDialog::Cancel);
    if(NULL==parent && 0!=xid)
        XSetTransientForHint(QX11Info::display(), winId(), xid);

    QFrame *page = new QFrame(this);
    setMainWidget(page);

    QGridLayout *layout=new QGridLayout(page);
    layout->setMargin(KDialog::marginHint());
    layout->setSpacing(KDialog::spacingHint());
    itsStatusLabel=new QLabel(page);
    itsProgress=new QProgressBar(page);
    layout->addWidget(itsPixmapLabel, 0, 0, 2, 1);
    layout->addWidget(itsStatusLabel, 0, 1);
    layout->addWidget(itsProgress, 1, 1);
}

CJobRunner::~CJobRunner()
{
}

bool CJobRunner::getAdminPasswd(QWidget *parent)
{
    if(!Misc::root())
    {
        // Prompt the user for a password, if we do not already have it...
        if(itsPasswd.isEmpty() || 0!=SuProcess(KFI_SYS_USER).checkInstall(itsPasswd.toLocal8Bit()))
        {
            CPasswordDialog dlg(parent);

            if(!dlg.exec())
                return false;
            itsPasswd=dlg.password().toLocal8Bit();
        }
    }

    return true;
}

void CJobRunner::getAssociatedUrls(const KUrl &url, KUrl::List &list, bool afmAndPfm, QWidget *widget)
{
    QString ext(url.path());
    int     dotPos(ext.lastIndexOf('.'));
    bool    check(false);

    if(-1==dotPos) // Hmm, no extension - check anyway...
        check=true;
    else           // Cool, got an extension - see if its a Type1 font...
    {
        ext=ext.mid(dotPos+1);
        check=0==ext.compare("pfa", Qt::CaseInsensitive) ||
              0==ext.compare("pfb", Qt::CaseInsensitive);
    }

    if(check)
    {
        const char *afm[]={"afm", "AFM", "Afm", NULL},
                   *pfm[]={"pfm", "PFM", "Pfm", NULL};
        bool       gotAfm(false),
                   localFile(url.isLocalFile());
        int        e;

        for(e=0; afm[e]; ++e)
        {
            KUrl statUrl(url);
            KIO::UDSEntry uds;

            statUrl.setPath(Misc::changeExt(url.path(), afm[e]));

            if(localFile ? Misc::fExists(statUrl.path()) : KIO::NetAccess::stat(statUrl, uds, widget))
            {
                list.append(statUrl);
                gotAfm=true;
                break;
            }
        }

        if(afmAndPfm || !gotAfm)
            for(e=0; pfm[e]; ++e)
            {
                KUrl          statUrl(url);
                KIO::UDSEntry uds;
                statUrl.setPath(Misc::changeExt(url.path(), pfm[e]));
                if(localFile ? Misc::fExists(statUrl.path()) : KIO::NetAccess::stat(statUrl, uds, widget))
                {
                    list.append(statUrl);
                    break;
                }
            }
    }
}

int CJobRunner::exec(ECommand cmd, const ItemList &urls, const KUrl &dest)
{
    switch(cmd)
    {
        case CMD_INSTALL:
            setCaption(i18n("Installing"));
            break;
        case CMD_DELETE:
            setCaption(i18n("Uninstalling"));
            break;
        case CMD_ENABLE:
            setCaption(i18n("Enabling"));
            break;
        case CMD_COPY:
            setCaption(i18n("Copying"));
            break;
        case CMD_MOVE:
            setCaption(i18n("Moving"));
            break;
        case CMD_UPDATE:
            setCaption(i18n("Updating"));
        default:
        case CMD_DISABLE:
            setCaption(i18n("Disabling"));
    }

    itsDest=dest;
    itsUrls=urls;
    itsIt=itsUrls.begin();
    itsEnd=itsUrls.end();
    itsProgress->setValue(0);
    itsProgress->setRange(0, itsUrls.count()+1);
    itsProgress->show();
    itsCmd=cmd;
    itsStatusLabel->setText(QString());
    itsAutoSkip=itsCancelClicked=false;
    QTimer::singleShot(0, this, SLOT(doNext()));
    return CActionDialog::exec();
}

void CJobRunner::doNext()
{
    if(itsIt==itsEnd || CMD_UPDATE==itsCmd)
    {
        //
        // Installation, deletion, enabling, disabling, completed - so now reconfigure...
        QByteArray  packedArgs;
        QDataStream stream(&packedArgs, QIODevice::WriteOnly);

        itsStatusLabel->setText(i18n("Updating font configuration. Please wait..."));

        stream << KFI::SPECIAL_CONFIGURE;

        if(CMD_UPDATE==itsCmd)
        {
            itsProgress->hide();
            for(; itsIt!=itsEnd; ++itsIt)
                stream << (*itsIt);
        }
        else
            itsProgress->setValue(itsProgress->maximum());

        itsUrls.empty();
        itsIt=itsEnd=itsUrls.end();

        KIO::Job *job=KIO::special(KUrl(KFI_KIO_FONTS_PROTOCOL":/"), packedArgs, false);
        setMetaData(job);
        connect(job, SIGNAL(result(KJob *)), SLOT(cfgResult(KJob *)));
        job->ui()->setWindow(this);
        job->ui()->setAutoErrorHandlingEnabled(false);
        job->ui()->setAutoWarningHandlingEnabled(false);
    }
    else
    {
        KIO::Job *job(NULL);

        switch(itsCmd)
        {
            case CMD_COPY:
            case CMD_INSTALL:
            {
                KUrl dest(itsDest);

                dest.setFileName(Misc::getFile((*itsIt).path()));
                itsStatusLabel->setText(CMD_INSTALL==itsCmd ? i18n("Installing %1", (*itsIt).displayName())
                                                            : i18n("Copying %1", (*itsIt).displayName()) );
                job=KIO::file_copy(*itsIt, dest, -1, false, false, false);
                break;
            }
            case CMD_DELETE:
                itsStatusLabel->setText(i18n("Deleting %1", (*itsIt).displayName()));
                job=KIO::file_delete(*itsIt, false);
                break;
            case CMD_ENABLE:
                itsStatusLabel->setText(i18n("Enabling %1", (*itsIt).displayName()));
                job=KIO::rename(*itsIt, toggle(*itsIt, true), false);
                break;
            case CMD_DISABLE:
                itsStatusLabel->setText(i18n("Disabling %1", (*itsIt).displayName()));
                job=KIO::rename(*itsIt, toggle(*itsIt, false), false);
                break;
            case CMD_MOVE:
            {
                KUrl dest(itsDest);

                dest.setFileName(Misc::getFile((*itsIt).path()));
                itsStatusLabel->setText(i18n("Moving %1", (*itsIt).displayName()));
                job=KIO::file_move(*itsIt, dest, -1, false, false, false);
                break;
            }
            default:
                break;
        }
        itsProgress->setValue(itsProgress->value()+1);
        //KIO::Scheduler::assignJobToSlave(slave(), job);
        job->ui()->setWindow(this);
        job->ui()->setAutoErrorHandlingEnabled(false);
        job->ui()->setAutoWarningHandlingEnabled(false);
        setMetaData(job);
        connect(job, SIGNAL(result(KJob *)), SLOT(jobResult(KJob *)));
    }
}

void CJobRunner::jobResult(KJob *job)
{
    Q_ASSERT(job);

    if(itsCancelClicked)
    {
        stopAnimation();
        if(KMessageBox::Yes==KMessageBox::warningYesNo(this, i18n("Are you sure you wish to cancel?")))
            itsIt=itsEnd;
        itsCancelClicked=false;
        startAnimation();
    }

    // itsIt will equal itsEnd if user decided to cancel the current op
    if(itsIt==itsEnd)
        doNext();
    else if (!job->error())
    {
        ++itsIt;
        doNext();
    }
    else
    {
        bool cont(itsAutoSkip && itsUrls.count()>1);

        if(!cont)
        {
            stopAnimation();

            ItemList::ConstIterator next(itsIt==itsEnd ? itsEnd : itsIt+1);

            if(1==itsUrls.count() || next==itsEnd)
            {
                if(!itsAutoSkip)
                    KMessageBox::error(this, job->errorString());
            }
            else
            {
                KIO::SkipDialog dlg(this, true, job->errorString(), true);

                switch(dlg.exec())
                {
                    case KIO::S_SKIP:
                        cont=true;
                        break;
                    case KIO::S_AUTO_SKIP:
                        cont=itsAutoSkip=true;
                        break;
                    case KIO::S_CANCEL:
                        break;
                }
            }
        }

        startAnimation();
        if(cont)
            ++itsIt;
        else
        {
            itsUrls.empty();
            itsIt=itsEnd=itsUrls.end();
        }
        doNext();
    }
}

void CJobRunner::cfgResult(KJob *job)
{
    stopAnimation();

    if(job && 0==job->error())
    {
        hide();
        KMessageBox::information(parentWidget(),
                                 i18n("<p>Please note that any open applications will need to be "
                                      "restarted in order for any changes to be noticed.</p>"),
                                 i18n("Success"), "FontChangesAndOpenApps");
        accept();
    }
    else
        reject();
}

void CJobRunner::slotButtonClicked(int)
{
    if(itsIt!=itsEnd)
        itsCancelClicked=true;
}

//
// Tell the io-slave not to clear, and re-read, the list of fonts. And also, tell it to not
// automatically recreate the config files - we want that to happen after all fonts are installed,
// deleted, etc.
void CJobRunner::setMetaData(KIO::Job *job)
{
    job->addMetaData(KFI_KIO_TIMEOUT, "0");
    job->addMetaData(KFI_KIO_NO_CLEAR, "1");

    if(!Misc::root() && !itsPasswd.isEmpty())
        job->addMetaData(KFI_KIO_PASS, itsPasswd);
}

}

#include "JobRunner.moc"
