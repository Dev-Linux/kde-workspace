/* vi: ts=8 sts=4 sw=4
 *
 * $Id$
 *
 * This file is part of the KDE project, module kcontrol.
 * Copyright (C) 1999-2001 by Alex Zepeda and Daniel Molkentin
 *               2001-2002 Michael Haeckel <haeckel@kde.org>
 *
 * You can freely distribute this program under the following terms:
 *
 * 1.) If you use this program in a product outside of KDE, you must
 * credit the authors, and must mention "kcmemail".
 *
 * 2.) If this program is used in a product other than KDE, this license
 * must be reproduced in its entirety in the documentation and/or some
 * other highly visible location (e.g. startup splash screen)
 *
 * 3.) Use of this product implies the following terms:
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <pwd.h>
#include <unistd.h>

#include <qfile.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>

#include <kdebug.h>
#include <klineedit.h>
#include <kcombobox.h>
#include <kdialog.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kemailsettings.h>
#include <kgenericfactory.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kconfig.h>
#include <dcopclient.h>

#include "email.h"

typedef KGenericFactory<topKCMEmail, QWidget> EMailFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_email, EMailFactory("kcmemail") );

topKCMEmail::topKCMEmail (QWidget *parent,  const char *name, const QStringList &)
	: KCModule(EMailFactory::instance(), parent, name)
{
	QVBoxLayout *topLayout = new QVBoxLayout(this, 0, KDialog::spacingHint());
	m_email = new KCMEmailBase(this);
	topLayout->add(m_email);

 	connect(m_email->cmbCurProfile, SIGNAL(activated(const QString &)), this, SLOT(slotComboChanged(const QString &)) );
	connect(m_email->btnNewProfile, SIGNAL(clicked()), SLOT(slotNewProfile()) );

	connect(m_email->txtFullName,SIGNAL(textChanged(const QString&)), SLOT(configChanged()) );
	connect(m_email->txtOrganization, SIGNAL(textChanged(const QString&)), SLOT(configChanged()) );
	connect(m_email->txtEMailAddr, SIGNAL(textChanged(const QString&)), SLOT(configChanged()) );
	connect(m_email->txtReplyTo, SIGNAL(textChanged(const QString&)), SLOT(configChanged()) );
	connect(m_email->txtSMTP, SIGNAL(textChanged(const QString&)), SLOT(configChanged()) );

	pSettings = new KEMailSettings();
	m_email->lblCurrentProfile->hide();
	m_email->cmbCurProfile->hide();
	m_email->btnNewProfile->hide();
	load();

	mAboutData = new KAboutData("kcmemail", I18N_NOOP("KDE Email Control Module"), "1.0",
	I18N_NOOP("Configure your identity, email addresses, mail servers, etc."),
		KAboutData::License_Custom);
	mAboutData->addAuthor("Michael H\303\244ckel", I18N_NOOP("Current maintainer"), "haeckel@kde.org" );
	mAboutData->addAuthor("Daniel Molkentin");
	mAboutData->addAuthor("Alex Zepeda");
	mAboutData->setLicenseText(
	"You can freely distribute this program under the following terms:\n"
	"\n"
	"1.) If you use this program in a product outside of KDE, you must\n"
	"credit the authors, and must mention \"kcmemail\".\n"
	"\n"
	"2.) If this program is used in a product other than KDE, this license\n"
	"must be reproduced in its entirety in the documentation and/or some\n"
	"other highly visible location (e.g. startup splash screen)\n"
	"\n"
	"3.) Use of this product implies the following terms:\n"
	"\n"
	"THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND\n"
	"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
	"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
	"ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE\n"
	"FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n"
	"DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n"
	"OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)\n"
	"HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT\n"
	"LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY\n"
	"OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n"
	"SUCH DAMAGE.");

	if ( m_email->txtFullName->text().isEmpty() &&
	     m_email->txtOrganization->text().isEmpty() &&
	     m_email->txtEMailAddr->text().isEmpty() &&
	     m_email->txtReplyTo->text().isEmpty() &&
	     m_email->txtSMTP->text().isEmpty() )
	{
		KConfigGroup config( EMailFactory::instance()->config(), 
				     "General" );
		if ( config.readBoolEntry( "FirstStart", true ) )
			defaults();
	}

	m_email->txtFullName->setFocus();
}

topKCMEmail::~topKCMEmail()
{
	delete mAboutData;
}

const KAboutData * topKCMEmail::aboutData() const
{
	return mAboutData;
}

void topKCMEmail::load()
{
	m_email->cmbCurProfile->clear();
	load(QString::null);
}

void topKCMEmail::load(const QString &s)
{
	if (s.isNull()) {
		m_email->cmbCurProfile->insertStringList(pSettings->profiles());
		if (!pSettings->defaultProfileName().isNull()) {
			kdDebug() << "prfile is: " << pSettings->defaultProfileName()<< "line is " << __LINE__ << endl;
			load(pSettings->defaultProfileName());
		} else {
			if (m_email->cmbCurProfile->count()) {
				//pSettings->setProfile(m_email->cmbCurProfile->text(0));
				kdDebug() << "prfile is: " << m_email->cmbCurProfile->text(0) << endl;
				load(m_email->cmbCurProfile->text(0));
				//pSettings->setDefault(m_email->cmbCurProfile->text(0));
			} else {
				m_email->cmbCurProfile->insertItem(i18n("Default"));
				pSettings->setProfile(i18n("Default"));
				pSettings->setDefault(i18n("Default"));
			}
		}
	} else {
		pSettings->setProfile(s);
		m_email->txtEMailAddr->setText(pSettings->getSetting(KEMailSettings::EmailAddress));
		m_email->txtReplyTo->setText(pSettings->getSetting(KEMailSettings::ReplyToAddress));
		m_email->txtOrganization->setText(pSettings->getSetting(KEMailSettings::Organization));
		m_email->txtFullName->setText(pSettings->getSetting(KEMailSettings::RealName));
		m_email->txtSMTP->setText(pSettings->getSetting(KEMailSettings::OutServer));
		configChanged(false);
	}

}

void topKCMEmail::clearData()
{
	m_email->txtEMailAddr->setText(QString::null);
	m_email->txtReplyTo->setText(QString::null);
	m_email->txtOrganization->setText(QString::null);
	m_email->txtFullName->setText(QString::null);
	m_email->txtSMTP->setText(QString::null);

	configChanged(false);
}

void topKCMEmail::slotNewProfile()
{
	KDialog *dlgAskName = new KDialog(this, "noname", true);
	dlgAskName->setCaption(i18n("New Email Profile"));

	QVBoxLayout *vlayout = new QVBoxLayout(dlgAskName, KDialog::marginHint(), KDialog::spacingHint());

	QHBoxLayout *layout = new QHBoxLayout(vlayout);

	QLabel *lblName = new QLabel(dlgAskName);
	lblName->setText(i18n("Name:"));
	lblName->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
	KLineEdit *txtName = new KLineEdit(dlgAskName);
	lblName->setBuddy(txtName);
	layout->addWidget(lblName);
	layout->addWidget(txtName);

	layout = new QHBoxLayout(vlayout);
	QPushButton *btnOK = new QPushButton(dlgAskName);
	btnOK->setText(i18n("&OK"));
	btnOK->setFixedSize(btnOK->sizeHint());
	QPushButton *btnCancel = new QPushButton(dlgAskName);
	btnCancel->setText(i18n("&Cancel"));
	btnCancel->setFixedSize(btnCancel->sizeHint());
	layout->addWidget(btnOK);
	layout->addWidget(btnCancel);
	connect(btnOK, SIGNAL(clicked()), dlgAskName, SLOT(accept()));
	connect(btnCancel, SIGNAL(clicked()), dlgAskName, SLOT(reject()));
        connect(txtName, SIGNAL(returnPressed ()),dlgAskName, SLOT(accept()));
	txtName->setFocus();

	if (dlgAskName->exec() == QDialog::Accepted) {
		if (txtName->text().isEmpty()) {
			KMessageBox::sorry(this, i18n("Please enter a name for the profile."));
		} else if (m_email->cmbCurProfile->currentText().contains(txtName->text()))
			KMessageBox::sorry(this, i18n("This email profile already exists, and cannot be created again."));
		else {
			pSettings->setProfile(txtName->text());
			m_email->cmbCurProfile->insertItem(txtName->text());
			// should probbaly load defaults instead
			clearData();
                        //you add new profile so you change config
                        configChanged();
			m_email->cmbCurProfile->setCurrentItem(m_email->cmbCurProfile->count()-1);
		}
	} else { // rejected
	}

	delete dlgAskName;
}

void topKCMEmail::configChanged(bool c)
{
	emit changed(c);
	m_bChanged=c;
}

void topKCMEmail::configChanged()
{
	configChanged(true);
}

void topKCMEmail::save()
{
	pSettings->setProfile(m_email->cmbCurProfile->text(m_email->cmbCurProfile->currentItem()));
	pSettings->setSetting(KEMailSettings::RealName, m_email->txtFullName->text());
	pSettings->setSetting(KEMailSettings::EmailAddress, m_email->txtEMailAddr->text());
	pSettings->setSetting(KEMailSettings::Organization, m_email->txtOrganization->text());
	pSettings->setSetting(KEMailSettings::ReplyToAddress, m_email->txtReplyTo->text());
	pSettings->setSetting(KEMailSettings::OutServer, m_email->txtSMTP->text());

	// insure proper permissions -- contains sensitive data
	QString cfgName(KGlobal::dirs()->findResource("config", "emaildefaults"));
	if (!cfgName.isEmpty())
		::chmod(QFile::encodeName(cfgName), 0600);
        
	KConfigGroup config( EMailFactory::instance()->config(), "General" );
	config.writeEntry( "FirstStart", false );

	configChanged(false);

	kapp->dcopClient()->emitDCOPSignal("KDE_emailSettingsChanged()", QByteArray());
}

void topKCMEmail::defaults()
{
	char hostname[80];
	struct passwd *p;

	p = getpwuid(getuid());
	gethostname(hostname, 80);

	m_email->txtFullName->setText(QString::fromLocal8Bit(p->pw_gecos));
        m_email->txtOrganization->setText(QString::null);
        m_email->txtReplyTo->setText(QString::null);
        m_email->txtSMTP->setText(QString::null);

	QString tmp = QString::fromLocal8Bit(p->pw_name);
	tmp += "@"; tmp += hostname;

	m_email->txtEMailAddr->setText(tmp);

	configChanged();
}

QString topKCMEmail::quickHelp() const
{
	return i18n("<h1>Email</h1> This module allows you to enter basic email"
	            " information for the current user. The information here is used,"
	            " among other things, for sending bug reports to the KDE developers"
	            " when you use the bug report dialog.<p>"
	            " Note that email programs like KMail and Empath offer many more"
	            " features, but they provide their own configuration facilities.");
}



void topKCMEmail::profileChanged(const QString &s)
{
	save(); // Save our changes...
	load(s);
}

void topKCMEmail::slotComboChanged(const QString &name)
{
	if (m_bChanged) {
		if (KMessageBox::warningYesNo(this, i18n("Do you wish to discard changes to the current profile?")) == KMessageBox::No) {
			if (KMessageBox::warningYesNo(this, i18n("Do you wish to save changes to the current profile?")) == KMessageBox::Yes) {
				save();
			} else {
				int keep=-1;
				for (int i=0; i < m_email->cmbCurProfile->count(); i++) {
					if (m_email->cmbCurProfile->text(i) == pSettings->currentProfileName()) {
						keep=i; break;
					}
				}
				if (keep != -1)
					m_email->cmbCurProfile->setCurrentItem(keep);
				return;
			}
		}
	}
	load(name);
}


#include "email.moc"
