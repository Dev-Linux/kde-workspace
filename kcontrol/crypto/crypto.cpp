/**
 * crypto.cpp
 *
 * Copyright (c) 2000 George Staikos <staikos@kde.org>
 *               2000 Carsten Pfeiffer <pfeiffer@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

#include <qfile.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qbuttongroup.h>
#include <qhbuttongroup.h>
#include <qradiobutton.h>
#include <qwhatsthis.h>
#include <qpushbutton.h>
#include <qfileinfo.h>
#include <qcheckbox.h>

#include <kfiledialog.h>
#include <klineedit.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <klocale.h>
#include <kdialog.h>
#include <kmessagebox.h>

#include <qframe.h>

#include "crypto.h"

#ifdef HAVE_SSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif


CipherItem::CipherItem( QListView *view, const QString& cipher, int bits,
			int maxBits, KCryptoConfig *module )
    : QCheckListItem( view, QString::null, CheckBox )
{
    m_cipher = cipher;
    m_bits = bits;
    m_module = module;

    QString tmp( "%1 (%2 of %3 bits)" );
    setText( 0, tmp.arg( cipher ).arg( bits ).arg( maxBits ));
}

void CipherItem::stateChange( bool )
{
    m_module->configChanged();
}

QString CipherItem::configName() const
{
    QString cipherName("cipher_%1");
    return cipherName.arg( m_cipher );
}


KCryptoConfig::KCryptoConfig(QWidget *parent, const char *name)
  : KCModule(parent, name)
{
QGridLayout *grid;
QBoxLayout *top = new QVBoxLayout(this);
QString whatstr;

  ///////////////////////////////////////////////////////////////////////////
  // Create the GUI here - there are currently a total of 4 tabs.
  // The first is SSL and cipher related
  // The second is user's SSL certificate related
  // The third is other SSL certificate related
  // The fourth is SSL certificate authority related
  ///////////////////////////////////////////////////////////////////////////

  tabs = new QTabWidget(this);
  top->addWidget(tabs);

  ///////////////////////////////////////////////////////////////////////////
  // FIRST TAB
  ///////////////////////////////////////////////////////////////////////////
  tabSSL = new QFrame(this);
  grid = new QGridLayout(tabSSL, 5, 2, KDialog::marginHint(),
                                        KDialog::spacingHint() );
  mUseTLS = new QCheckBox(i18n("Use &TLS instead of SSLv2/v3"), tabSSL);
  connect(mUseTLS, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mUseTLS, 0, 0);
  whatstr = i18n("TLS is the proposed new implementation of secure sockets."
                "  Enabling TLS disables SSLv2 and SSLv3.  In most cases"
                " you should leave this turned off.");
  QWhatsThis::add(mUseTLS, whatstr);

  mUseSSLv2 = new QCheckBox(i18n("Enable SSLv&2"), tabSSL);
  connect(mUseSSLv2, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mUseSSLv2, 1, 0);
  whatstr = i18n("SSL v2 is the second revision of the SSL protocol."
                " It is most common to enable v2 and v3.");
  QWhatsThis::add(mUseSSLv2, whatstr);

  mUseSSLv3 = new QCheckBox(i18n("Enable SSLv&3"), tabSSL);
  connect(mUseSSLv3, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mUseSSLv3, 1, 1);
  whatstr = i18n("SSL v3 is the third revision of the SSL protocol."
                " It is most common to enable v2 and v3.");
  QWhatsThis::add(mUseSSLv3, whatstr);

#ifdef HAVE_SSL
  SSLv2Box = new QListView(tabSSL, "v2ciphers");
  (void) SSLv2Box->addColumn(i18n("SSLv2 Ciphers To Use:"));
  whatstr = i18n("Select the ciphers you wish to enable when using the"
                " SSL v2 protocol.  The actual protocol used will be"
                " negotiated with the server at connection time.");
  QWhatsThis::add(SSLv2Box, whatstr);
  SSLv2Box->setSelectionMode(QListView::NoSelection);

  grid->addWidget( SSLv2Box, 2, 0 );
  connect( mUseSSLv2, SIGNAL( toggled( bool ) ), 
	   SSLv2Box, SLOT( setEnabled( bool )));
#else
  QLabel *nossllabel = new QLabel(i18n("SSL ciphers cannot be configured"
                               " because this module was not linked"
                               " with OpenSSL."), tabSSL);
  grid->addWidget(nossllabel, 2, 0);
  grid->addRowSpacing( 3, 100 ); // give minimum height to look better
#endif

  // no need to parse kdeglobals.
  config = new KConfig("cryptodefaults", false, false);

#ifdef HAVE_SSL
  SSLv3Box = new QListView(tabSSL, "v3ciphers");
  (void) SSLv3Box->addColumn(i18n("SSLv3 Ciphers To Use:"));
  whatstr = i18n("Select the ciphers you wish to enable when using the"
                " SSL v3 protocol.  The actual protocol used will be"
                " negotiated with the server at connection time.");
  QWhatsThis::add(SSLv3Box, whatstr);
  SSLv3Box->setSelectionMode(QListView::NoSelection);
  grid->addWidget(SSLv3Box, 2, 1);
  connect( mUseSSLv3, SIGNAL( toggled( bool ) ), 
	   SSLv3Box, SLOT( setEnabled( bool )));

  loadCiphers();
#endif

  mWarnOnEnter = new QCheckBox(i18n("Warn on &entering SSL mode"), tabSSL);
  connect(mWarnOnEnter, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mWarnOnEnter, 3, 0);
  whatstr = i18n("If selected, you will be notified when entering an SSL"
                " enabled site");
  QWhatsThis::add(mWarnOnEnter, whatstr);

  mWarnOnLeave = new QCheckBox(i18n("Warn on &leaving SSL mode"), tabSSL);
  connect(mWarnOnLeave, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mWarnOnLeave, 3, 1);
  whatstr = i18n("If selected, you will be notified when leaving an SSL"
                " based site.");
  QWhatsThis::add(mWarnOnLeave, whatstr);

  mWarnOnUnencrypted = new QCheckBox(i18n("Warn on sending &unencrypted data"), tabSSL);
  connect(mWarnOnUnencrypted, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mWarnOnUnencrypted, 4, 0);
  whatstr = i18n("If selected, you will be notified before sending"
                " unencrypted data via a web browser.");
  QWhatsThis::add(mWarnOnUnencrypted, whatstr);

  mWarnOnMixed = new QCheckBox(i18n("Warn on &mixed SSL/non-SSL pages"), tabSSL);
  connect(mWarnOnMixed, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addWidget(mWarnOnMixed, 4, 1);
  whatstr = i18n("If selected, you will be notified if you view a page"
                " that has both encrypted and non-encrypted parts.");
  QWhatsThis::add(mWarnOnMixed, whatstr);

  ///////////////////////////////////////////////////////////////////////////
  // SECOND TAB
  ///////////////////////////////////////////////////////////////////////////
  tabYourSSLCert = new QFrame(this);

#ifdef HAVE_SSL
  grid = new QGridLayout(tabYourSSLCert, 10, 2, KDialog::marginHint(), KDialog::spacingHint() );

  yourSSLBox = new QListBox(tabYourSSLCert);
  whatstr = i18n("This list box shows which certificates of yours KDE"
                " knows about.  You can easily manage them from here.");
  QWhatsThis::add(yourSSLBox, whatstr);
  yourSSLBox->setSelectionMode(QListBox::Single);
  yourSSLBox->setColumnMode(QListBox::FixedNumber);
  grid->addMultiCellWidget(yourSSLBox, 0, 7, 0, 0);

  yourSSLImport = new QPushButton(i18n("&Import..."), tabYourSSLCert);
  //connect(yourSSLImport, SIGNAL(), SLOT());
  grid->addWidget(yourSSLImport, 0, 1);

  yourSSLView = new QPushButton(i18n("&View/Edit..."), tabYourSSLCert);
  //connect(yourSSLView, SIGNAL(), SLOT());
  grid->addWidget(yourSSLView, 1, 1);

  yourSSLRemove = new QPushButton(i18n("&Remove..."), tabYourSSLCert);
  //connect(yourSSLRemove, SIGNAL(), SLOT());
  grid->addWidget(yourSSLRemove, 2, 1);

  yourSSLExport = new QPushButton(i18n("&Export..."), tabYourSSLCert);
  //connect(yourSSLExport, SIGNAL(), SLOT());
  grid->addWidget(yourSSLExport, 3, 1);

  yourSSLDefault = new QPushButton(i18n("&Set Default..."), tabYourSSLCert);
  //connect(yourSSLDefault, SIGNAL(), SLOT());
  grid->addWidget(yourSSLDefault, 4, 1);

  yourSSLVerify = new QPushButton(i18n("Verif&y..."), tabYourSSLCert);
  //connect(yourSSLVerify, SIGNAL(), SLOT());
  grid->addWidget(yourSSLVerify, 5, 1);

  QHButtonGroup *ocbg = new QHButtonGroup(i18n("On SSL Connection..."), tabYourSSLCert);
  yourSSLUseDefault = new QRadioButton(i18n("&Use default certificate"), ocbg);
  yourSSLList = new QRadioButton(i18n("&List upon connection"), ocbg);
  yourSSLDont = new QRadioButton(i18n("&Do not use certificates"), ocbg);
  grid->addMultiCellWidget(ocbg, 9, 9, 0, 1);
#else
  nossllabel = new QLabel(i18n("SSL certificates cannot be managed"
                               " because this module was not linked"
                               " with OpenSSL."), tabYourSSLCert);
  grid->addMultiCellWidget(nossllabel, 3, 3, 0, 1);
#endif


  ///////////////////////////////////////////////////////////////////////////
  // THIRD TAB
  ///////////////////////////////////////////////////////////////////////////
  tabOtherSSLCert = new QFrame(this);

#ifdef HAVE_SSL
  grid = new QGridLayout(tabOtherSSLCert, 8, 2, KDialog::marginHint(), KDialog::spacingHint());

  otherSSLBox = new QListBox(tabOtherSSLCert);
  whatstr = i18n("This list box shows which site and person certificates KDE"
                " knows about.  You can easily manage them from here.");
  QWhatsThis::add(otherSSLBox, whatstr);
  otherSSLBox->setSelectionMode(QListBox::Single);
  otherSSLBox->setColumnMode(QListBox::FixedNumber);
  grid->addMultiCellWidget(otherSSLBox, 0, 7, 0, 0);

  otherSSLImport = new QPushButton(i18n("&Import..."), tabOtherSSLCert);
  //connect(otherSSLImport, SIGNAL(), SLOT());
  grid->addWidget(otherSSLImport, 0, 1);

  otherSSLView = new QPushButton(i18n("&View/Edit..."), tabOtherSSLCert);
  //connect(otherSSLView, SIGNAL(), SLOT());
  grid->addWidget(otherSSLView, 1, 1);

  otherSSLRemove = new QPushButton(i18n("&Remove..."), tabOtherSSLCert);
  //connect(otherSSLRemove, SIGNAL(), SLOT());
  grid->addWidget(otherSSLRemove, 2, 1);

  otherSSLVerify = new QPushButton(i18n("Verif&y..."), tabOtherSSLCert);
  //connect(otherSSLVerify, SIGNAL(), SLOT());
  grid->addWidget(otherSSLVerify, 3, 1);

#else
  nossllabel = new QLabel(i18n("SSL certificates cannot be managed"
                               " because this module was not linked"
                               " with OpenSSL."), tabOtherSSLCert);
  grid->addMultiCellWidget(nossllabel, 1, 1, 0, 1);
#endif


  ///////////////////////////////////////////////////////////////////////////
  // FOURTH TAB
  ///////////////////////////////////////////////////////////////////////////
  tabSSLCA = new QFrame(this);

#ifdef HAVE_SSL
  grid = new QGridLayout(tabSSLCA, 8, 2, KDialog::marginHint(), KDialog::spacingHint());

  caSSLBox = new QListBox(tabSSLCA);
  whatstr = i18n("This list box shows which certificate authorities KDE"
                " knows about.  You can easily manage them from here.");
  QWhatsThis::add(caSSLBox, whatstr);
  caSSLBox->setSelectionMode(QListBox::Single);
  caSSLBox->setColumnMode(QListBox::FixedNumber);
  grid->addMultiCellWidget(caSSLBox, 0, 7, 0, 0);

  caSSLImport = new QPushButton(i18n("&Import..."), tabSSLCA);
  //connect(caSSLImport, SIGNAL(), SLOT());
  grid->addWidget(caSSLImport, 0, 1);

  caSSLView = new QPushButton(i18n("&View/Edit..."), tabSSLCA);
  //connect(caSSLView, SIGNAL(), SLOT());
  grid->addWidget(caSSLView, 1, 1);

  caSSLRemove = new QPushButton(i18n("&Remove..."), tabSSLCA);
  //connect(caSSLRemove, SIGNAL(), SLOT());
  grid->addWidget(caSSLRemove, 2, 1);

  caSSLVerify = new QPushButton(i18n("Verif&y..."), tabSSLCA);
  //connect(caSSLVerify, SIGNAL(), SLOT());
  grid->addWidget(caSSLVerify, 3, 1);

#else
  nossllabel = new QLabel(i18n("SSL certificates cannot be managed"
                               " because this module was not linked"
                               " with OpenSSL."), tabSSLCA);
  grid->addMultiCellWidget(nossllabel, 1, 1, 0, 1);
#endif

  ///////////////////////////////////////////////////////////////////////////
  // FIFTH TAB
  ///////////////////////////////////////////////////////////////////////////
  tabSSLCOpts = new QFrame(this);

#ifdef HAVE_SSL
  grid = new QGridLayout(tabSSLCOpts, 9, 4, KDialog::marginHint(), KDialog::spacingHint());
  mWarnSelfSigned = new QCheckBox(i18n("Warn on &self signed certificates or unknown CA's"), tabSSLCOpts);
  connect(mWarnSelfSigned, SIGNAL(clicked()), SLOT(configChanged()));
  mWarnExpired = new QCheckBox(i18n("Warn on &expired certificates"), tabSSLCOpts);
  connect(mWarnExpired, SIGNAL(clicked()), SLOT(configChanged()));
  mWarnRevoked = new QCheckBox(i18n("Warn on re&voked certificates"), tabSSLCOpts);
  connect(mWarnRevoked, SIGNAL(clicked()), SLOT(configChanged()));
  grid->addMultiCellWidget(mWarnSelfSigned, 0, 0, 0, 3);
  grid->addMultiCellWidget(mWarnExpired, 1, 1, 0, 3);
  grid->addMultiCellWidget(mWarnRevoked, 2, 2, 0, 3);

  macCert = new QLineEdit(tabSSLCOpts);
  grid->addMultiCellWidget(macCert, 4, 4, 0, 2);

  macBox = new QListBox(tabSSLCOpts);
  whatstr = i18n("This list box shows which sites you have decided to accept"
                " a certificate from even though the certificate might fail"
                " the validation procedure.");
  QWhatsThis::add(macBox, whatstr);
  caSSLBox->setSelectionMode(QListBox::Single);
  caSSLBox->setColumnMode(QListBox::FixedNumber);
  grid->addMultiCellWidget(macBox, 5, 8, 0, 2);

  macAdd = new QPushButton(i18n("&Add"), tabSSLCOpts);
  //connect(macAdd, SIGNAL(), SLOT());
  grid->addWidget(macAdd, 4, 3);

  macRemove = new QPushButton(i18n("&Remove"), tabSSLCOpts);
  //connect(macRemove, SIGNAL(), SLOT());
  grid->addWidget(macRemove, 5, 3);

  macClear = new QPushButton(i18n("&Clear"), tabSSLCOpts);
  //connect(macAdd, SIGNAL(), SLOT());
  grid->addWidget(macClear, 6, 3);

#else
  nossllabel = new QLabel(i18n("These options are not configurable"
                               " because this module was not linked"
                               " with OpenSSL."), tabSSLCOpts);
  grid->addMultiCellWidget(nossllabel, 1, 1, 0, 1);
#endif

  ///////////////////////////////////////////////////////////////////////////
  // Add the tabs and startup
  ///////////////////////////////////////////////////////////////////////////
  tabs->addTab(tabSSL, i18n("SSL"));
  tabs->addTab(tabYourSSLCert, i18n("Your SSL Certificates"));
  tabs->addTab(tabOtherSSLCert, i18n("Other SSL Certificates"));
  tabs->addTab(tabSSLCA, i18n("SSL C.A.s"));
  tabs->addTab(tabSSLCOpts, i18n("Validation Options"));

  tabs->resize(tabs->sizeHint());
  load();
}

KCryptoConfig::~KCryptoConfig()
{
    delete config;
}

void KCryptoConfig::configChanged()
{
    emit changed(true);
}


void KCryptoConfig::load()
{
#ifdef HAVE_SSL
  config->setGroup("TLSv1");
  mUseTLS->setChecked(config->readBoolEntry("Enabled", false));

  config->setGroup("SSLv2");
  mUseSSLv2->setChecked(config->readBoolEntry("Enabled", true));

  config->setGroup("SSLv3");
  mUseSSLv3->setChecked(config->readBoolEntry("Enabled", true));

  config->setGroup("Warnings");
  mWarnOnEnter->setChecked(config->readBoolEntry("OnEnter", false));
  mWarnOnLeave->setChecked(config->readBoolEntry("OnLeave", true));
  mWarnOnUnencrypted->setChecked(config->readBoolEntry("OnUnencrypted", false));
  mWarnOnMixed->setChecked(config->readBoolEntry("OnMixed", true));

  config->setGroup("Validation");
  mWarnSelfSigned->setChecked(config->readBoolEntry("WarnSelfSigned", true));
  mWarnExpired->setChecked(config->readBoolEntry("WarnExpired", true));
  mWarnRevoked->setChecked(config->readBoolEntry("WarnRevoked", true));

  config->setGroup("SSLv2");
  CipherItem *item = static_cast<CipherItem *>(SSLv2Box->firstChild());
  while ( item ) {
      item->setOn(config->readBoolEntry(item->configName(),
					item->bits() >= 40));
      item = static_cast<CipherItem *>(item->nextSibling());
  }

  config->setGroup("SSLv3");
  item = static_cast<CipherItem *>(SSLv3Box->firstChild());
  while ( item ) {
      item->setOn(config->readBoolEntry(item->configName(),
					item->bits() >= 40));
      item = static_cast<CipherItem *>(item->nextSibling());
  }

  SSLv2Box->setEnabled( mUseSSLv2->isChecked() );
  SSLv3Box->setEnabled( mUseSSLv3->isChecked() );
#endif

  emit changed(false);
}

void KCryptoConfig::save()
{
#ifdef HAVE_SSL
  if (!mUseTLS->isChecked() &&
      !mUseSSLv2->isChecked() &&
      !mUseSSLv3->isChecked())
    KMessageBox::information(this, i18n("If you don't select at least one"
                                       " SSL algorithm, either SSL will not"
                                       " work or the application may be"
                                       " forced to choose a suitable default."),
                                   i18n("SSL"));

  config->setGroup("TLSv1");
  config->writeEntry("Enabled", mUseTLS->isChecked());

  config->setGroup("SSLv2");
  config->writeEntry("Enabled", mUseSSLv2->isChecked());

  config->setGroup("SSLv3");
  config->writeEntry("Enabled", mUseSSLv3->isChecked());

  config->setGroup("Warnings");
  config->writeEntry("OnEnter", mWarnOnEnter->isChecked());
  config->writeEntry("OnLeave", mWarnOnLeave->isChecked());
  config->writeEntry("OnUnencrypted", mWarnOnUnencrypted->isChecked());
  config->writeEntry("OnMixed", mWarnOnMixed->isChecked());

  config->setGroup("Validation");
  config->writeEntry("WarnSelfSigned", mWarnSelfSigned->isChecked());
  config->writeEntry("WarnExpired", mWarnExpired->isChecked());
  config->writeEntry("WarnRevoked", mWarnRevoked->isChecked());

  int ciphercount = 0;
  config->setGroup("SSLv2");
  CipherItem *item = static_cast<CipherItem *>(SSLv2Box->firstChild());
  while ( item ) {
    if (item->isOn()) {
      config->writeEntry(item->configName(), true);
      ciphercount++;
    } else config->writeEntry(item->configName(), false);

    item = static_cast<CipherItem *>(item->nextSibling());
  }

  if (mUseSSLv2->isChecked() && ciphercount == 0)
    KMessageBox::information(this, i18n("If you don't select at least one"
                                       " cipher, SSLv2 will not work."),
                                   i18n("SSLv2 Ciphers"));

  ciphercount = 0;
  config->setGroup("SSLv3");
  item = static_cast<CipherItem *>(SSLv3Box->firstChild());
  while ( item ) {
    if (item->isOn()) {
      config->writeEntry(item->configName(), true);
      ciphercount++;
    } else config->writeEntry(item->configName(), false);

    item = static_cast<CipherItem *>(item->nextSibling());
  }

  if (mUseSSLv3->isChecked() && ciphercount == 0)
    KMessageBox::information(this, i18n("If you don't select at least one"
                                       " cipher, SSLv3 will not work."),
                                   i18n("SSLv3 Ciphers"));
#endif

  config->sync();

  // insure proper permissions -- contains sensitive data
  QString cfgName(KGlobal::dirs()->findResource("config", "cryptodefaults"));
  if (!cfgName.isEmpty())
    ::chmod(QFile::encodeName(cfgName), 0600);

  emit changed(false);
}

void KCryptoConfig::defaults()
{
  mUseTLS->setChecked(false);
  mUseSSLv2->setChecked(true);
  mUseSSLv3->setChecked(true);
  mWarnOnEnter->setChecked(false);
  mWarnOnLeave->setChecked(true);
  mWarnOnUnencrypted->setChecked(false);
  mWarnOnMixed->setChecked(true);
  mWarnSelfSigned->setChecked(true);
  mWarnExpired->setChecked(true);
  mWarnRevoked->setChecked(true);

#ifdef HAVE_SSL
    // We don't want to make
    // ciphers < 40 bit a default selection.  This is very unsafe and
    // I have already witnessed OpenSSL negotiate a 0 bit connection
    // on me after tracing the https ioslave on a suspicion.

  CipherItem *item;
  for ( item = static_cast<CipherItem *>(SSLv2Box->firstChild()); item;
	item = static_cast<CipherItem *>(item->nextSibling()) ) {
    item->setOn( item->bits() >= 40 );
  }

  for ( item = static_cast<CipherItem *>(SSLv3Box->firstChild()); item;
	item = static_cast<CipherItem *>(item->nextSibling()) ) {
    item->setOn( item->bits() >= 40 );
  }
#endif

  emit changed(true);
}

QString KCryptoConfig::quickHelp() const
{
  return i18n("<h1>crypto</h1> This module allows you to configure SSL for"
     " use with most KDE applications, as well as manage your personal"
     " certificates and the known certificate authorities.");
}

#ifdef HAVE_SSL
// This gets all the available ciphers from OpenSSL
bool KCryptoConfig::loadCiphers() {
unsigned int i;
SSL_CTX *ctx;
SSL *ssl;
SSL_METHOD *meth;

  SSLv2Box->clear(); 
  SSLv3Box->clear(); 
 
  meth = SSLv2_client_method();
  SSLeay_add_ssl_algorithms();
  ctx = SSL_CTX_new(meth);
  if (ctx == NULL) return false;

  ssl = SSL_new(ctx);
  if (!ssl) return false;

  CipherItem *item;
  for (i=0; ; i++) {
    int j, k;
    SSL_CIPHER *sc;
    QString cname;
    sc = (meth->get_cipher)(i);
    if (!sc) break;
    k = SSL_CIPHER_get_bits(sc, &j);

    item = new CipherItem( SSLv2Box, sc->name, k, j, this );
  }

  if (ctx) SSL_CTX_free(ctx);
  if (ssl) SSL_free(ssl);

  // We repeat for SSLv3
  meth = SSLv3_client_method();
  SSLeay_add_ssl_algorithms();
  ctx = SSL_CTX_new(meth);
  if (ctx == NULL) return false;

  ssl = SSL_new(ctx);
  if (!ssl) return false;

  for (i=0; ; i++) {
    int j, k;
    SSL_CIPHER *sc;
    QString cname;
    sc = (meth->get_cipher)(i);
    if (!sc) break;
    k = SSL_CIPHER_get_bits(sc, &j);

    item = new CipherItem( SSLv3Box, sc->name, k, j, this );
  }

  if (ctx) SSL_CTX_free(ctx);
  if (ssl) SSL_free(ssl);

return true;
}
#endif

extern "C"
{
  KCModule *create_crypto(QWidget *parent, const char *name)
  {
    KGlobal::locale()->insertCatalogue("kcmcrypto");
    return new KCryptoConfig(parent, name);
  };
}
#include "crypto.moc"
