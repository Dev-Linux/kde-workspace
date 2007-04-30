//
// C++ Implementation: kxkbwidget
//
// Description:
//
//
// Author: Andriy Rysin <rysin@kde.org>, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution
//

#include <QSystemTrayIcon>
#include <QMenu>
#include <QMouseEvent>

#include <kdebug.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kaction.h>

#include "pixmap.h"
#include "rules.h"
#include "kxkbconfig.h"

#include "kxkbwidget.h"

#include "kxkbwidget.moc"


KxkbWidget::KxkbWidget():
	m_configSeparator(NULL)
{
}

void KxkbWidget::setCurrentLayout(const LayoutUnit& layoutUnit)
{
	setToolTip(m_descriptionMap[layoutUnit.toPair()]);
	const QPixmap& icon = LayoutIcon::getInstance().findPixmap(layoutUnit.layout, m_showFlag, layoutUnit.displayName);
//	kDebug() << "setting pixmap: " << icon.width() << endl;
	setPixmap( icon );
	kDebug() << "setting text: " << layoutUnit.layout << endl;
	setText(layoutUnit.layout);
}

void KxkbWidget::setError(const QString& layoutInfo)
{
    QString msg = i18n("Error changing keyboard layout to '%1'", layoutInfo);
	setToolTip(msg);
	setPixmap(LayoutIcon::getInstance().findPixmap("error", m_showFlag));
}


void KxkbWidget::initLayoutList(const QList<LayoutUnit>& layouts, const XkbRules& rules)
{
	QMenu* menu = contextMenu();

    m_descriptionMap.clear();

//    menu->clear();
//    menu->addTitle( qApp->windowIcon(), KGlobal::caption() );
//    menu->setTitle( KGlobal::mainComponent().aboutData()->programName() );

	for(QList<QAction*>::Iterator it=m_actions.begin(); it != m_actions.end(); it++ )
			menu->removeAction(*it);
	m_actions.clear();
	
	int cnt = 0;
    QList<LayoutUnit>::ConstIterator it;
    for (it=layouts.begin(); it != layouts.end(); ++it)
    {
		const QString layoutName = (*it).layout;
		const QString variantName = (*it).variant;

		const QPixmap& layoutPixmap = LayoutIcon::getInstance().findPixmap(layoutName, m_showFlag, (*it).displayName);
//         const QPixmap pix = iconeffect.apply(layoutPixmap, KIcon::Small, KIcon::DefaultState);

		QString layoutString = rules.layouts()[layoutName];
		QString fullName = i18n( layoutString.toLatin1().constData() );
		if( variantName.isEmpty() == false )
			fullName += " (" + variantName + ')';
//		menu->insertItem(pix, fullName, START_MENU_ID + cnt, m_menuStartIndex + cnt);

		QAction* action = new QAction(layoutPixmap, fullName, menu);
		action->setData(START_MENU_ID + cnt);
		m_actions.append(action);
		m_descriptionMap.insert((*it).toPair(), fullName);

		cnt++;
    }
	menu->insertActions(m_configSeparator, m_actions);

	// if show config, if show help
//	if( menu->indexOf(CONFIG_MENU_ID) == -1 ) {
	if( m_configSeparator == NULL ) { // first call
		m_configSeparator = menu->addSeparator();

		QAction* configAction = new QAction(SmallIcon("configure"), i18n("Configure..."), menu);
		configAction->setData(CONFIG_MENU_ID);
		menu->addAction(configAction);

//		if( menu->indexOf(HELP_MENU_ID) == -1 )
		QAction* helpAction = new QAction(SmallIcon("help-contents"), i18n("Help"), menu);
		helpAction->setData(HELP_MENU_ID);
		menu->addAction(helpAction);
	}

/*    if( index != -1 ) { //not first start
		menu->addSeparator();
		KAction* quitAction = KStdAction::quit(this, SIGNAL(quitSelected()), actionCollection());
        if (quitAction)
    	    quitAction->plug(menu);
    }*/
}


// ----------------------------
// QSysTrayIcon implementation

KxkbSysTrayIcon::KxkbSysTrayIcon()
{
	m_tray = new KSystemTrayIcon();

	connect(contextMenu(), SIGNAL(triggered(QAction*)), this, SIGNAL(menuTriggered(QAction*)));
	connect(m_tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), 
					this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
}

KxkbSysTrayIcon::~KxkbSysTrayIcon()
{
	delete m_tray;
}

void KxkbSysTrayIcon::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
	if( reason == QSystemTrayIcon::Trigger )
	  emit iconToggled();
}

void KxkbSysTrayIcon::setPixmap(const QPixmap& pixmap)
{
	kDebug() << "setting icon to tray" << endl;
	m_tray->setIcon( pixmap );
// 	if( ! m_tray->isVisible() )
		m_tray->show();
}

// ----------------------------
// text-only applet widget (temporary workaround)

void MyLineEdit::mousePressEvent ( QMouseEvent * event ) {
	if (event->button() == Qt::LeftButton)
		emit leftClick();
	else {
		emit rightClick();
	}
}

KxkbLabel::KxkbLabel(QWidget* parent):
		KxkbWidget()
{
	m_tray = new MyLineEdit(parent); 
	m_menu = new QMenu(m_tray); 
	connect(m_tray, SIGNAL(leftClick()), this, SIGNAL(iconToggled())); 
	connect(m_tray, SIGNAL(rightClick()), this, SLOT(rightClick())); 
	connect(contextMenu(), SIGNAL(triggered(QAction*)), this, SIGNAL(menuTriggered(QAction*)));
	m_tray->resize( 24,24 ); 
	show();
}

void KxkbLabel::rightClick() {
	QMenu* menu = contextMenu();
	menu->exec(m_tray->mapToGlobal(QPoint(0, 0)));
}

void KxkbLabel::setPixmap(const QPixmap& pixmap)
{
//	kDebug() << "setting pixmap to label, width: " << pixmap.width() << endl;
	m_tray->setIcon( pixmap );
	if( ! m_tray->isVisible() )
		m_tray->show();
}

