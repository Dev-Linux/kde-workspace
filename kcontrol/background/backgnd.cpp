/* vi: ts=8 sts=4 sw=4
 *
 * $Id$
 *
 * This file is part of the KDE project, module kcmdisplay.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 *
 * Based on old backgnd.cpp:
 *
 * Copyright (c)  Martin R. Jones 1996
 * Converted to a kcc module by Matthias Hoelzer 1997
 * Gradient backgrounds by Mark Donohoe 1997
 * Pattern backgrounds by Stephan Kulow 1998
 * Randomizing & dnd & new display modes by Matej Koss 1998
 *
 * You can Freely distribute this program under the GNU General Public
 * License. See the file "COPYING" for the exact licensing terms.
 */

#include <qobject.h>
#include <qlayout.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qlabel.h>
#include <qlistbox.h>
#include <qgroupbox.h>
#include <qcombobox.h>
#include <qslider.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qdragobject.h>
#include <qhbox.h>
#include <qevent.h>
#include <qwhatsthis.h>
#include <qtabwidget.h>
#include <qspinbox.h>

#include <kapp.h>
#include <kconfig.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <kiconloader.h>
#include <kdebug.h>
#include <kcolorbtn.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kpixmap.h>
#include <dcopclient.h>
#include <kimageio.h>

#include <bgdefaults.h>
#include <bgsettings.h>
#include <bgrender.h>
#include <bgdialogs.h>
#include <backgnd.h>

/* as late as possible, as it includes some X headers without protecting them */
#include <kwin.h>

/**** DLL Interface ****/

extern "C" {
    KCModule *create_background(QWidget *parent, const char *name) {
    KGlobal::locale()->insertCatalogue("kcmdisplay");
    return new KBackground(parent, name);
    }
}


/**** KBGMonitor ****/

KBGMonitor::KBGMonitor(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    setAcceptDrops(true);
}


void KBGMonitor::dropEvent(QDropEvent *e)
{
    if (!QUriDrag::canDecode(e))
    return;

    QStringList uris;
    if (QUriDrag::decodeLocalFiles(e, uris) && (uris.count() > 0)) {
    QString uri = *uris.begin();
    emit imageDropped(uri);
    }
}


void KBGMonitor::dragEnterEvent(QDragEnterEvent *e)
{
    if (QUriDrag::canDecode(e))
    e->accept(rect());
    else
    e->ignore(rect());
}


/**** KBackground ****/

KBackground::KBackground(QWidget *parent, const char *name)
    : KCModule(parent, name)
{

    KImageIO::registerFormats();

    m_pConfig = new KConfig("kdesktoprc");
    m_pDirs = KGlobal::dirs();

    // Top layout
    QGridLayout *top = new QGridLayout(this, 3, 2);
    top->setSpacing(10); top->setMargin(10);
    top->setColStretch(0, 1);
    top->setColStretch(1, 2);

    // A nice button size. Translators can adapt this
    QPushButton *pbut = new QPushButton(i18n("some nonsense string as example string", "abcdefgh"), this);
    QSize bsize = pbut->sizeHint();
    delete pbut;

    // Desktop chooser at (0, 0)
    QGroupBox *group = new QGroupBox(i18n("Desktop"), this);
    top->addWidget(group, 0, 0);
    QVBoxLayout *vbox = new QVBoxLayout(group);
    vbox->setMargin(10);
    vbox->setSpacing(10);
    vbox->addSpacing(10);
    m_pDeskList = new QListBox(group);
    connect(m_pDeskList, SIGNAL(highlighted(int)), SLOT(slotSelectDesk(int)));
    vbox->addWidget(m_pDeskList);
    m_pCBCommon = new QCheckBox(i18n("&Common Background"), group);
    vbox->addWidget(m_pCBCommon);
    connect(m_pCBCommon, SIGNAL(toggled(bool)), SLOT(slotCommonDesk(bool)));
    QWhatsThis::add( m_pDeskList, i18n("Choose the desktop whose background"
      " you want to modify. If you want the same background settings to be"
      " applied to all desktops, check the \"Common Background\" option, and"
      " this list will be disabled.") );
    QWhatsThis::add( m_pCBCommon, i18n("Check this option if you want to have"
      " the same background settings for all desktops. If this option is not"
      " checked, the background settings can be customized for each desktop.") );

    // Preview monitor at (0,1)
    QHBoxLayout *hbox = new QHBoxLayout();
    top->addLayout(hbox, 0, 1);
    QLabel *lbl = new QLabel(this);
    lbl->setPixmap(locate("data", "kcontrol/pics/monitor.png"));
    lbl->setFixedSize(lbl->sizeHint());
    hbox->addWidget(lbl);
    m_pMonitor = new KBGMonitor(lbl, "preview monitor");
    m_pMonitor->setGeometry(20, 10, 157, 111);
    connect(m_pMonitor, SIGNAL(imageDropped(QString)), SLOT(slotImageDropped(QString)));
    QWhatsThis::add( m_pMonitor, "In this monitor, you can preview how your settings will look like on a \"real\" desktop." );

    // Tabwidget at (1,0) - (1,1)
    m_pTabWidget = new QTabWidget(this);
    top->addMultiCellWidget(m_pTabWidget, 1, 1, 0, 1);

    // Background settings at Tab 1
    m_pTab1 = new QWidget(0L, "Background Tab");
    m_pTabWidget->addTab(m_pTab1, i18n("&Background"));
    QGridLayout *grid = new QGridLayout(m_pTab1, 4, 3, 10, 10);
    grid->setColStretch(1, 1);
    grid->setColStretch(2, 1);

    lbl = new QLabel(i18n("&Mode:"), m_pTab1);
    lbl->setFixedSize(lbl->sizeHint());
    grid->addWidget(lbl, 0, 0, Qt::AlignLeft);
    m_pBackgroundBox = new QComboBox(m_pTab1);
    connect(m_pBackgroundBox, SIGNAL(activated(int)), SLOT(slotBGMode(int)));
    lbl->setBuddy(m_pBackgroundBox);
    grid->addWidget(m_pBackgroundBox, 0, 1);
    QWhatsThis::add( m_pBackgroundBox, i18n("You can select the manner in which"
      " the background is painted. The choices are:"
      " <ul><li><em>Flat:</em> Use a solid color (\"Color 1\").</li>"
      " <li><em>Pattern:</em> Use a two-color pattern. Click \"Setup\" to choose"
      " the pattern.</li>"
      " <li><em>Gradients:</em> Blend two colors using a predefined gradient"
      " (horizontal, vertical, etc.).</li>"
      " <li><em>Program:</em> Use an application which paints the background, for"
      " example, with a day/night map of the world that is refreshed periodically."
      " Click \"Setup\" to select and configure the program.</li></ul>") );

    lbl = new QLabel(i18n("Color &1:"), m_pTab1);
    lbl->setFixedSize(lbl->sizeHint());
    grid->addWidget(lbl, 1, 0, Qt::AlignLeft);
    m_pColor1But = new KColorButton(m_pTab1);
    connect(m_pColor1But, SIGNAL(changed(const QColor &)),
        SLOT(slotColor1(const QColor &)));
    grid->addWidget(m_pColor1But, 1, 1);
    lbl->setBuddy(m_pColor1But);
    QWhatsThis::add( m_pColor1But, i18n("Click to choose a color.") );

    lbl = new QLabel(i18n("Color &2:"), m_pTab1);
    lbl->setFixedSize(lbl->sizeHint());
    grid->addWidget(lbl, 2, 0, Qt::AlignLeft);
    m_pColor2But = new KColorButton(m_pTab1);
    connect(m_pColor2But, SIGNAL(changed(const QColor &)),
        SLOT(slotColor2(const QColor &)));
    grid->addWidget(m_pColor2But, 2, 1);
    lbl->setBuddy(m_pColor2But);
    QWhatsThis::add( m_pColor2But, i18n("Click to choose a second color. If the"
      " background mode does not require a second color, this button is disabled.") );

    m_pBGSetupBut = new QPushButton(i18n("&Setup"), m_pTab1);
    m_pBGSetupBut->setFixedSize(bsize);
    grid->addWidget(m_pBGSetupBut, 3, 1, Qt::AlignLeft);
    connect(m_pBGSetupBut, SIGNAL(clicked()), SLOT(slotBGSetup()));
    QWhatsThis::add( m_pBGSetupBut, i18n("If the background mode you selected has"
      " additional options to configure, click here.") );

    // Wallpaper at Tab 2
    m_pTab2 = new QWidget(0L, "Wallpaper Tab");
    m_pTabWidget->addTab(m_pTab2, i18n("&Wallpaper"));
    grid = new QGridLayout(m_pTab2, 4, 3, 10, 10);
    grid->setColStretch(1, 1);
    grid->setColStretch(2, 1);

    lbl = new QLabel(i18n("M&ode:"), m_pTab2);
    lbl->setFixedSize(lbl->sizeHint());
    grid->addWidget(lbl, 0, 0, Qt::AlignLeft);
    m_pArrangementBox = new QComboBox(m_pTab2);
    connect(m_pArrangementBox, SIGNAL(activated(int)), SLOT(slotWPMode(int)));
    lbl->setBuddy(m_pArrangementBox);
    grid->addWidget(m_pArrangementBox, 0, 1, Qt::AlignLeft);
    QWhatsThis::add( m_pArrangementBox, i18n("You can have a wallpaper (based on"
      " a graphic) on top of your background. You can choose one of the following"
      " methods for displaying the wallpaper:"
      " <ul><li><em>Centered:</em> Center the graphic on the desktop.</li>"
      " <li><em>Tiled:</em> Tile the graphic beginning at the top left of the"
      " desktop, so the background is totally covered up.</li>"
      " <li><em>Center Tiled:</em> Center the graphic on the desktop, and then"
      " tile around it so that the background is totally covered up.</li>"
      " <li><em>Centered Maxpect:</em> Magnify the graphic without distorting it"
      " until it fills either the width or height of the desktop, and then center"
      " it on the desktop.</li>"
      " <li><em>Scaled:</em> Magnify the graphic, distorting it if necessary,"
      " until the entire desktop is covered.</li></ul>") );

    lbl = new QLabel(i18n("&Wallpaper:"), m_pTab2);
    lbl->setFixedSize(lbl->sizeHint());
    grid->addWidget(lbl, 1, 0, Qt::AlignLeft);
    m_pWallpaperBox = new QComboBox(m_pTab2);
    lbl->setBuddy(m_pWallpaperBox);
    connect(m_pWallpaperBox, SIGNAL(activated(const QString &)),
        SLOT(slotWallpaper(const QString &)));
    grid->addWidget(m_pWallpaperBox, 1, 1);
    QWhatsThis::add( m_pWallpaperBox, i18n("Click to choose the graphic you want"
      " to use as wallpaper.") );

    m_pBrowseBut = new QPushButton(i18n("&Browse"), m_pTab2);
    grid->addWidget(m_pBrowseBut, 2, 1, Qt::AlignLeft);
    m_pBrowseBut->setFixedSize(bsize);
    connect(m_pBrowseBut, SIGNAL(clicked()), SLOT(slotBrowseWallpaper()));
    QWhatsThis::add( m_pBrowseBut, i18n("If the graphic you want is not in a standard"
      " directory, you can still find it by clicking here.") );

    m_pCBMulti = new QCheckBox(i18n("M&ultiple:"), m_pTab2);
    m_pCBMulti->setFixedSize(m_pCBMulti->sizeHint());
    grid->addWidget(m_pCBMulti, 3, 0, Qt::AlignLeft);
    connect(m_pCBMulti, SIGNAL(toggled(bool)), SLOT(slotMultiMode(bool)));
    m_pMSetupBut = new QPushButton(i18n("S&etup"), m_pTab2);
    m_pMSetupBut->setFixedSize(bsize);
    grid->addWidget(m_pMSetupBut, 3, 1, Qt::AlignLeft);
    connect(m_pMSetupBut, SIGNAL(clicked()), SLOT(slotSetupMulti()));
    QWhatsThis::add( m_pCBMulti, i18n("If you check this option, you can choose"
      " a set of graphic files to be used as wallpaper, one at a time, for an"
      " interval ranging from 5 minutes to 4 hours. You can also choose to have"
      " the graphics selected at random or in the order you specified them.") );
    QWhatsThis::add( m_pMSetupBut, i18n("Click here to select graphics to be used"
      " for wallpaper, and to configure other options.") );

    // Tab 3: Advanced
    m_pTab3 = new QWidget(0L, "Advanced Tab");
    m_pTabWidget->addTab(m_pTab3, i18n("&Advanced"));
    grid = new QGridLayout(m_pTab3, 3, 3, 10, 10);
    grid->setColStretch(1, 1);
    grid->setColStretch(2, 1);

    lbl = new QLabel(i18n("B&lending:"), m_pTab3);
    lbl->setFixedSize(lbl->sizeHint());
    grid->addWidget(lbl, 0, 0, Qt::AlignLeft);
    m_pBlendBox = new QComboBox(m_pTab3);
    connect(m_pBlendBox, SIGNAL(activated(int)), SLOT(slotBlendMode(int)));
    lbl->setBuddy(m_pBlendBox);
    grid->addWidget(m_pBlendBox, 0, 1);
    QWhatsThis::add( m_pBlendBox, i18n("If you have selected to wallpaper, you"
      " can choose various methods of blending the background colors and patterns"
      " with the wallpaper. The default option, \"No Blending\", means that the"
      " wallpaper simply obscures the background below.") );

    hbox = new QHBoxLayout();
    grid->addLayout(hbox, 1, 0);
    lbl = new QLabel(i18n("B&alance:"), m_pTab3);
    lbl->setFixedSize(lbl->sizeHint());
    hbox->addSpacing(20); hbox->addWidget(lbl); hbox->addStretch();
    m_pBlendSlider = new QSlider(QSlider::Horizontal, m_pTab3);
    m_pBlendSlider->setRange( -200, 200 );
    connect(m_pBlendSlider, SIGNAL(valueChanged(int)), SLOT(slotBlendBalance(int)));
    lbl->setBuddy(m_pBlendSlider);
    grid->addWidget(m_pBlendSlider, 1, 1);
    QWhatsThis::add( m_pBlendSlider, i18n("You can use this slider to control"
      " the degree of blending. You can experiment by moving the slider and"
      " looking at the effects in the preview image above.") );
    m_pReverseBlending = new QCheckBox(i18n("&Reverse"), m_pTab3);
    m_pReverseBlending->setFixedSize(m_pReverseBlending->sizeHint());
    connect(m_pReverseBlending, SIGNAL(toggled(bool)),
        SLOT(slotReverseBlending(bool)));
    grid->addWidget(m_pReverseBlending, 1, 2, Qt::AlignLeft);
    QWhatsThis::add( m_pReverseBlending, i18n("For some types of blending, you can"
      " reverse the background and wallpaper layers by checking this option.") );

    m_pCBLimit = new QCheckBox(i18n("&Limit Pixmap Cache"), m_pTab3);
    QWhatsThis::add( m_pCBLimit, i18n( "Checking this box limits the amount of memory that KDE will use to save pixmap (raster graphics). It is advisable to do this, especially if you do not have lots of memory." ) );
    grid->addMultiCellWidget(m_pCBLimit, 2, 2, 0, 1);
    connect(m_pCBLimit, SIGNAL(toggled(bool)), SLOT(slotLimitCache(bool)));
    hbox = new QHBoxLayout();
    grid->addLayout(hbox, 3, 0);
    lbl = new QLabel(i18n("Cache &Size"), m_pTab3);
    lbl->setFixedSize(lbl->sizeHint());
    hbox->addSpacing(20); hbox->addWidget(lbl); hbox->addStretch();
    m_pCacheBox = new QSpinBox(m_pTab3);
    m_pCacheBox->setSteps(512, 1024);
    m_pCacheBox->setSuffix(i18n(" Kb"));
    m_pCacheBox->setRange(512, 10240);
    grid->addWidget(m_pCacheBox, 3, 1, Qt::AlignLeft);
    lbl->setBuddy(m_pCacheBox);
    connect(m_pCacheBox, SIGNAL(valueChanged(int)), SLOT(slotCacheSize(int)));
    QString wtstr = i18n( "In this box you can enter how much memory KDE should use for caching pixmaps for faster access." );
    QWhatsThis::add( lbl, wtstr );
    QWhatsThis::add( m_pCacheBox, wtstr );

    m_Desk = KWin::currentDesktop() - 1;
    m_Max = KWin::numberOfDesktops();
    m_pGlobals = new KGlobalBackgroundSettings();
    for (int i=0; i<m_Max; i++) {
    m_Renderer[i] = new KBackgroundRenderer(i);
    connect(m_Renderer[i], SIGNAL(imageDone(int)), SLOT(slotPreviewDone(int)));
    }

    // Doing this only in KBGMonitor only doesn't work, probably due to the
    // reparenting that is done.
    setAcceptDrops(true);

    init();
    apply();

}


/*
 * Fill all check/listboxen
 */
void KBackground::init()
{
    int i;

    // Desktop names
    for (i=0; i<m_Max; i++)
    m_pDeskList->insertItem(m_pGlobals->deskName(i));

    // Background modes: make sure these match with kdesktop/bgrender.cc !!
    m_pBackgroundBox->insertItem(i18n("Simple Color", "Flat"));
    m_pBackgroundBox->insertItem(i18n("Pattern"));
    m_pBackgroundBox->insertItem(i18n("Background Program"));
    m_pBackgroundBox->insertItem(i18n("Horizontal Gradient"));
    m_pBackgroundBox->insertItem(i18n("Vertical Gradient"));
    m_pBackgroundBox->insertItem(i18n("Pyramid Gradient"));
    m_pBackgroundBox->insertItem(i18n("Pipecross Gradient"));
    m_pBackgroundBox->insertItem(i18n("Elliptic Gradient"));

    // Blend modes: make sure these match with kdesktop/bgrender.cc !!
    m_pBlendBox->insertItem(i18n("No Blending"));
    m_pBlendBox->insertItem(i18n("Horizontal Blending"));
    m_pBlendBox->insertItem(i18n("Vertical Blending"));
    m_pBlendBox->insertItem(i18n("Pyramid Blending"));
    m_pBlendBox->insertItem(i18n("Pipecross Blending"));
    m_pBlendBox->insertItem(i18n("Elliptic Blending"));
    m_pBlendBox->insertItem(i18n("Intensity Blending"));
    m_pBlendBox->insertItem(i18n("Saturate Blending"));
    m_pBlendBox->insertItem(i18n("Contrast Blending"));
    m_pBlendBox->insertItem(i18n("Hue Shift Blending"));

    // Use this as a common size for the combo boxen
    int width = m_pBackgroundBox->sizeHint().width();

    // Wallpapers
    QStringList lst = m_pDirs->findAllResources("wallpaper", "*", false, true);
    for (i=0; i<(int)lst.count(); i++) {
    int n = lst[i].findRev('/');
    QString s = lst[i].mid(n+1);
    m_pWallpaperBox->insertItem(s);
    m_Wallpaper[s] = i;
    }

    // I would have prefered to use setMinimumWidth here, but it seems to
    // have no effect. The default width is too large because of the long
    // filenames that are inserted.
    // Yeah but truncating filenames is even more ugly (David)
    //m_pWallpaperBox->setFixedWidth(width);
    // Better set a _max_ size that doesn't truncate the names.
    m_pWallpaperBox->setMaximumWidth(m_pWallpaperBox->sizeHint().width());

    // Wallpaper tilings: again they must match the ones from bgrender.cc
    m_pArrangementBox->insertItem(i18n("No Wallpaper"));
    m_pArrangementBox->insertItem(i18n("Centered"));
    m_pArrangementBox->insertItem(i18n("Tiled"));
    m_pArrangementBox->insertItem(i18n("Center Tiled"));
    m_pArrangementBox->insertItem(i18n("Centered Maxpect"));
    m_pArrangementBox->insertItem(i18n("Scaled"));
    m_pArrangementBox->setFixedWidth(width);
}


void KBackground::apply()
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    // Desktop names
    if (m_pGlobals->commonBackground()) {
    m_pCBCommon->setChecked(true);
    m_pDeskList->setEnabled(false);
    } else  {
    m_pCBCommon->setChecked(false);
    m_pDeskList->setEnabled(true);
    m_pDeskList->setCurrentItem(m_Desk);
    }

    // Background mode
    m_pBackgroundBox->setCurrentItem(r->backgroundMode());
    m_pColor1But->setColor(r->colorA());
    m_pColor2But->setColor(r->colorB());
    switch (r->backgroundMode()) {
    case KBackgroundSettings::Program:
    m_pColor1But->setEnabled(false);
    m_pColor2But->setEnabled(false);
    m_pBGSetupBut->setEnabled(true);
    break;
    case KBackgroundSettings::Flat:
    m_pColor1But->setEnabled(true);
    m_pColor2But->setEnabled(false);
    m_pBGSetupBut->setEnabled(false);
    break;
    case KBackgroundSettings::Pattern:
    m_pColor1But->setEnabled(true);
    m_pColor2But->setEnabled(true);
    m_pBGSetupBut->setEnabled(true);
    break;
    default:
    m_pColor1But->setEnabled(true);
    m_pColor2But->setEnabled(true);
    m_pBGSetupBut->setEnabled(false);
    break;
    }

    // Wallpaper mode
    QString wp = r->wallpaper();
    if (wp.isEmpty())
    wp = QString(" ");
    if (!m_Wallpaper.contains(wp)) {
    int count = m_Wallpaper.count();
    m_Wallpaper[wp] = count;
    m_pWallpaperBox->insertItem(wp);
    m_pWallpaperBox->setCurrentItem(count);
    }
    m_pWallpaperBox->setCurrentItem(m_Wallpaper[wp]);
    m_pArrangementBox->setCurrentItem(r->wallpaperMode());

    if (r->wallpaperMode() == KBackgroundSettings::NoWallpaper)
    {
    m_pCBMulti->setEnabled(false);
    m_pWallpaperBox->setEnabled(false);
    m_pBrowseBut->setEnabled(false);
    m_pMSetupBut->setEnabled(false);

    // Blending not possible without wallpaper
    m_pBlendBox->setEnabled(false);
    m_pBlendSlider->setEnabled(false);
    m_pReverseBlending->setEnabled(false);
    }
    else
    {
    m_pCBMulti->setEnabled(true);
    m_pBlendBox->setEnabled(true);
    m_pBlendSlider->setEnabled(
        (r->blendMode() == KBackgroundSettings::NoBlending) ? false : true);
    m_pReverseBlending->setEnabled(
        (r->blendMode() < KBackgroundSettings::IntensityBlending) ? false : true);

    // Multi mode
    if (r->multiWallpaperMode() == KBackgroundSettings::NoMulti)
    {
        m_pCBMulti->setChecked(false);
        m_pWallpaperBox->setEnabled(true);
        m_pBrowseBut->setEnabled(true);
        m_pMSetupBut->setEnabled(false);
    } else
    {
        m_pCBMulti->setChecked(true);
        m_pWallpaperBox->setEnabled(false);
        m_pBrowseBut->setEnabled(false);
        m_pMSetupBut->setEnabled(true);
    }
    }

    m_pBlendBox->setCurrentItem(r->blendMode());
    m_pBlendSlider->setValue(r->blendBalance());
    m_pReverseBlending->setChecked(r->reverseBlending());

    if (m_pGlobals->limitCache())
    {
    m_pCBLimit->setChecked(true);
    m_pCacheBox->setEnabled(true);
    } else
    {
    m_pCBLimit->setChecked(false);
    m_pCacheBox->setEnabled(false);
    }
    m_pCacheBox->setValue(m_pGlobals->cacheSize());

    // Start preview render
    r->setPreview(m_pMonitor->size());
    r->start();
}


void KBackground::load()
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    m_Renderer[desk]->load(desk);

    apply();
    emit changed(false);
}


void KBackground::save()
{
    kdDebug() << "Saving stuff..." << endl;
    m_pGlobals->writeSettings();
    for (int i=0; i<m_Max; i++)
    m_Renderer[i]->writeSettings();

    // reconfigure kdesktop. kdesktop will notify all clients
    DCOPClient *client = kapp->dcopClient();
    if (!client->isAttached())
    client->attach();
    client->send("kdesktop", "KBackgroundIface", "configure()", "");

    emit changed(false);
}


void KBackground::defaults()
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (r->isActive())
    r->stop();
    r->setBackgroundMode(KBackgroundSettings::Flat);
    r->setColorA(_defColorA);
    r->setColorB(_defColorB);
    r->setWallpaperMode(KBackgroundSettings::NoWallpaper);
    r->setMultiWallpaperMode(KBackgroundSettings::NoMulti);
    r->setBlendMode(KBackgroundSettings::NoBlending);
    r->setBlendBalance(_defBlendBalance);
    r->setReverseBlending(_defReverseBlending);
    m_pGlobals->setCommonBackground(_defCommon);
    m_pGlobals->setLimitCache(_defLimitCache);
    m_pGlobals->setCacheSize(_defCacheSize);
    apply();
    emit changed(true);
}


void KBackground::slotSelectDesk(int desk)
{
    if (desk == m_Desk)
    return;

    if (m_Renderer[m_Desk]->isActive())
    m_Renderer[m_Desk]->stop();
    m_Desk = desk;
    apply();
}


void KBackground::slotCommonDesk(bool common)
{
    if (common == m_pGlobals->commonBackground())
    return;

    m_pGlobals->setCommonBackground(common);
    apply();
    emit changed(true);
}


/*
 * Called from the "Background Mode" combobox.
 */
void KBackground::slotBGMode(int mode)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (mode == r->backgroundMode())
    return;

    r->stop();
    r->setBackgroundMode(mode);
    apply();
    emit changed(true);
}

/*
 * Called from the "Blending Mode" combobox.
 */
void KBackground::slotBlendMode(int mode)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (mode == r->blendMode())
    return;

    m_pBlendSlider->setEnabled( (mode==KBackgroundSettings::NoBlending)
                ?false:true);
    m_pReverseBlending->setEnabled(
      (r->blendMode()<KBackgroundSettings::IntensityBlending)?false:true);

    r->stop();
    r->setBlendMode(mode);
    apply();
    emit changed(true);
}

/*
 * Called from the "Blending" Slider
 */
void KBackground::slotBlendBalance(int value)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (value == r->blendBalance())
    return;

    r->stop();
    r->setBlendBalance(value);
    apply();
    emit changed(true);
}

/*
 * Called from the "Reverse Blending" Checkbox
 */
void KBackground::slotReverseBlending(bool value)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (value == r->reverseBlending())
    return;

    r->stop();
    r->setReverseBlending(value);
    apply();
    emit changed(true);
}


/*
 * Called from the "Background Setup" pushbutton.
 */
void KBackground::slotBGSetup()
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    switch (r->backgroundMode()) {
    case KBackgroundSettings::Pattern:
    {
    KPatternSelectDialog dlg;
    QString cur = r->KBackgroundPattern::name();
    dlg.setCurrent(cur);
    if ((dlg.exec() == QDialog::Accepted) && !dlg.pattern().isEmpty()) {
        r->stop();
        r->setPattern(dlg.pattern());
        r->start();
        emit changed(true);
    }
    break;
    }
    case KBackgroundSettings::Program:
    {
    KProgramSelectDialog dlg;
    QString cur = r->KBackgroundProgram::name();
    dlg.setCurrent(cur);
    if ((dlg.exec() == QDialog::Accepted) && !dlg.program().isEmpty()) {
        r->stop();
        r->setProgram(dlg.program());
        r->start();
        emit changed(true);
    }
    break;
    }
    default:
    break;
    }
}


void KBackground::slotColor1(const QColor &color)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (color == r->colorA())
    return;

    r->stop();
    r->setColorA(color);
    r->start();
    emit changed(true);
}


void KBackground::slotColor2(const QColor &color)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (color == r->colorB())
    return;

    r->stop();
    r->setColorB(color);
    r->start();
    emit changed(true);
}


void KBackground::slotImageDropped(QString uri)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];
    if (uri == r->wallpaper())
    return;

    if (!m_Wallpaper.contains(uri)) {
    int count = m_Wallpaper.count();
    m_Wallpaper[uri] = count;
    m_pWallpaperBox->insertItem(uri);
    m_pWallpaperBox->setCurrentItem(count);
    }

    r->stop();
    r->setWallpaper(uri);
    r->start();
    emit changed(true);
}


void KBackground::slotMultiMode(bool multi)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];
    if (multi == (r->multiWallpaperMode() != KBackgroundSettings::NoMulti))
    return;

    r->stop();
    r->setMultiWallpaperMode(multi ? 1 : 0);
    r->start();

    if (multi) {
    m_pWallpaperBox->setEnabled(false);
    m_pBrowseBut->setEnabled(false);
    m_pMSetupBut->setEnabled(true);
    } else {
    m_pWallpaperBox->setEnabled(true);
    m_pBrowseBut->setEnabled(true);
    m_pMSetupBut->setEnabled(false);
    }
    emit changed(true);
}


void KBackground::slotWallpaper(const QString &wallpaper)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (wallpaper == r->wallpaper())
    return;

    r->stop();
    r->setWallpaper(wallpaper);
    r->start();
    emit changed(true);
}


void KBackground::slotBrowseWallpaper()
{
    static KURL startUrl;

    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    KURL url = KFileDialog::getOpenURL(startUrl.url(),
                                       KImageIO::pattern(KImageIO::Reading),
                                       0,
                                       i18n("Select Wallpaper"));
    if (url.isEmpty())
      return;
    if (!url.isLocalFile()) {
      KMessageBox::sorry(this, i18n("Currently only local wallpapers are allowed."));
      return;
    }
    startUrl = url;
    QString file = url.path();
    if (file == r->wallpaper())
    return;

    if (!m_Wallpaper.contains(file)) {
    int count = m_Wallpaper.count();
    m_Wallpaper[file] = count;
    m_pWallpaperBox->insertItem(file);
    m_pWallpaperBox->setCurrentItem(count);
    }

    r->stop();
    r->setWallpaper(file);
    r->start();
    emit changed(true);
}


/*
 * Called from the "Wallpaper Arrangement" combobox.
 */
void KBackground::slotWPMode(int mode)
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    if (mode == r->wallpaperMode())
    return;

    if (mode == KBackgroundSettings::NoWallpaper) {
      m_pCBMulti->setEnabled(false);
      m_pWallpaperBox->setEnabled(false);
      m_pBrowseBut->setEnabled(false);
      m_pMSetupBut->setEnabled(false);

      // Blending not possible without wallpaper
      m_pBlendBox->setEnabled(false);
      m_pBlendSlider->setEnabled(false);
    }
    else {
      m_pCBMulti->setEnabled(true);
      m_pBlendBox->setEnabled(true);
      m_pBlendSlider->setEnabled(
      (r->blendMode()==KBackgroundSettings::NoBlending)?false:true);

      // Multi mode
      if (r->multiWallpaperMode() == KBackgroundSettings::NoMulti) {
    m_pCBMulti->setChecked(false);
    m_pWallpaperBox->setEnabled(true);
    m_pBrowseBut->setEnabled(true);
    m_pMSetupBut->setEnabled(false);
      } else {
    m_pCBMulti->setChecked(true);
    m_pWallpaperBox->setEnabled(false);
    m_pBrowseBut->setEnabled(false);
    m_pMSetupBut->setEnabled(true);
      }
    }

    r->stop();
    r->setWallpaperMode(mode);
    r->start();
    emit changed(true);
}


void KBackground::slotSetupMulti()
{
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    KBackgroundRenderer *r = m_Renderer[desk];

    KMultiWallpaperDialog dlg(r);
    if (dlg.exec() == QDialog::Accepted) {
    r->stop();
    r->start();
    emit changed(true);
    }
}


void KBackground::slotLimitCache(bool limit)
{
    m_pGlobals->setLimitCache(limit);
    m_pCacheBox->setEnabled(limit);
    emit changed(true);
}


void KBackground::slotCacheSize(int size)
{
    m_pGlobals->setCacheSize(size);
    emit changed(true);
}


void KBackground::slotPreviewDone(int desk_done)
{
    kdDebug() << "Preview for desktop " << desk_done << " done" << endl;

    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
    desk = 0;
    if (desk != desk_done)
    return;
    KBackgroundRenderer *r = m_Renderer[desk];

    KPixmap pm;
    if (QPixmap::defaultDepth() < 15)
    pm.convertFromImage(*r->image(), KPixmap::LowColor);
    else
    pm.convertFromImage(*r->image());

    m_pMonitor->setBackgroundPixmap(pm);
}


QString KBackground::quickHelp() const
{
    return i18n("<h1>Background</h1> This module allows you to control the"
      " appearance of the virtual desktops. KDE offers a variety of options"
      " for customization, including the ability to specify different settings"
      " for each virtual desktop, or a common background for all of them.<p>"
      " The appearance of the desktop results from the combination of its"
      " background colors and patterns, and optionally, wallpaper, which is"
      " based on the image from a graphic file.<p>"
      " The background can be made up of a single color, or a pair of colors"
      " which can be blended in a variety of patterns. Wallpaper is also"
      " customizable, with options for tiling and stretching images. The"
      " wallpaper can be overlaid opaquely, or blended in different ways with"
      " the background colors and patterns.<p>"
      " KDE allows you to have the wallpaper change automatically at prespecified"
      " intervals of time. You can also replace the background with a program"
      " that updates the desktop dynamically. For example, the \"kdeworld\""
      " program shows a day/night map of the world which is updated periodically.");
}


#include "backgnd.moc"
