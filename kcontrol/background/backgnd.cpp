/* vi: ts=8 sts=4 sw=4
 *
 * $Id$
 *
 * This file is part of the KDE project, module kcmbackground.
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
#include <qhbuttongroup.h>
#include <kpixmap.h>
#include <kcolorbutton.h>
#include <kcombobox.h>
#include <dcopclient.h>
#include <qpainter.h>

#include <kfiledialog.h>
#include <kimageio.h>

#include <kdebug.h>

#include <kconfig.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kgenericfactory.h>

#include <netwm.h>

#include <bgdefaults.h>
#include <bgsettings.h>
#include <bgrender.h>
#include <bgdialogs.h>
#include <bgadvanced.h>

#include <backgndbase.h>
#include <backgnd.h>

/* as late as possible, as it includes some X headers without protecting them */
#include <kwin.h>

#include "advancedDialog.h"

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


/*
 *  Constructs a Backgnd which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 */
Backgnd::Backgnd( QWidget* parent, KConfig *_config, bool _multidesktop,  const char* name, WFlags fl )
    : BackgndBase( parent, name, fl )
{
    m_multidesktop= _multidesktop;
    m_Max = m_multidesktop ? KWin::numberOfDesktops(): 1;
    m_Renderer = QPtrVector<KBackgroundRenderer>( m_Max );
    m_multidesktop= _multidesktop;

    m_pGlobals = new KGlobalBackgroundSettings();


    m_Desk = m_multidesktop ? (KWin::currentDesktop() - 1) : 0;

    if(m_pGlobals->commonBackground())
        m_Desk = 0;

    // get number of desktops
    NETRootInfo info( qt_xdisplay(), NET::NumberOfDesktops | NET::DesktopNames );
    if (info.numberOfDesktops() == 1 || !m_multidesktop)
    {
        m_pDesktopLabel->hide();
        m_pDesktopBox->hide();
    }

    int i;
    for (i=0; i<m_Max; i++) {
        m_Renderer.insert(i, new KBackgroundRenderer(i, _config));
        connect(m_Renderer[i], SIGNAL(imageDone(int)), SLOT(slotPreviewDone(int)));
    }

    // Preview monitor at (0,1)
    m_monitorImage->setPixmap(locate("data", "kcontrol/pics/monitor.png"));
    m_monitorImage->setFixedSize(m_monitorImage->sizeHint());
    m_pMonitor = new KBGMonitor(m_monitorImage, "preview monitor");
    m_pMonitor->setGeometry(23, 14, 151, 115);
    connect(m_pMonitor, SIGNAL(imageDropped(QString)), SLOT(slotImageDropped(QString)));
    QWhatsThis::add( m_pMonitor, i18n("In this monitor, you can preview how your settings will look like on a \"real\" desktop.") );

    if ( m_multidesktop)
    {
        // Desktop names
        for (i=0; i<m_Max; i++)
            m_pDesktopBox->insertItem(m_pGlobals->deskName(i));
    }

}

/*
 *  Destroys the object and frees any allocated resources
 */
Backgnd::~Backgnd()
{
    m_Renderer.setAutoDelete(true);
    delete m_pGlobals;

    // no need to delete child widgets, Qt does it all for us
}

void Backgnd::load()
{
    delete m_pGlobals;
    m_pGlobals = new KGlobalBackgroundSettings();
    int desk = m_Desk;
    if (m_pGlobals->commonBackground())
        desk = 0;
    m_Renderer[desk]->load(desk);

    setWidgets();
    emit changed(false);
}

void Backgnd::save()
{
    kdDebug() << "Saving stuff..." << endl;
    m_pGlobals->writeSettings();
    for (int i=0; i<m_Max; i++)
        m_Renderer[i]->writeSettings();
}

void Backgnd::defaults()
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (r->isActive())
        r->stop();

    if (QPixmap::defaultDepth() > 8) {
        r->setBackgroundMode(_defBackgroundMode);
    }
    else
        r->setBackgroundMode(KBackgroundSettings::Flat);

    r->setColorA(_defColorA);
    r->setColorB(_defColorB);
    r->setWallpaperMode(_defWallpaperMode);
    r->setMultiWallpaperMode(_defMultiMode);
    r->setBlendMode(_defBlendMode);
    r->setBlendBalance(_defBlendBalance);
    r->setReverseBlending(_defReverseBlending);
    m_pGlobals->setCommonBackground(_defCommon);
    m_pGlobals->setLimitCache(_defLimitCache);
    m_pGlobals->setCacheSize(_defCacheSize);

    QStringList lst;
    r->setWallpaperList(lst);
    m_pWallpaperBox->clear();

//    setAcceptDrops(true);

    setWidgets();

    emit changed(true);
}

void Backgnd::setWidgets()
{
    if (m_pGlobals->commonBackground())
        m_pDesktopBox->setCurrentItem(m_Desk);
    else
        m_pDesktopBox->setCurrentItem(m_Desk+1);

    KBackgroundRenderer *r = m_Renderer[m_Desk];


    // Set all the widgets in the color group box
    //
    m_pColorRadio->setChecked(r->backgroundMode() != KBackgroundSettings::Program);
    m_pProgramRadio->setChecked(r->backgroundMode() == KBackgroundSettings::Program);

    m_pColor1But->setColor(r->colorA());
    m_pColor2But->setColor(r->colorB());

    if(r->backgroundMode() != KBackgroundSettings::Program)
       m_pColorBlendBox->setCurrentItem(r->backgroundMode());
    else
       m_pColorBlendBox->setCurrentItem(0);

    if (r->backgroundMode() != KBackgroundSettings::Program)
    {
        m_pPatternEditBut->setEnabled(r->backgroundMode() == KBackgroundSettings::Pattern);
        m_pColor2But->setEnabled(r->backgroundMode() != KBackgroundSettings::Flat);
        m_pColor2Label->setEnabled(r->backgroundMode() != KBackgroundSettings::Flat);
    }
    else
    {
        m_pPatternEditBut->setEnabled(false);
        m_pColor2But->setEnabled(false);
        m_pColor2Label->setEnabled(false);
    }

    // set all the widgets in the wall paper groups box

    if (r->wallpaperMode() < KBackgroundSettings::NoWallpaper)
      m_pWPModeBox->setCurrentItem(r->wallpaperMode());
    else
      m_pWPModeBox->setCurrentItem(0);


    m_pWPBlendBox->setCurrentItem(r->blendMode());
    m_pBalanceSlider->setValue(r->blendBalance());
    m_pChangeInterval->setValue(r->wallpaperChangeInterval());


    if ((r->multiWallpaperMode() == KBackgroundSettings::NoMulti) ||
        (r->multiWallpaperMode() == KBackgroundSettings::InOrder))
       m_pImageOrderBox->setCurrentItem(0);
    else
       m_pImageOrderBox->setCurrentItem(1);

    m_pWallpaperBox->clear();
    m_pWallpaperBox->insertStringList(r->wallpaperList());

    adjustMultiWP();

    // Set the names in the group boxes

    if (m_pGlobals->commonBackground())
    {
       m_pBackgroundGrp->setTitle(i18n("Coloring"));
       m_pWallpaperGrp->setTitle(i18n("Images"));
    }
    else
    {
       QString bgStr(i18n("Coloring for '%1'").arg(m_pGlobals->deskName(m_Desk)));
       m_pBackgroundGrp->setTitle(bgStr);

       QString wpStr(i18n("Images for '%1'").arg(m_pGlobals->deskName(m_Desk)));
       m_pWallpaperGrp->setTitle(wpStr);
    }


    // Start preview render
    r->setPreview(m_pMonitor->size());
    r->start();
}

void Backgnd::slotDesktop(int ipDesktop)
{
    if (ipDesktop==0)
    {
       m_pGlobals->setCommonBackground(true);
       m_Desk = 0;
    }
    else
    {
       m_pGlobals->setCommonBackground(false);
       // The render array starts at 0.
       m_Desk = ipDesktop - 1;
    }

    if (m_Renderer[m_Desk]->isActive())
        m_Renderer[m_Desk]->stop();

    setWidgets();

    emit changed(true);
}

/*
 * Called from the "Background Mode" combobox.
 */
void Backgnd::slotBGMode(int ipRadioState)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    int mode = r->backgroundMode();

    // if Colors is slected
    if (ipRadioState)
    {
       mode = m_pColorBlendBox->currentItem();
       m_pColor1But->setEnabled(true);
       m_pColor1Label->setEnabled(true);
       m_pColorBlendBox->setEnabled(true);
       m_pProgramSetupBut->setEnabled(false);
       m_pPatternEditBut->setEnabled(mode == KBackgroundSettings::Pattern);
       m_pColor2But->setEnabled(mode != KBackgroundSettings::Flat);
       m_pColor2Label->setEnabled(mode != KBackgroundSettings::Flat);
    }
    else // Must be External Program
    {
       mode = KBackgroundSettings::Program;
       m_pColor1But->setEnabled(false);
       m_pColor1Label->setEnabled(false);
       m_pColor2But->setEnabled(false);
       m_pColor2Label->setEnabled(false);
       m_pColorBlendBox->setEnabled(false);
       m_pPatternEditBut->setEnabled(false);
       m_pProgramSetupBut->setEnabled(true);
    }

    r->stop();
    r->setBackgroundMode(mode);

    emit changed(true);
}

/*
 * Called from the "Blending Mode" combobox.
 */
void Backgnd::slotWPBlendMode(int ipMode)
{
    int newMode = ipMode;
    bool reverseBlend = false;

    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (newMode == r->blendMode())
        return;

    if ((ipMode == KBackgroundSettings::IntensityReversed) ||
        (ipMode == KBackgroundSettings::SaturateReversed) ||
        (ipMode == KBackgroundSettings::ContrastReversed) ||
        (ipMode == KBackgroundSettings::HueShiftReversed))
    {
       newMode--;
       reverseBlend = true;
    }

    m_pBalanceSlider->setEnabled(ipMode!=KBackgroundSettings::NoBlending);
    m_pBalanceLbl->setEnabled(ipMode!=KBackgroundSettings::NoBlending);

    r->stop();
    r->setBlendMode(newMode);
    r->setReverseBlending(reverseBlend);
    r->start();

    emit changed(true);
}

/*
 * Called from the "Blending" Slider
 */
void Backgnd::slotWPBlendBalance(int value)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (value == r->blendBalance())
        return;

    r->stop();
    r->setBlendBalance(value);
    r->start();

    emit changed(true);
}

void Backgnd::slotWPSet(int ipSelected)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    r->stop();
    r->setCurrentWallpaper(ipSelected);
    r->start();

    emit changed(true);

}

void Backgnd::slotImageDropped(QString uri)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (uri == r->wallpaper())
        return;

    m_pWallpaperBox->insertItem(uri);
    int pos = m_pWallpaperBox->count() - 1;

    m_pWallpaperBox->setCurrentItem(pos);

    r->stop();
    adjustMultiWP();
    r->setCurrentWallpaper(pos);
    r->start();

    emit changed(true);
}

void Backgnd::slotWPAdd()
{
    KFileDialog fileDialog(KGlobal::dirs()->findDirs("wallpaper", "").first(),
			   KImageIO::pattern(), this,
			   0L, true);

    fileDialog.setCaption(i18n("Select"));
    KFile::Mode mode = static_cast<KFile::Mode> (KFile::Files |
                                                 KFile::Directory |
                                                 KFile::ExistingOnly |
                                                 KFile::LocalOnly);
    fileDialog.setMode(mode);
    fileDialog.exec();
    QStringList files = fileDialog.selectedFiles();
    if (files.isEmpty())
	return;

    // There is at least one wallpaper at this point

    KBackgroundRenderer *r = m_Renderer[m_Desk];

    m_pWallpaperBox->insertStringList(files);

    r->stop();
    adjustMultiWP();
    r->start();

    emit changed(true);
}

void Backgnd::slotWPDelete()
{
    int SelectedItem = m_pWallpaperBox->currentItem();

    if (SelectedItem == -1)
	return;

    KBackgroundRenderer *r = m_Renderer[m_Desk];

    m_pWallpaperBox->removeItem(SelectedItem);

    r->stop();
    adjustMultiWP();
    r->start();

    emit changed(true);
}

void Backgnd::adjustMultiWP()
{
    int count = m_pWallpaperBox->count();

    KBackgroundRenderer *r = m_Renderer[m_Desk];

    // Adjust the rendering settings
    QStringList lst;
    for (unsigned i=0; i<m_pWallpaperBox->count(); i++)
	lst.append(m_pWallpaperBox->text(i));

    r->setWallpaperList(lst);

    if (count == 0)
    {   // No Wall Paper
        r->setWallpaperMode(KBackgroundSettings::NoWallpaper);
	r->setMultiWallpaperMode(KBackgroundSettings::NoMulti);
    }
    else if (count == 1)
    {   // single Wall Paper
        r->setWallpaperMode(m_pWPModeBox->currentItem());
	// This should be no multi but the render doesn't seem to
	// work unless this is set to in Order. This should be
	// fine since one wall paper in order looks like
	// is the same as no multi.
//	r->setMultiWallpaperMode(KBackgroundSettings::NoMulti);
	r->setMultiWallpaperMode(KBackgroundSettings::InOrder);
    }
    else  // multiple Wallpaper
    {
        r->setWallpaperMode(m_pWPModeBox->currentItem());
	if (m_pImageOrderBox->currentItem() == 0)
           r->setMultiWallpaperMode(KBackgroundSettings::InOrder);
	else
           r->setMultiWallpaperMode(KBackgroundSettings::Random);
    }

    bool enable = count > 0;
    bool enableBalance = enable &&
            (m_pWPBlendBox->currentItem() != KBackgroundSettings::NoBlending);
    bool enableMulti = count > 1;
    m_pWPModeBox->setEnabled(enable);
    m_pWPModeLbl->setEnabled(enable);
    m_pWPBlendBox->setEnabled(enable);
    m_pWPBlendLbl->setEnabled(enable);
    m_pBalanceSlider->setEnabled(enableBalance);
    m_pBalanceLbl->setEnabled(enableBalance);
    m_pChangeInterval->setEnabled(enableMulti);
    m_pWPChangeIntervalLbl->setEnabled(enableMulti);
    m_pImageOrderBox->setEnabled(enableMulti);
    m_pWPImageOrderLbl->setEnabled(enableMulti);
    m_pRemoveBut->setEnabled(enable);
}


void Backgnd::slotWPChangeInterval(int ipInterval)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    r->stop();
    r->setWallpaperChangeInterval(ipInterval);

    emit changed(true);

}

void Backgnd::slotImageOrder(int ipMultiMode)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    // increment past NoMulti
    ipMultiMode++;

    r->setMultiWallpaperMode(ipMultiMode);

    emit changed(true);
}


void Backgnd::slotColor1(const QColor &color)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (color == r->colorA())
        return;

    r->stop();
    r->setColorA(color);
    r->start();

    emit changed(true);
}


void Backgnd::slotColor2(const QColor &color)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (color == r->colorB())
        return;

    r->stop();
    r->setColorB(color);
    r->start();

    emit changed(true);
}

/*
 * Called from the "Wallpaper Arrangement" combobox.
 */
void Backgnd::slotWPMode(int mode)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (mode == r->wallpaperMode())
        return;

    r->stop();
    r->setWallpaperMode(mode);
    r->start();

    emit changed(true);
}

void Backgnd::slotPreviewDone(int desk_done)
{
    kdDebug() << "Preview for desktop " << desk_done << " done" << endl;

    if (m_Desk != desk_done)
        return;
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    KPixmap pm;
    if (QPixmap::defaultDepth() < 15)
        pm.convertFromImage(*r->image(), KPixmap::LowColor);
    else
        pm.convertFromImage(*r->image());

    m_pMonitor->setBackgroundPixmap(pm);
}

void Backgnd::slotColorBlendMode(int ipMode)
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    if (ipMode == r->backgroundMode())
        return;

    m_pPatternEditBut->setEnabled(ipMode == KBackgroundSettings::Pattern);

    m_pColor2But->setEnabled(ipMode != KBackgroundSettings::Flat);
    m_pColor2Label->setEnabled(ipMode != KBackgroundSettings::Flat);

    r->stop();
    r->setBackgroundMode(ipMode);
    r->setPreview(m_pMonitor->size());
    r->start();

    emit changed(true);
}

void Backgnd::slotProgramSetup()
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    KProgramSelectDialog dlg;
    QString cur = r->KBackgroundProgram::name();
    dlg.setCurrent(cur);
    if ((dlg.exec() == QDialog::Accepted) && !dlg.program().isEmpty())
    {
        r->stop();
        r->setProgram(dlg.program());
        r->start();
        emit changed(true);
    }

}

void Backgnd::slotPatternEdit()
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];

    KPatternSelectDialog dlg;
    QString cur = r->KBackgroundPattern::name();
    dlg.setCurrent(cur);
    if ((dlg.exec() == QDialog::Accepted) && !dlg.pattern().isEmpty())
    {
        r->stop();
        r->setPatternName(dlg.pattern());
        r->start();
        emit changed(true);
    }

}

void Backgnd::slotAdvanced()
{
    AdvancedDialog* dlg = new AdvancedDialog(m_pGlobals, this);
    dlg->exec();
    delete dlg;
}

void Backgnd::setEnabled( bool state )
{
    KBackgroundRenderer *r = m_Renderer[m_Desk];
    m_pDesktopLabel->setEnabled(state);
    m_pDesktopBox->setEnabled(state);
    m_monitorImage->setEnabled(state);
    m_pAdvancedBut->setEnabled( state );
    m_pWallpaperGrp->setEnabled( state );
    PushButton17->setEnabled( state );
    m_pWallpaperBox->setEnabled( state );
    m_pRemoveBut->setEnabled( state );
    m_pChangeInterval->setEnabled( state );
    m_pWPBlendBox->setEnabled( state );
    m_pWPImageOrderLbl->setEnabled( state );
    m_pWPBlendLbl->setEnabled( state );
    m_pBalanceLbl->setEnabled( state );
    m_pImageOrderBox->setEnabled( state );
    m_pWPChangeIntervalLbl->setEnabled( state );
    m_pWPModeLbl->setEnabled( state );

    m_pBalanceSlider->setEnabled( state );
    m_pWPModeBox->setEnabled( state );
    m_pBackgroundGrp->setEnabled( state );
    m_pProgramRadio->setEnabled( state );
    m_pProgramSetupBut->setEnabled( state );
    m_pPatternEditBut->setEnabled( state );
    m_pColor1Label->setEnabled( state );
    m_pColor2But->setEnabled( state);
    m_pColor2Label->setEnabled( state );
    m_pColor1But->setEnabled( state );
    m_pBlendModeLabel->setEnabled( state );
    m_pColorBlendBox->setEnabled( state );
    m_pColorRadio->setEnabled( state );

    if ( state )
    {
        m_pPatternEditBut->setEnabled(r->backgroundMode() == KBackgroundSettings::Pattern);

        m_pColor2But->setEnabled(r->backgroundMode() != KBackgroundSettings::Flat);
        m_pColor2Label->setEnabled(r->backgroundMode() != KBackgroundSettings::Flat);
        m_pBalanceSlider->setEnabled(r->backgroundMode()!=KBackgroundSettings::NoBlending);
        m_pBalanceLbl->setEnabled(r->backgroundMode()!=KBackgroundSettings::NoBlending);
        adjustMultiWP();
    }

}

void Backgnd::makeReadOnly()
{
    m_pDesktopLabel->setEnabled(false);
    m_pDesktopBox->setEnabled(false);
    m_monitorImage->setEnabled(false);
    m_pAdvancedBut->setEnabled( false );
    m_pWallpaperGrp->setEnabled( false );
    PushButton17->setEnabled( false );
    m_pWallpaperBox->setEnabled( false );
    m_pRemoveBut->setEnabled( false );
    m_pChangeInterval->setEnabled( false );
    m_pWPBlendBox->setEnabled( false );
    m_pWPImageOrderLbl->setEnabled( false );
    m_pWPBlendLbl->setEnabled( false );
    m_pBalanceLbl->setEnabled( false );
    m_pImageOrderBox->setEnabled( false );
    m_pWPChangeIntervalLbl->setEnabled( false );
    m_pWPModeLbl->setEnabled( false );

    m_pBalanceSlider->setEnabled( false );
    m_pWPModeBox->setEnabled( false );
    m_pBackgroundGrp->setEnabled( false );
    m_pProgramRadio->setEnabled( false );
    m_pProgramSetupBut->setEnabled( false );
    m_pPatternEditBut->setEnabled( false );
    m_pColor1Label->setEnabled( false );
    m_pColor2But->setEnabled( false);
    m_pColor2Label->setEnabled( false );
    m_pColor1But->setEnabled( false );
    m_pBlendModeLabel->setEnabled( false );
    m_pColorBlendBox->setEnabled( false );
    m_pColorRadio->setEnabled( false );
}

#include "backgnd.moc"

