//
//
// "Misc Options" Tab for KFM configuration
//
// (c) Sven Radej 1998
// (c) David Faure 1998

#include <qlabel.h>
#include <qgroupbox.h>
#include <qlayout.h>//CT - 12Nov1998
#include <qwhatsthis.h>
#include <kapp.h>

#include "miscopts.h"

#include <konq_defaults.h> // include default values directly from konqueror
#include <kglobalsettings.h> // get default for DEFAULT_CHANGECURSOR
#include <klocale.h>
#include <kconfig.h>

//-----------------------------------------------------------------------------

KMiscHTMLOptions::KMiscHTMLOptions(KConfig *config, QString group, QWidget *parent, const char *name )
    : KCModule( parent, name ), m_pConfig(config), m_groupname(group)
{
    QVBoxLayout *lay = new QVBoxLayout(this, 10, 5);

    cbCursor = new QCheckBox(i18n("&Change cursor over links"), this);
    lay->addWidget(cbCursor);

    QWhatsThis::add( cbCursor, i18n("If this option is set, the shape of the cursor will change "
       "(usually to a hand) if it is moved over a hyperlink.") );

    connect(cbCursor, SIGNAL(clicked()), this, SLOT(changed()));

    cbUnderline = new QCheckBox(i18n("&Underline links"), this);
    lay->addWidget(cbUnderline);

    QWhatsThis::add( cbUnderline, i18n("Tells konqueror to underline hyperlinks.") );

    connect(cbUnderline, SIGNAL(clicked()), this, SLOT(changed()));

    m_pAutoLoadImagesCheckBox = new QCheckBox( i18n( ""
     "Automatically load images\n"
     "(Otherwise, click the Images button to load when needed)" ), this );

    QWhatsThis::add( m_pAutoLoadImagesCheckBox, i18n( "If this box is checked, Konqueror will automatically load any images that are embedded in a web page. Otherwise, it will display placeholders for the images, and you can then manually load the images by clicking on the image button.<br>Unless you have a very slow network connection, you will probably want to check this box to enhance your browsing experience." ) );

    connect(m_pAutoLoadImagesCheckBox, SIGNAL(clicked()), this, SLOT(changed()));

    lay->addWidget( m_pAutoLoadImagesCheckBox, 1 );

    lay->addStretch(10);
    lay->activate();

    load();
}

void KMiscHTMLOptions::load()
{
    // *** load ***
    m_pConfig->setGroup( "HTML Settings" );
    bool changeCursor = m_pConfig->readBoolEntry("ChangeCursor", KDE_DEFAULT_CHANGECURSOR);
    bool underlineLinks = m_pConfig->readBoolEntry("UnderlineLinks", DEFAULT_UNDERLINELINKS);
    bool bAutoLoadImages = m_pConfig->readBoolEntry( "AutoLoadImages", true );

    // *** apply to GUI ***

    cbCursor->setChecked( changeCursor );
    cbUnderline->setChecked( underlineLinks );
    m_pAutoLoadImagesCheckBox->setChecked( bAutoLoadImages );
}

void KMiscHTMLOptions::defaults()
{
    cbCursor->setChecked( false );
    cbUnderline->setChecked( true );
    m_pAutoLoadImagesCheckBox->setChecked( false );
}

void KMiscHTMLOptions::save()
{
    m_pConfig->setGroup( "HTML Settings" );
    m_pConfig->writeEntry( "ChangeCursor", cbCursor->isChecked() );
    m_pConfig->writeEntry( "UnderlineLinks", cbUnderline->isChecked() );
    m_pConfig->writeEntry( "AutoLoadImages", m_pAutoLoadImagesCheckBox->isChecked() );
    m_pConfig->sync();
}

void KMiscHTMLOptions::changed()
{
    emit KCModule::changed(true);
}

#include "miscopts.moc"
