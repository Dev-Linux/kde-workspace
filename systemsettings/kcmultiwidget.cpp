/*
   Copyright (c) 2000 Matthias Elter <elter@kde.org>
   Copyright (c) 2003 Daniel Molkentin <molkentin@kde.org>
   Copyright (c) 2003 Matthias Kretz <kretz@kde.org>
   Copyright (c) 2004 Frans Englich <frans.erglich.com>

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
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.

*/

#include <qcursor.h>
#include <q3hbox.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <QProcess>
//Added by qt3to4:
#include <Q3HBoxLayout>
#include <Q3Frame>

#include <kaboutdata.h>
#include <kapplication.h>
#include <kdebug.h>
#include <kiconloader.h>
#include <klibloader.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <krun.h>
#include <kstandardguiitem.h>
#include <kuser.h>
#include <kauthorized.h>

#include "kcmoduleloader.h"
#include "kcmoduleproxy.h"
#include "kcmultiwidget.h"

/*
Button usage:

    User1 => Reset
    User2 => Close
    User3 => Admin (dead in KDE 4)
*/

class KCMultiWidget::KCMultiWidgetPrivate
{
	public:
		KCMultiWidgetPrivate()
			: hasRootKCM( false ), currentModule( 0 )
		{}

		bool hasRootKCM;
		KCModuleProxy* currentModule;
};

 
KCMultiWidget::KCMultiWidget(QWidget *parent, Qt::WindowModality modality)
	: KPageDialog( parent ),
    d( new KCMultiWidgetPrivate )
{
  InitKIconDialog(i18n("Configure"), modality);
	init();
}

KCMultiWidget::KCMultiWidget( int dialogFace, QWidget * parent, Qt::WindowModality modality )
	: KPageDialog( parent ),
    dialogface( dialogFace ),
    d( new KCMultiWidgetPrivate )
{
  InitKIconDialog(i18n(""), modality);
	init();
}

// Maybe move into init()?
void KCMultiWidget::InitKIconDialog(const QString& caption,
                                    Qt::WindowModality modality)
{
  setCaption(caption);
  setButtons(KDialog::Help |
             KDialog::Default |
             KDialog::Cancel |
             KDialog::Apply |
             KDialog::Ok |
             KDialog::User1 |
             KDialog::User2);
  setDefaultButton(KDialog::Ok);
  setButtonGuiItem(KDialog::User1, KStandardGuiItem::reset());
  setButtonGuiItem(KDialog::User2, KStandardGuiItem::close());

  setWindowModality(modality);

//   iconPage.setParent(this);
}

inline void KCMultiWidget::init()
{
	connect( this, SIGNAL( finished()), SLOT( dialogClosed()));
	showButton( Ok, false );
	showButton( Cancel, false );
	showButton( User1, true );     // Reset button
	showButton( User2, false );    // Close button.

	enableButton(Apply, false);
	enableButton(User1, false);

	connect( this, SIGNAL(currentPageChanged(KPageWidgetItem*, KPageWidgetItem*)), this, SLOT(slotAboutToShow(KPageWidgetItem*, KPageWidgetItem* )) );
	setInitialSize(QSize(640,480));
	moduleParentComponents.setAutoDelete( true );
	setFaceType( Auto );
	connect( this, SIGNAL(helpClicked()), this, SLOT(slotHelp()) );
	connect( this, SIGNAL(defaultClicked()), this, SLOT(slotDefault()) );
	connect( this, SIGNAL(applyClicked()), this, SLOT(slotApply()) );
	connect( this, SIGNAL(user1Clicked()), this, SLOT(slotReset()) );
	connect( this, SIGNAL(user2Clicked()), this, SLOT(slotClose()) );
}

KCMultiWidget::~KCMultiWidget()
{
	OrphanMap::Iterator end2 = m_orphanModules.end();
	for( OrphanMap::Iterator it = m_orphanModules.begin(); it != end2; ++it )
		delete ( *it );
}

void KCMultiWidget::slotDefault()
{
	currentModule()->defaults();
	clientChanged( true );
	return;
}

void KCMultiWidget::slotReset()
{
	ModuleList::Iterator end = m_modules.end();
	currentModule()->load();
	clientChanged( false );
}

void KCMultiWidget::apply()
{
	QStringList updatedModules;
	ModuleList::Iterator end = m_modules.end();
	for( ModuleList::Iterator it = m_modules.begin(); it != end; ++it )
	{
		KCModuleProxy * m = ( *it ).kcm;
		if( m && m->changed() )
		{
			m->save();
			QStringList * names = moduleParentComponents[ m ];
			for( QStringList::ConstIterator it = names->begin(); it != names->end(); ++it )
				if( updatedModules.indexOf( *it ) == -1 )
					updatedModules.append( *it );
		}
	}
	for( QStringList::const_iterator it = updatedModules.begin(); it != updatedModules.end(); ++it )
	{
		kDebug() << k_funcinfo << *it << " " << ( *it ) << endl;
		emit configCommitted( ( *it ).latin1() );
	}
	emit configCommitted();
}

void KCMultiWidget::slotApply()
{
	apply();
}

void KCMultiWidget::slotOk()
{
	emit okClicked();
	apply();
	accept();
}

void KCMultiWidget::slotHelp()
{
	QString docPath = currentModule()->moduleInfo().docPath();

	KUrl url( KUrl("help:/"), docPath );

	if (url.protocol() == "help" || url.protocol() == "man" || url.protocol() == "info") {
		QProcess process(this);
		process.start("khelpcenter", QStringList() << url.url());
	} else {
		new KRun(url, this);
	}
}

// Close button
void KCMultiWidget::slotClose() {
    emit close();
}

void KCMultiWidget::clientChanged(bool state)
{
	kDebug( 710 ) << k_funcinfo << state << endl;
	ModuleList::Iterator end = m_modules.end();
	for( ModuleList::Iterator it = m_modules.begin(); it != end; ++it )
		if( ( *it ).kcm->changed() ) {
			enableButton( Apply, true );
                        enableButton( User1, true);
			return;
		}
	enableButton( Apply, false );
        enableButton( User1, false);
}

void KCMultiWidget::addModule(const QString& path, bool withfallback)
{
	QString complete = path;

	if( !path.endsWith( ".desktop" ))
		complete += ".desktop";

	KService::Ptr service = KService::serviceByStorageId( complete );

	addModule( KCModuleInfo( service ), QStringList(), withfallback);
}

void KCMultiWidget::addModule(const KCModuleInfo& moduleinfo,
		QStringList parentmodulenames, bool withfallback)
{
	if( !moduleinfo.service() ) {
		kWarning() << "ModuleInfo has no associated KService" << endl;
		return;
	}

	if ( !KAuthorized::authorizeControlModule( moduleinfo.service()->menuId() )) {
		kWarning() << "Not authorised to load module" << endl;
		return;
	}

	if(moduleinfo.service()->noDisplay()) {
		KCModuleLoader::unloadModule(moduleinfo);
		return;
	}
	KCModuleProxy * module;
	if( m_orphanModules.contains( moduleinfo.service() ) )
	{
		// the KCModule already exists - it was removed from the dialog in
		// removeAllModules
		module = m_orphanModules[ moduleinfo.service() ];
		m_orphanModules.remove( moduleinfo.service() );
		kDebug( 710 ) << "Use KCModule from the list of orphans for " <<
			moduleinfo.moduleName() << ": " << module << endl;

		if( module->changed() ) {
			clientChanged( true );
		}

	}
	else
	{
		module = new KCModuleProxy( moduleinfo, this );

		QStringList parentComponents = moduleinfo.service()->property(
				"X-KDE-System-Settings-Parent-Category" ).toStringList();
		moduleParentComponents.insert( module,
				new QStringList( parentComponents ) );

		connect(module, SIGNAL(changed(bool)), this, SLOT(clientChanged(bool)));
	}

	CreatedModule cm;
	cm.kcm = module;
	cm.service = moduleinfo.service();
	cm.adminmode = false;
	cm.buttons = module->buttons();
// "root KCMs are gone" says KControl
//	if ( moduleinfo.needsRootPrivileges() && !d->hasRootKCM &&
//			!KUser().isSuperUser() ) {/* If we're embedded, it's true */
// 		d->hasRootKCM = true;
// 		cm.adminmode = true;
// 		m_modules.append( cm );
// 		if( dialogface==Plain ) {
// 			slotAboutToShow( page ); // Won't be called otherwise, necessary for adminMode button
//                }
// 	} else {
// 		m_modules.append( cm );
// 	}

	m_modules.append( cm );
	if( m_modules.count() == 1 ) {
		slotAboutToShow( module );
	}
	KPageWidgetItem* page = addPage(module, moduleinfo.moduleName());
	page->setIcon( KIcon(moduleinfo.icon()) );
	page->setHeader(moduleinfo.comment());
}

KCModuleProxy* KCMultiWidget::currentModule() {
	QWidget* pageWidget = currentPage()->widget();
	KCModuleProxy* module = qobject_cast<KCModuleProxy*>(pageWidget);
	if (!module) {
		return NULL;
	} else {
		return module;
	}
}

void KCMultiWidget::applyOrRevert(KCModuleProxy * module){
	if( !module || !module->changed() )
		return;
	
	int res = KMessageBox::warningYesNo(this,
				i18n("There are unsaved changes in the active module.\n"
             "Do you want to apply the changes or discard them?"),
                                      i18n("Unsaved Changes"),
                                      KStandardGuiItem::apply(),
                                      KStandardGuiItem::discard());
	if (res == KMessageBox::Yes) {
		slotApply();
	} else {
		module->load();
		clientChanged( false );
	}
}

void KCMultiWidget::slotAboutToShow(KPageWidgetItem* current, KPageWidgetItem* before)
{
	QWidget* sendingWidget = current->widget();
	slotAboutToShow(sendingWidget);
}

void KCMultiWidget::slotAboutToShow(QWidget *page)
{
	QObject * obj = page->child( 0, "KCModuleProxy" );
	if( ! obj ) {
		obj = page;
	}

	KCModuleProxy *module = qobject_cast<KCModuleProxy*>(obj);
	if( ! module ) {
		return;
	}

	if( d && d->currentModule ) {
		applyOrRevert( d->currentModule );
	}
	d->currentModule = module;
	emit ( aboutToShow( d->currentModule ) );

	ModuleList::Iterator end = m_modules.end();
	int buttons = 0;
	for( ModuleList::Iterator it = m_modules.begin(); it != end; ++it ) {
		if( ( *it ).kcm==d->currentModule) {
			showButton(User3, ( *it ).adminmode);
			buttons = ( *it ).buttons;
		}
	}

        showButton(Apply, buttons & KCModule::Apply);
        showButton(User1, buttons & KCModule::Apply);   // Reset button.

        // Close button. No Apply button implies a Close button.
        showButton(User2, (buttons & KCModule::Apply)==0);

	enableButton( KDialog::Help, buttons & KCModule::Help );
	enableButton( KDialog::Default, buttons & KCModule::Default );

	disconnect( this, SIGNAL(user3Clicked()), 0, 0 );

// 	if (d->currentModule->moduleInfo().needsRootPrivileges() &&
// 			!d->currentModule->rootMode() )
// 	{ /* Enable the Admin Mode button */
// 		enableButton( User3, true );
// 		connect( this, SIGNAL(user3Clicked()), d->currentModule, SLOT( runAsRoot() ));
// 		connect( this, SIGNAL(user3Clicked()), SLOT( disableRModeButton() ));
// 	} else {
// 		enableButton( User3, false );
// 	}
}

void KCMultiWidget::rootExit()
{
	enableButton( User3, true);
}

void KCMultiWidget::disableRModeButton()
{
	enableButton( User3, false );
	connect ( d->currentModule, SIGNAL( childClosed() ), SLOT( rootExit() ) );
}

void KCMultiWidget::slotCancel() {
	dialogClosed();
}

void KCMultiWidget::dialogClosed()
{
	if(d)
	{
		applyOrRevert(d->currentModule);
	}
}

#include "kcmultiwidget.moc"
