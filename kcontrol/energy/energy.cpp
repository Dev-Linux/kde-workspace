/* vi: ts=8 sts=4 sw=4
 *
 * $Id$
 *
 * This file is part of the KDE project, module kcontrol.
 * Copyright (C) 1999 Geert Jansen <g.t.jansen@stud.tue.nl>
 *
 * You can Freely distribute this program under the GNU General Public
 * License. See the file "COPYING" for the exact licensing terms.
 *
 * Based on kcontrol1 energy.cpp, Copyright (c) 1999 Tom Vijlbrief
 */


/*
 * KDE Energy setup module.
 */

#include <config.h>

#include <qglobal.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>

#include <kglobal.h>
#include <kconfig.h>
#include <kstddirs.h>
#include <kiconloader.h>
#include <knuminput.h>
#include <klocale.h>
#include <kcmodule.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include "energy.h"


#ifdef HAVE_DPMS
#include <X11/Xmd.h>
extern "C" {
#include <X11/extensions/dpms.h>
int __kde_do_not_unload = 1;
}

#ifdef XIMStringConversionRetrival
extern "C" {
#endif
    Bool DPMSQueryExtension(Display *, int *, int *);
    Status DPMSEnable(Display *);
    Status DPMSDisable(Display *);
    Bool DPMSSetTimeouts(Display *, CARD16, CARD16, CARD16);
#ifdef XIMStringConversionRetrival
}
#endif
#endif


static const int DFLT_STANDBY   = 0;
static const int DFLT_SUSPEND   = 30;
static const int DFLT_OFF   = 60;



/**** DLL Interface ****/

extern "C" {

    KCModule *create_energy(QWidget *parent, char *name) {
    KGlobal::locale()->insertCatalogue("kcmenergy");
    return new KEnergy(parent, name);
    }

    void init_energy() {
    KConfig *cfg = new KConfig("kcmdisplayrc");
    cfg->setGroup("DisplayEnergy");

    bool enabled = cfg->readBoolEntry("displayEnergySaving", false);
    int standby = cfg->readNumEntry("displayStandby", DFLT_STANDBY);
    int suspend = cfg->readNumEntry("displaySuspend", DFLT_SUSPEND);
    int off = cfg->readNumEntry("displayPowerOff", DFLT_OFF);

    delete cfg;

    KEnergy::applySettings(enabled, standby, suspend, off);
    }
}



/**** KEnergy ****/

KEnergy::KEnergy(QWidget *parent, const char *name)
    : KCModule(parent, name)
{
    m_bChanged = false;
    m_bEnabled = false;
    m_Standby = DFLT_STANDBY;
    m_Suspend = DFLT_SUSPEND;
    m_Off = DFLT_OFF;
    m_bDPMS = false;

#ifdef HAVE_DPMS
    int dummy;
    m_bDPMS = DPMSQueryExtension(qt_xdisplay(), &dummy, &dummy);
#endif

    QVBoxLayout *top = new QVBoxLayout(this, 10, 10);
    QHBoxLayout *hbox = new QHBoxLayout();
    top->addLayout(hbox);

    QLabel *lbl;
    if (m_bDPMS) {
    m_pCBEnable= new QCheckBox(i18n("&Enable Display Energy Saving" ), this);
    connect(m_pCBEnable, SIGNAL(toggled(bool)), SLOT(slotChangeEnable(bool)));
    hbox->addWidget(m_pCBEnable);
        QWhatsThis::add( m_pCBEnable, i18n("Check this option to enable the"
           " power saving features of your display.") );
    } else {
    lbl = new QLabel(i18n("Your display has NO power saving features!"), this);
    hbox->addWidget(lbl);
    }

    lbl= new QLabel(this);
    lbl->setPixmap(QPixmap(locate("data", "kcontrol/pics/energybig.png")));
    hbox->addStretch();
    hbox->addWidget(lbl);

    // Sliders
    m_pStandbySlider = new KIntNumInput(m_Standby, this);
    m_pStandbySlider->setLabel(i18n("&Standby after:"));
    m_pStandbySlider->setRange(0, 120, 10);
    m_pStandbySlider->setSuffix(i18n(" min"));
    m_pStandbySlider->setSpecialValueText(i18n("Disabled"));
    connect(m_pStandbySlider, SIGNAL(valueChanged(int)), SLOT(slotChangeStandby(int)));
    top->addWidget(m_pStandbySlider);
    QWhatsThis::add( m_pStandbySlider, i18n("Choose the period of inactivity"
       " after which the display should enter \"standby\" mode. This is the"
       " first level of power saving.") );

    m_pSuspendSlider = new KIntNumInput(m_pStandbySlider, m_Suspend, this);
    m_pSuspendSlider->setLabel(i18n("S&uspend after:"));
    m_pSuspendSlider->setRange(0, 120, 10);
    m_pSuspendSlider->setSuffix(i18n(" min"));
    m_pSuspendSlider->setSpecialValueText(i18n("Disabled"));
    connect(m_pSuspendSlider, SIGNAL(valueChanged(int)), SLOT(slotChangeSuspend(int)));
    top->addWidget(m_pSuspendSlider);
    QWhatsThis::add( m_pSuspendSlider, i18n("Choose the period of inactivity"
       " after which the display should enter \"suspend\" mode. This is the"
       " second level of power saving, but for some displays, may not be"
       " different from the first level.") );

    m_pOffSlider = new KIntNumInput(m_pSuspendSlider, m_Off, this);
    m_pOffSlider->setLabel(i18n("&Power Off after:"));
    m_pOffSlider->setRange(0, 120, 10);
    m_pOffSlider->setSuffix(i18n(" min"));
    m_pOffSlider->setSpecialValueText(i18n("Disabled"));
    connect(m_pOffSlider, SIGNAL(valueChanged(int)), SLOT(slotChangeOff(int)));
    top->addWidget(m_pOffSlider);
    QWhatsThis::add( m_pOffSlider, i18n("Choose the period of inactivity"
       " after which the display should be powered off. This is the"
       " greatest level of power saving that can be achieved while the"
       " display is still physically turned on.") );

    top->addStretch();

    m_pConfig = new KConfig("kcmdisplayrc");
    m_pConfig->setGroup("DisplayEnergy");

    load();
    setButtons(buttons());
}


KEnergy::~KEnergy()
{
    delete m_pConfig;
}


int KEnergy::buttons()
{
    if (m_bDPMS)
    return KCModule::Help | KCModule::Default | KCModule::Reset |
           KCModule::Cancel | KCModule::Ok;
    else
    return KCModule::Help | KCModule::Ok;
}


void KEnergy::load()
{
    readSettings();
    showSettings();

    emit changed(false);
}


void KEnergy::save()
{
    writeSettings();
    applySettings(m_bEnabled, m_Standby, m_Suspend, m_Off);

    emit changed(false);
}


void KEnergy::defaults()
{
    m_bEnabled = false;
    m_Standby = DFLT_STANDBY;
    m_Suspend = DFLT_SUSPEND;
    m_Off = DFLT_OFF;

    showSettings();
    emit changed(true);
}


void KEnergy::readSettings()
{
    m_bEnabled = m_pConfig->readBoolEntry("displayEnergySaving", false);
    m_Standby = m_pConfig->readNumEntry("displayStandby", DFLT_STANDBY);
    m_Suspend = m_pConfig->readNumEntry("displaySuspend", DFLT_SUSPEND);
    m_Off = m_pConfig->readNumEntry("displayPowerOff", DFLT_OFF);

    m_bChanged = false;
}


void KEnergy::writeSettings()
{
    if (!m_bChanged)
     return;

    m_pConfig->writeEntry( "displayEnergySaving", m_bEnabled);
    m_pConfig->writeEntry("displayStandby", m_Standby);
    m_pConfig->writeEntry("displaySuspend", m_Suspend);
    m_pConfig->writeEntry("displayPowerOff", m_Off);

    m_pConfig->sync();
    m_bChanged = false;
}


void KEnergy::showSettings()
{
    if (m_bDPMS)
    m_pCBEnable->setChecked(m_bEnabled);

    m_pStandbySlider->setEnabled(m_bEnabled);
    m_pStandbySlider->setValue(m_Standby);
    m_pSuspendSlider->setEnabled(m_bEnabled);
    m_pSuspendSlider->setValue(m_Suspend);
    m_pOffSlider->setEnabled(m_bEnabled);
    m_pOffSlider->setValue(m_Off);
}


int dropError(Display *, XErrorEvent *)
{
    return 0;
}

/* static */
void KEnergy::applySettings(bool enable, int standby, int suspend, int off)
{
#ifdef HAVE_DPMS
    int (*defaultHandler)(Display *, XErrorEvent *);
    defaultHandler = XSetErrorHandler(dropError);

    Display *dpy = qt_xdisplay();

    int dummy;
    bool hasDPMS = DPMSQueryExtension(dpy, &dummy, &dummy);
    if (hasDPMS) {
    if (enable) {
            DPMSEnable(dpy);
            DPMSSetTimeouts(dpy, 60*standby, 60*suspend, 60*off);
        } else
            DPMSDisable(dpy);
    } else
    qWarning("Server has no DPMS extension");

    XFlush(dpy);
    XSetErrorHandler(defaultHandler);
#else
    /* keep gcc silent */
    if (enable | standby | suspend | off)
    /* nothing */ ;
#endif
}


void KEnergy::slotChangeEnable(bool ena)
{
    m_bEnabled = ena;
    m_bChanged = true;

    m_pStandbySlider->setEnabled(ena);
    m_pSuspendSlider->setEnabled(ena);
    m_pOffSlider->setEnabled(ena);
}


void KEnergy::slotChangeStandby(int value)
{
    m_Standby = value;
    if (m_Suspend > 0 && m_Standby > m_Suspend)
    m_pSuspendSlider->setValue(m_Standby);
    else if (m_Off > 0 && m_Standby > m_Off)
    m_pOffSlider->setValue(m_Standby);

    m_bChanged = true;
    emit changed(true);
}


void KEnergy::slotChangeSuspend(int value)
{
    m_Suspend = value;
    if (m_Off > 0 && m_Suspend > m_Off)
    m_pOffSlider->setValue(m_Suspend);
    if (m_Suspend < m_Standby)
    m_pStandbySlider->setValue(m_Suspend);

    m_bChanged = true;
    emit changed(true);
}


void KEnergy::slotChangeOff(int value)
{
    m_Off = value;
    if (m_Off < m_Suspend)
    m_pSuspendSlider->setValue(m_Off);

    m_bChanged = true;
    emit changed(true);
}


QString KEnergy::quickHelp() const
{
    return i18n("<h1>Energy Saving For Display</h1> If your display supports"
      " power saving features, you can configure them using this module.<p>"
      " There are three levels of power saving: standby, suspend, and off."
      " The greater the level of power saving, the longer it takes for the"
      " display to return to an active state.<p>"
      " To wake up the display from a power saving mode, you can make a small"
      " movement with the mouse, or press a key that is not likely to cause"
      " any unintended side-effects, for example, the \"shift\" key.");
}


#include "energy.moc"
