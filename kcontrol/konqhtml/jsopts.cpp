// (c) Martin R. Jones 1996
// (c) Bernd Wuebben 1998
// KControl port & modifications
// (c) Torben Weis 1998
// End of the KControl port, added 'kfmclient configure' call.
// (c) David Faure 1998
// New configuration scheme for JavaScript
// (C) Kalle Dalheimer 2000
// Major cleanup & Java/JS settings splitted
// (c) Daniel Molkentin 2000

#include <kfiledialog.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qmessagebox.h>
#include <qwhatsthis.h>
#include <qvgroupbox.h>
#include <qhbox.h>
#include <qvbox.h>
#include <kglobal.h>
#include <kglobalsettings.h>
#include <kconfig.h>
#include <klistview.h>
#include <kmessagebox.h>
#include <qlabel.h>
#include <kcharsets.h>
#include <qspinbox.h>
#include <kdebug.h>
#include <kurlrequester.h>
#include <X11/Xlib.h>
#include <klineedit.h>

#include "htmlopts.h"
#include "policydlg.h"

#include <konq_defaults.h> // include default values directly from konqueror
#include <klocale.h>
#include <khtml_settings.h>
#include <khtmldefaults.h>

#include "jsopts.h"

#include "jsopts.moc"

KJavaScriptOptions::KJavaScriptOptions( KConfig* config, QString group, QWidget *parent,
										const char *name ) :
  KCModule( parent, name ), m_pConfig( config ), m_groupname( group )
{
  QVBoxLayout* toplevel = new QVBoxLayout( this, 10, 5 );

  // the global checkbox
  QVGroupBox* globalGB = new QVGroupBox( i18n( "Global Settings" ), this );
  toplevel->addWidget( globalGB );
  enableJavaScriptGloballyCB = new QCheckBox( i18n( "Enable Java&Script globally" ), globalGB );
  QWhatsThis::add( enableJavaScriptGloballyCB, i18n("Enables the execution of scripts written in ECMA-Script "
        "(as known as JavaScript) that can be contained in HTML pages. Be aware that JavaScript support "
        "is not yet finished. Note that, as with any browser, enabling scripting languages can be a security problem.") );
  connect( enableJavaScriptGloballyCB, SIGNAL( clicked() ), this, SLOT( changed() ) );

  // the domain-specific listview (copied and modified from Cookies configuration)
  QVGroupBox* domainSpecificGB = new QVGroupBox( i18n( "Domain-specific" ), this );
  toplevel->addWidget( domainSpecificGB, 2 );
  QHBox* domainSpecificHB = new QHBox( domainSpecificGB );
  domainSpecificHB->setSpacing( 10 );
  domainSpecificLV = new KListView( domainSpecificHB );
  domainSpecificLV->addColumn(i18n("Host/Domain"));
  domainSpecificLV->addColumn(i18n("Policy"), 100);
  QString wtstr = i18n("This box contains the domains and hosts you have set "
                       "a specific JavaScript policy for. This policy will be used "
                       "instead of the default policy for enabling or disabling JavaScript on pages sent by these "
                       "domains or hosts. <p>Select a policy and use the controls on "
                       "the right to modify it.");
  QWhatsThis::add( domainSpecificLV, wtstr );
  QWhatsThis::add( domainSpecificGB, wtstr );

  QVBox* domainSpecificVB = new QVBox( domainSpecificHB );
  domainSpecificVB->setSpacing( 10 );
  QPushButton* addDomainPB = new QPushButton( i18n("Add..."), domainSpecificVB );
  QWhatsThis::add( addDomainPB, i18n("Click on this button to manually add a host or domain "
                                     "specific policy.") );
  connect( addDomainPB, SIGNAL(clicked()), SLOT( addPressed() ) );

  QPushButton* changeDomainPB = new QPushButton( i18n("Change..."), domainSpecificVB );
  QWhatsThis::add( changeDomainPB, i18n("Click on this button to change the policy for the "
                                        "host or domain selected in the list box.") );
  connect( changeDomainPB, SIGNAL( clicked() ), this, SLOT( changePressed() ) );

  QPushButton* deleteDomainPB = new QPushButton( i18n("Delete"), domainSpecificVB );
  QWhatsThis::add( deleteDomainPB, i18n("Click on this button to change the policy for the "
                                        "host or domain selected in the list box.") );
  connect( deleteDomainPB, SIGNAL( clicked() ), this, SLOT( deletePressed() ) );

  QPushButton* importDomainPB = new QPushButton( i18n("Import..."), domainSpecificVB );
  QWhatsThis::add( importDomainPB, i18n("Click this button to choose the file that contains "
                                        "the JavaScript policies.  These policies will be merged "
                                        "with the exisiting ones.  Duplicate entries are ignored.") );
  connect( importDomainPB, SIGNAL( clicked() ), this, SLOT( importPressed() ) );
  importDomainPB->setEnabled( false );

  QPushButton* exportDomainPB = new QPushButton( i18n("Export..."), domainSpecificVB );
  QWhatsThis::add( exportDomainPB, i18n("Click this button to save the JavaScript policy to a zipped "
                                        "file.  The file, named <b>javascript_policy.tgz</b>, will be "
                                        "saved to a location of your choice." ) );

  connect( exportDomainPB, SIGNAL( clicked() ), this, SLOT( exportPressed() ) );
  exportDomainPB->setEnabled( false );

  QWhatsThis::add( domainSpecificGB, i18n("Here you can set specific JavaScript policies for any particular "
                                          "host or domain. To add a new policy, simply click the <i>Add...</i> "
                                          "button and supply the necessary information requested by the "
                                          "dialog box. To change an existing policy, click on the <i>Change...</i> "
                                          "button and choose the new policy from the policy dialog box.  Clicking "
                                          "on the <i>Delete</i> button will remove the selected policy causing the default "
                                          "policy setting to be used for that domain. The <i>Import</i> and <i>Export</i> "
                                          "button allows you to easily share your policies with other people by allowing "
                                          "you to save and retrive them from a zipped file.") );

  QVGroupBox* miscSettingsGB = new QVGroupBox( i18n( "Miscellaneous JavaScript Settings" ), this );
  toplevel->addWidget( miscSettingsGB );

  disableWindowOpenCB = new QCheckBox( i18n( "Disable \"window.open()\"" ), miscSettingsGB );
  QWhatsThis::add( disableWindowOpenCB, i18n("If you disable this option, Konqueror will stop interpreting the <i>window.open()</i> "
                                             "command. This is usefull if you regulary visit sites that make extensive use of this "
                                             "command to pop up ad banner.<br><br><b>Note:</b> Disableling this option might also "
                                             "break certain sites that require <i>window.open()</b> for proper operation. Use this "
                                             "feature carefully!") );
  connect( disableWindowOpenCB, SIGNAL( clicked() ), this, SLOT( changed() ) );

  enableDebugOutputCB = new QCheckBox( i18n( "Show debugger window" ), miscSettingsGB);
  QWhatsThis::add( enableDebugOutputCB, i18n("Show a window with informations and warnings issued by the JavaScript interpreter. "
                                             "This is extremely useful for both debugging your own html pages and tracing down "
                                             "problems with Konquerors JavaScript support.") );
  connect( enableDebugOutputCB, SIGNAL( clicked() ), this, SLOT( changed() ) );


  // Finally do the loading
  load();
}


void KJavaScriptOptions::load()
{
    // *** load ***
    m_pConfig->setGroup(m_groupname);

    if( m_pConfig->hasKey( "ECMADomainSettings" ) )
        updateDomainList( m_pConfig->readListEntry( "ECMADomainSettings" ) );
    else
        updateDomainList(m_pConfig->readListEntry("JavaScriptDomainAdvice") );

    // *** apply to GUI ***
    enableJavaScriptGloballyCB->setChecked( bJavaScriptGlobal );
}

void KJavaScriptOptions::defaults()
{
  enableJavaScriptGloballyCB->setChecked( false );
}

void KJavaScriptOptions::save()
{
    m_pConfig->setGroup(m_groupname);
    m_pConfig->writeEntry( "EnableJavaScript", enableJavaScriptGloballyCB->isChecked());

    QStringList domainConfig;
    QListViewItemIterator it( domainSpecificLV );
    QListViewItem* current;
    while( ( current = it.current() ) ) {
        ++it;
        QCString javaPolicy = KHTMLSettings::adviceToStr( KHTMLSettings::KJavaScriptDunno );
        QCString javaScriptPolicy = KHTMLSettings::adviceToStr(
                         (KHTMLSettings::KJavaScriptAdvice) javaScriptDomainPolicy[current] );

        domainConfig.append(QString::fromLatin1("%1:%2:%3").arg(current->text(0)).arg(javaPolicy).arg(javaScriptPolicy));
    }
    if( domainConfig.count() > 0 )
        m_pConfig->writeEntry("ECMADomainSettings", domainConfig);

    m_pConfig->sync();
}

void KJavaScriptOptions::changed()
{
  emit KCModule::changed(true);
}

void KJavaScriptOptions::addPressed()
{
    PolicyDialog pDlg( true, false, this );
    int def_javapolicy = KHTMLSettings::KJavaScriptDunno;
    int def_javascriptpolicy = KHTMLSettings::KJavaScriptReject;
    pDlg.setDefaultPolicy( def_javapolicy, def_javascriptpolicy );
    pDlg.setCaption( i18n( "New JavaScript Policy" ) );
    if( pDlg.exec() ) {
        QListViewItem* index = new QListViewItem( domainSpecificLV, pDlg.domain(),
                                                  KHTMLSettings::adviceToStr( (KHTMLSettings::KJavaScriptAdvice)
                                                                              pDlg.javaScriptPolicyAdvice() ) );
        javaScriptDomainPolicy.insert( index, (KHTMLSettings::KJavaScriptAdvice)pDlg.javaScriptPolicyAdvice());
        domainSpecificLV->setCurrentItem( index );
        changed();
    }
}

void KJavaScriptOptions::changePressed()
{
    QListViewItem *index = domainSpecificLV->currentItem();
    if ( index == 0 )
    {
        KMessageBox::information( 0, i18n("You must first select a policy to be changed!" ) );
        return;
    }

    int javaScriptAdvice = javaScriptDomainPolicy[index];

    PolicyDialog pDlg( true, false, this );
    pDlg.setDisableEdit( false, index->text(0) );
    pDlg.setCaption( i18n( "Change JavaScript Policy" ) );
    pDlg.setDefaultPolicy( KHTMLSettings::KJavaScriptDunno, javaScriptAdvice );
    if( pDlg.exec() )
    {
        javaScriptDomainPolicy[index] = pDlg.javaScriptPolicyAdvice();
        index->setText(2, i18n(KHTMLSettings::adviceToStr(
                (KHTMLSettings::KJavaScriptAdvice)javaScriptDomainPolicy[index])));
        changed();
    }
}

void KJavaScriptOptions::deletePressed()
{
    QListViewItem *index = domainSpecificLV->currentItem();
    if ( index == 0 )
    {
        KMessageBox::information( 0, i18n("You must first select a policy to delete!" ) );
        return;
    }
    javaScriptDomainPolicy.remove(index);
    delete index;
    changed();
}

void KJavaScriptOptions::importPressed()
{
  // PENDING(kalle) Implement this.
}

void KJavaScriptOptions::exportPressed()
{
  // PENDING(kalle) Implement this.
}


void KJavaScriptOptions::changeJavaScriptEnabled()
{
  bool enabled = enableJavaScriptGloballyCB->isChecked();
  enableJavaScriptGloballyCB->setChecked( enabled );
}

void KJavaScriptOptions::updateDomainList(const QStringList &domainConfig)
{
    for (QStringList::ConstIterator it = domainConfig.begin();
         it != domainConfig.end(); ++it) {
      QString domain;
      KHTMLSettings::KJavaScriptAdvice javaAdvice;
      KHTMLSettings::KJavaScriptAdvice javaScriptAdvice;
      KHTMLSettings::splitDomainAdvice(*it, domain, javaAdvice, javaScriptAdvice);
      QListViewItem *index =
        new QListViewItem( domainSpecificLV, domain,
                i18n(KHTMLSettings::adviceToStr(javaScriptAdvice)) );

      javaScriptDomainPolicy[index] = javaScriptAdvice;
    }
}
