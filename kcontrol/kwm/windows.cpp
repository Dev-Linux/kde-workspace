/*
 * windows.cpp
 *
 * Copyright (c) 1997 Patrick Dowler dowler@morgul.fsh.uvic.ca
 *
 * Requires the Qt widget libraries, available at no cost at
 * http://www.troll.no/
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

#include <qlayout.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qwhatsthis.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qlabel.h>
#include <qcombobox.h>
#include <qlcdnumber.h>

#include <kapp.h>
#include <klocale.h>
#include <kconfig.h>
#include <knuminput.h>
#include <kdialog.h>
#include <kapp.h>
#include <dcopclient.h>

#include <kglobal.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "windows.h"
#include "geom.h"


// kwin config keywords
#define KWIN_FOCUS     "FocusPolicy"
#define KWIN_PLACEMENT "Placement"
#define KWIN_MOVE      "MoveMode"
#define KWIN_MINIMIZE_ANIM    "AnimateMinimize"
#define KWIN_MINIMIZE_ANIM_SPEED    "AnimateMinimizeSpeed"
#define KWIN_RESIZE_OPAQUE    "ResizeMode"
#define KWIN_AUTORAISE_INTERVAL "AutoRaiseInterval"
#define KWIN_AUTORAISE "AutoRaise"
#define KWIN_CLICKRAISE "ClickRaise"
#define KWIN_ALTTABMODE "AltTabStyle"
#define KWIN_CTRLTAB "ControlTab"


extern "C" {
    KCModule *create_kwinoptions ( QWidget *parent, const char *name )
    {
        //CT there's need for decision: kwm or kwin?
        KGlobal::locale()->insertCatalogue("kcmkwm");
        return new KWindowConfig( parent, name );
    }
}

KWindowConfig::~KWindowConfig ()
{
}

KWindowConfig::KWindowConfig (QWidget * parent, const char *name)
    : KCModule (parent, name)
{
    QString wtstr;
    QBoxLayout *lay = new QVBoxLayout (this, KDialog::marginHint(),
                                       KDialog::spacingHint());

    windowsBox = new QButtonGroup(i18n("Windows"), this);

    QBoxLayout *wLay = new QVBoxLayout (windowsBox,KDialog::marginHint(),
                                        KDialog::spacingHint());
    wLay->addSpacing(fontMetrics().lineSpacing());

    QBoxLayout *bLay = new QVBoxLayout;
    wLay->addLayout(bLay);

    opaque = new QCheckBox(i18n("Display content in moving windows"), windowsBox);
    bLay->addWidget(opaque);
    QWhatsThis::add( opaque, i18n("Enable this option if you want a window's content to be fully shown"
                                  " while moving it, instead of just showing a window 'skeleton'. The result may not be satisfying"
                                  " on slow machines without graphic acceleration.") );

    resizeOpaqueOn = new QCheckBox(i18n("Display content in resizing windows"), windowsBox);
    bLay->addWidget(resizeOpaqueOn);
    QWhatsThis::add( resizeOpaqueOn, i18n("Enable this option if you want a window's content to be shown"
                                          " while resizing it, instead of just showing a window 'skeleton'. The result may not be satisfying"
                                          " on slow machines.") );

    animateShade = new QCheckBox(i18n("Animate window shading"), windowsBox);
    QWhatsThis::add(animateShade, i18n("Animate the action of reducing the window to its titlebar (shading)"
                                       " as well as the expansion of a shaded window") );
    bLay->addWidget(animateShade);

    QGridLayout *rLay = new QGridLayout(2,3);
    bLay->addLayout(rLay);
    rLay->setColStretch(0,0);
    rLay->setColStretch(1,1);

    minimizeAnimOn = new QCheckBox(i18n("Minimize animation"),
                                   windowsBox);
    QWhatsThis::add( minimizeAnimOn, i18n("Enable this option if you want an animation shown when"
                                          " windows are minimized or restored." ) );
    rLay->addWidget(minimizeAnimOn,0,0);

    minimizeAnimSlider = new QSlider(0,10,10,0,QSlider::Horizontal, windowsBox);
    minimizeAnimSlider->setSteps(10,1);
    rLay->addMultiCellWidget(minimizeAnimSlider,0,0,1,2);

    minimizeAnimSlowLabel= new QLabel(i18n("Slow"),windowsBox);
    minimizeAnimSlowLabel->setAlignment(AlignTop|AlignLeft);
    rLay->addWidget(minimizeAnimSlowLabel,1,1);

    minimizeAnimFastLabel= new QLabel(i18n("Fast"),windowsBox);
    minimizeAnimFastLabel->setAlignment(AlignTop|AlignRight);
    rLay->addWidget(minimizeAnimFastLabel,1,2);

    wtstr = i18n("Here you can set the speed for the animation shown when windows are"
                 " minimized and restored. ");
    QWhatsThis::add( minimizeAnimSlider, wtstr );
    QWhatsThis::add( minimizeAnimSlowLabel, wtstr );
    QWhatsThis::add( minimizeAnimFastLabel, wtstr );

    //CT 17Dec2000 - maybe restore this button box when placement becomes again what was in KDE-1
    // placement policy --- CT 19jan98, 13mar98 ---
    //plcBox = new QButtonGroup(i18n("Placement policy"),this);

    //QGridLayout *pLay = new QGridLayout(plcBox,3,3,
    //                    KDialog::marginHint(),
    //                    KDialog::spacingHint());
    //pLay->addRowSpacing(0,fontMetrics().lineSpacing());

    bLay->addSpacing(20);

    rLay = new QGridLayout(1,3);
    bLay->addLayout(rLay);
    rLay->setColStretch(0,1);
    rLay->setColStretch(1,3);
    rLay->setColStretch(2,2);

    QLabel *plcLabel = new QLabel(i18n("Placement:"),windowsBox);

    placementCombo = new QComboBox(false, windowsBox);
    placementCombo->insertItem(i18n("Smart"), SMART_PLACEMENT);
    placementCombo->insertItem(i18n("Cascade"), CASCADE_PLACEMENT);
    placementCombo->insertItem(i18n("Random"), RANDOM_PLACEMENT);
    // CT: disabling is needed as long as functionality misses in kwin
    //placementCombo->insertItem(i18n("Interactive"), INTERACTIVE_PLACEMENT);
    //placementCombo->insertItem(i18n("Manual"), MANUAL_PLACEMENT);
    placementCombo->setCurrentItem(SMART_PLACEMENT);

    connect(placementCombo, SIGNAL(activated(int)),this,
            SLOT(ifPlacementIsInteractive()) );

    // FIXME, when more policies have been added to KWin
    wtstr = i18n("The placement policy determines where a new window"
                 " will appear on the desktop. For now, there are three different policies:"
                 " <ul><li><em>Smart</em> will try to achieve a minimum overlap of windows</li>"
                 " <li><em>Cascade</em> will cascade the windows</li>"
                 " <li><em>Random</em> will use a random position</li></ul>") ;

    QWhatsThis::add( plcLabel, wtstr);
    QWhatsThis::add( placementCombo, wtstr);

    plcLabel->setBuddy(placementCombo);
    rLay->addWidget(plcLabel, 0, 0);
    rLay->addWidget(placementCombo, 0, 1);

    bLay->addSpacing(10);

    lay->addWidget(windowsBox);

    //iTLabel = new QLabel(i18n("  Allowed overlap:\n"
    //                         "(% of desktop space)"),
    //             plcBox);
    //iTLabel->setAlignment(AlignTop|AlignHCenter);
    //pLay->addWidget(iTLabel,1,1);

    //interactiveTrigger = new QSpinBox(0, 500, 1, plcBox);
    //pLay->addWidget(interactiveTrigger,1,2);

    //pLay->addRowSpacing(2,KDialog::spacingHint());

    //lay->addWidget(plcBox);

    // focus policy
    fcsBox = new QButtonGroup(i18n("Focus policy"),this);

    QGridLayout *fLay = new QGridLayout(fcsBox,5,3,
                                        KDialog::marginHint(),
                                        KDialog::spacingHint());
    fLay->addRowSpacing(0,fontMetrics().lineSpacing());
    fLay->setColStretch(0,0);
    fLay->setColStretch(1,1);
    fLay->setColStretch(2,1);


    focusCombo =  new QComboBox(false, fcsBox);
    focusCombo->insertItem(i18n("Click to focus"), CLICK_TO_FOCUS);
    focusCombo->insertItem(i18n("Focus follows mouse"), FOCUS_FOLLOWS_MOUSE);
    focusCombo->insertItem(i18n("Focus under mouse"), FOCUS_UNDER_MOUSE);
    focusCombo->insertItem(i18n("Focus strictly under mouse"), FOCUS_STRICTLY_UNDER_MOUSE);
    fLay->addMultiCellWidget(focusCombo,1,1,0,1);

    // FIXME, when more policies have been added to KWin
    QWhatsThis::add( focusCombo, i18n("The focus policy is used to determin the active window, i.e."
                                      " the window you can work in. <ul>"
                                      " <li><em>Click to focus:</em> a window becomes active when you click into it. This is the behavior"
                                      " you might know from other operating systems.</li>"
                                      " <li><em>Focus follows mouse:</em> Moving the mouse pointer actively onto a"
                                      " normal window activates it. Very practical if you are using the mouse a lot.</li>"
                                      " <li><em>FocusUnderMouse</em> - The window that happens to be under the"
                                      "  mouse pointer becomes active. The invariant is: no window can"
                                      "  have focus that is not under the mouse. </li>"
                                      " <li><em>FocusStrictlyUnderMouse</em> - this is even worse than"
                                      " FocusUnderMouse. Only the window under the mouse pointer is"
                                      " active. If the mouse points nowhere, nothing has the focus. "
                                      " </ul>"
                                      "  Note that FocusUnderMouse and FocusStrictlyUnderMouse are not"
                                      " particularly useful. They are only provided for old-fashioned"
                                      " die-hard UNIX people ;-)"
                         ) );

    connect(focusCombo, SIGNAL(activated(int)),this,
            SLOT(setAutoRaiseEnabled()) );

    // autoraise delay

    autoRaiseOn = new QCheckBox(i18n("Auto Raise"), fcsBox);
    fLay->addWidget(autoRaiseOn,2,0);
    connect(autoRaiseOn,SIGNAL(toggled(bool)), this, SLOT(autoRaiseOnTog(bool)));

    clickRaiseOn = new QCheckBox(i18n("Click Raise"), fcsBox);
    fLay->addWidget(clickRaiseOn,4,0);

    connect(clickRaiseOn,SIGNAL(toggled(bool)), this, SLOT(clickRaiseOnTog(bool)));

    alabel = new QLabel(i18n("Delay (ms)"), fcsBox);
    alabel->setAlignment(AlignVCenter|AlignHCenter);
    fLay->addWidget(alabel,2,1,AlignLeft);

    s = new QLCDNumber (4, fcsBox);
    s->setFrameStyle( QFrame::NoFrame );
    s->setFixedHeight(30);
    s->adjustSize();
    s->setMinimumSize(s->size());
    fLay->addWidget(s,2,2);

    autoRaise = new KIntNumInput(500, fcsBox);
    autoRaise->setRange(0, 3000, 100, true);
    autoRaise->setSteps(100,100);
    fLay->addMultiCellWidget(autoRaise,3,3,1,2);
    connect( autoRaise, SIGNAL(valueChanged(int)), s, SLOT(display(int)) );

    fLay->addColSpacing(0,QMAX(autoRaiseOn->sizeHint().width(),
                               clickRaiseOn->sizeHint().width()) + 15);

    QWhatsThis::add( autoRaiseOn, i18n("If Auto Raise is enabled, a window in the background will automatically"
                                       " come to front when the mouse pointer has been over it for some time.") );
    wtstr = i18n("This is the delay after which the window the mouse pointer is over will automatically"
                 " come to front.");
    QWhatsThis::add( autoRaise, wtstr );
    QWhatsThis::add( s, wtstr );
    QWhatsThis::add( alabel, wtstr );

    QWhatsThis::add( clickRaiseOn, i18n("Disable this option if you don't want windows to be brought to"
                                        " front automatically when you click somewhere into the window contents.") );

    lay->addWidget(fcsBox);


    kbdBox = new QButtonGroup(i18n("Keyboard"), this);
    QGridLayout *kLay = new QGridLayout(kbdBox, 3, 4,
                                        KDialog::marginHint(),
                                        KDialog::spacingHint());
    kLay->addRowSpacing(0,10);
    QLabel *altTabLabel = new QLabel( i18n("Alt-Tab mode:"), kbdBox);
    kLay->addWidget(altTabLabel, 1, 0);
    kdeMode = new QRadioButton(i18n("KDE"), kbdBox);
    kLay->addWidget(kdeMode, 1, 1);
    cdeMode = new QRadioButton(i18n("CDE"), kbdBox);
    kLay->addWidget(cdeMode, 1, 2);
    kdeMode->setChecked(true);

    wtstr = i18n("Keep the Alt key pressed and hit repeatedly the Tab key to walk"
                 " throught the windows on the current desktop. The two different modes mean:<ul>"
                 "<li><b>KDE</b>: a nice widget is shown, displaying the icons of all windows to"
                 " walk through and the title of the currently selected one;"
                 "<li><b>CDE</b>: the focus is passed to a new window at each time Tab is hit."
                 " No fancy widget.</li></ul>");
    QWhatsThis::add( altTabLabel, wtstr );
    QWhatsThis::add( kdeMode, wtstr );
    QWhatsThis::add( cdeMode, wtstr );

    ctrlTab = new QCheckBox( i18n("Use Ctrl-Tab to walk through desktops"), kbdBox);
    kLay->addMultiCellWidget(ctrlTab, 2, 2, 0, 3);

    QWhatsThis::add(ctrlTab, i18n("Keep the Ctrl key pressed and hit repeatedly the Tab key to walk"
                                  " through the virtual desktops of your work space.<p>Disable this to"
                                  " avoid interference with other programs that hardwire the same keyboard"
                                  " combination for internal usage."));

    lay->addWidget(kbdBox);

    lay->addStretch();

    // Any changes goes to slotChanged()
    connect(opaque, SIGNAL(clicked()), this, SLOT(slotChanged()));
    connect(resizeOpaqueOn, SIGNAL(clicked()), this, SLOT(slotChanged()));
    connect(minimizeAnimOn, SIGNAL(clicked() ), SLOT(slotChanged()));
    connect(minimizeAnimSlider, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
    connect(placementCombo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
    connect(focusCombo, SIGNAL(activated(int)), this, SLOT(slotChanged()));
    connect(fcsBox, SIGNAL(clicked(int)), this, SLOT(slotChanged()));
    connect(autoRaise, SIGNAL(valueChanged(int)), this, SLOT(slotChanged()));
    connect(kdeMode, SIGNAL(clicked()), this, SLOT(slotChanged()));
    connect(cdeMode, SIGNAL(clicked()), this, SLOT(slotChanged()));
    connect(ctrlTab, SIGNAL(clicked()), this, SLOT(slotChanged()));

    load();

}

// many widgets connect to this slot
void KWindowConfig::slotChanged()
{
    emit changed(true);
}

int KWindowConfig::getMove()
{
    if (opaque->isChecked())
        return OPAQUE;
    else
        return TRANSPARENT;
}

void KWindowConfig::setMove(int trans)
{
    if (trans == TRANSPARENT)
        opaque->setChecked(false);
    else
        opaque->setChecked(true);
}

// placement policy --- CT 31jan98 ---
int KWindowConfig::getPlacement()
{
    return placementCombo->currentItem();
}

void KWindowConfig::setPlacement(int plac)
{
    placementCombo->setCurrentItem(plac);
}

int KWindowConfig::getFocus()
{
    return focusCombo->currentItem();
}

void KWindowConfig::setFocus(int foc)
{
    focusCombo->setCurrentItem(foc);

    // this will disable/hide the auto raise delay widget if focus==click
    setAutoRaiseEnabled();
}

bool KWindowConfig::getMinimizeAnim()
{
    return minimizeAnimOn->isChecked();
}

int KWindowConfig::getMinimizeAnimSpeed()
{
    return minimizeAnimSlider->value();
}

void KWindowConfig::setMinimizeAnim(bool anim, int speed)
{
    minimizeAnimOn->setChecked( anim );
    minimizeAnimSlider->setValue(speed);
    minimizeAnimSlider->setEnabled( anim );
    minimizeAnimSlowLabel->setEnabled( anim );
    minimizeAnimFastLabel->setEnabled( anim );
}

int KWindowConfig::getResizeOpaque()
{
    if (resizeOpaqueOn->isChecked())
        return RESIZE_OPAQUE;
    else
        return RESIZE_TRANSPARENT;
}

void KWindowConfig::setResizeOpaque(int opaque)
{
    if (opaque == RESIZE_OPAQUE)
        resizeOpaqueOn->setChecked(true);
    else
        resizeOpaqueOn->setChecked(false);
}

void KWindowConfig::setAutoRaiseInterval(int tb)
{
    autoRaise->setValue(tb);
    s->display(tb);
}

int KWindowConfig::getAutoRaiseInterval()
{
    return s->intValue();
}

void KWindowConfig::setAutoRaise(bool on)
{
    autoRaiseOn->setChecked(on);
}

void KWindowConfig::setClickRaise(bool on)
{
    clickRaiseOn->setChecked(on);
}

void KWindowConfig::setAutoRaiseEnabled()
{
    // the auto raise related widgets are: autoRaise, alabel, s, sec
    if ( focusCombo->currentItem() != CLICK_TO_FOCUS )
    {
        clickRaiseOn->setEnabled(true);
        clickRaiseOnTog(clickRaiseOn->isChecked());
        autoRaiseOn->setEnabled(true);
        autoRaiseOnTog(autoRaiseOn->isChecked());
    }
    else
    {
        autoRaiseOn->setEnabled(false);
        autoRaiseOnTog(false);
        clickRaiseOn->setEnabled(false);
        clickRaiseOnTog(false);
    }
}


// CT 13mar98 interactiveTrigger configured by this slot
// void KWindowConfig::ifPlacementIsInteractive( )
// {
//   if( placementCombo->currentItem() == INTERACTIVE_PLACEMENT) {
//     iTLabel->setEnabled(true);
//     interactiveTrigger->show();
//   }
//   else {
//     iTLabel->setEnabled(false);
//     interactiveTrigger->hide();
//   }
// }
//CT

//CT 23Oct1998 make AutoRaise toggling much clear
void KWindowConfig::autoRaiseOnTog(bool a) {
    autoRaise->setEnabled(a);
    s->setEnabled(a);
    alabel->setEnabled(a);
    clickRaiseOn->setEnabled( !a );
    if ( a )
        clickRaiseOn->setChecked( TRUE );

}
//CT

void KWindowConfig::clickRaiseOnTog(bool ) {
}

void KWindowConfig::setAltTabMode(bool a) {
    kdeMode->setChecked(a);
}

void KWindowConfig::setCtrlTab(bool a) {
    ctrlTab->setChecked(a);
}

void KWindowConfig::load( void )
{
    QString key;

    KConfig *config = new KConfig("kwinrc", false, false);
    config->setGroup( "Windows" );

    key = config->readEntry(KWIN_MOVE, "Opaque");
    if( key == "Transparent")
        setMove(TRANSPARENT);
    else if( key == "Opaque")
        setMove(OPAQUE);

    //CT 17Jun1998 - variable animation speed from 0 (none!!) to 10 (max)
    int anim = 1;
    if (config->hasKey(KWIN_MINIMIZE_ANIM_SPEED)) {
        anim = config->readNumEntry(KWIN_MINIMIZE_ANIM_SPEED);
        if( anim < 1 ) anim = 0;
        if( anim > 10 ) anim = 10;
        setMinimizeAnim( config->readBoolEntry("KWIN_MINIMIZE_ANIM", true ), anim );
    }
    else{
        config->writeEntry(KWIN_MINIMIZE_ANIM, true );
        config->writeEntry(KWIN_MINIMIZE_ANIM_SPEED, 5);
        setMinimizeAnim(true, 5);
    }

    // DF: please keep the default consistent with kwin (options.cpp line 145)
    key = config->readEntry(KWIN_RESIZE_OPAQUE, "Opaque");
    if( key == "Opaque")
        setResizeOpaque(RESIZE_OPAQUE);
    else if ( key == "Transparent")
        setResizeOpaque(RESIZE_TRANSPARENT);

    // placement policy --- CT 19jan98 ---
    key = config->readEntry(KWIN_PLACEMENT);
    //CT 13mar98 interactive placement
//   if( key.left(11) == "interactive") {
//     setPlacement(INTERACTIVE_PLACEMENT);
//     int comma_pos = key.find(',');
//     if (comma_pos < 0)
//       interactiveTrigger->setValue(0);
//     else
//       interactiveTrigger->setValue (key.right(key.length()
//                           - comma_pos).toUInt(0));
//     iTLabel->setEnabled(true);
//     interactiveTrigger->show();
//   }
//   else {
//     interactiveTrigger->setValue(0);
//     iTLabel->setEnabled(false);
//     interactiveTrigger->hide();
    if( key == "Random")
        setPlacement(RANDOM_PLACEMENT);
    else if( key == "Cascade")
        setPlacement(CASCADE_PLACEMENT); //CT 31jan98
    //CT 31mar98 manual placement
    else if( key == "manual")
        setPlacement(MANUAL_PLACEMENT);

    else
        setPlacement(SMART_PLACEMENT);
//  }

    key = config->readEntry(KWIN_FOCUS);
    if( key == "ClickToFocus")
        setFocus(CLICK_TO_FOCUS);
    else if( key == "FocusFollowsMouse")
        setFocus(FOCUS_FOLLOWS_MOUSE);
    else if(key == "FocusUnderMouse")
        setFocus(FOCUS_UNDER_MOUSE);
    else if(key == "FocusStrictlyUnderMouse")
        setFocus(FOCUS_STRICTLY_UNDER_MOUSE);

    int k = config->readNumEntry(KWIN_AUTORAISE_INTERVAL,0);
    setAutoRaiseInterval(k);

    key = config->readEntry(KWIN_AUTORAISE);
    setAutoRaise(key == "on");
    key = config->readEntry(KWIN_CLICKRAISE);
    setClickRaise(key != "off");
    setAutoRaiseEnabled();      // this will disable/hide the auto raise delay widget if focus==click

    key = config->readEntry(KWIN_ALTTABMODE, "KDE");
    setAltTabMode(key == "KDE");

    setCtrlTab(config->readBoolEntry(KWIN_CTRLTAB, true));


delete config;
}

void KWindowConfig::save( void )
{
int v;

KConfig *config = new KConfig("kwinrc", false, false);
config->setGroup( "Windows" );

v = getMove();
if (v == TRANSPARENT)
    config->writeEntry(KWIN_MOVE,"Transparent");
else
config->writeEntry(KWIN_MOVE,"Opaque");


// placement policy --- CT 31jan98 ---
v =getPlacement();
if (v == RANDOM_PLACEMENT)
    config->writeEntry(KWIN_PLACEMENT, "Random");
else if (v == CASCADE_PLACEMENT)
    config->writeEntry(KWIN_PLACEMENT, "Cascade");
//CT 13mar98 manual and interactive placement
//   else if (v == MANUAL_PLACEMENT)
//     config->writeEntry(KWIN_PLACEMENT, "Manual");
//   else if (v == INTERACTIVE_PLACEMENT) {
//       QString tmpstr = QString("Interactive,%1").arg(interactiveTrigger->value());
//       config->writeEntry(KWIN_PLACEMENT, tmpstr);
//   }
else
config->writeEntry(KWIN_PLACEMENT, "Smart");


v = getFocus();
if (v == CLICK_TO_FOCUS)
    config->writeEntry(KWIN_FOCUS,"ClickToFocus");
else if (v == FOCUS_UNDER_MOUSE)
    config->writeEntry(KWIN_FOCUS,"FocusUnderMouse");
else if (v == FOCUS_STRICTLY_UNDER_MOUSE)
    config->writeEntry(KWIN_FOCUS,"FocusStrictlyUnderMouse");
else
config->writeEntry(KWIN_FOCUS,"FocusFollowsMouse");

//CT - 17Jun1998
config->writeEntry(KWIN_MINIMIZE_ANIM, getMinimizeAnim());
config->writeEntry(KWIN_MINIMIZE_ANIM_SPEED, getMinimizeAnimSpeed());

if ( getMinimizeAnim() > 0 )
    config->writeEntry("AnimateMinimize", true );


v = getResizeOpaque();
if (v == RESIZE_OPAQUE)
    config->writeEntry(KWIN_RESIZE_OPAQUE, "Opaque");
else
config->writeEntry(KWIN_RESIZE_OPAQUE, "Transparent");


v = getAutoRaiseInterval();
if (v <0) v = 0;
config->writeEntry(KWIN_AUTORAISE_INTERVAL,v);

if (autoRaiseOn->isChecked())
    config->writeEntry(KWIN_AUTORAISE, "on");
else
config->writeEntry(KWIN_AUTORAISE, "off");

if (clickRaiseOn->isChecked())
    config->writeEntry(KWIN_CLICKRAISE, "on");
else
config->writeEntry(KWIN_CLICKRAISE, "off");

if (kdeMode->isChecked())
    config->writeEntry(KWIN_ALTTABMODE, "KDE");
else
config->writeEntry(KWIN_ALTTABMODE, "CDE");

config->writeEntry(KWIN_CTRLTAB, ctrlTab->isChecked());

config->sync();

delete config;

if ( !kapp->dcopClient()->isAttached() )
    kapp->dcopClient()->attach();
kapp->dcopClient()->send("kwin", "", "reconfigure()", "");
}

void KWindowConfig::defaults()
{
    setMove(OPAQUE);
    setResizeOpaque(RESIZE_TRANSPARENT);
    setPlacement(SMART_PLACEMENT);
    setFocus(CLICK_TO_FOCUS);
    setAutoRaise(false);
    setClickRaise(false);
    setAltTabMode(true);
    setCtrlTab(true);
}

QString KWindowConfig::quickHelp() const
{
    return i18n("<h1>Window Behavior</h1> Here you can modify the way windows behave when being"
                " moved or resized and KWin's policies regarding window placement and window focus.<p>"
                " Please note that changes in this module will only take effect if you use KWin as your"
                " window manager. If you do use a different window manager, please consult its documentation"
                " on how to change these options.");
}

void KWindowConfig::loadSettings()
{
    load();
}

void KWindowConfig::applySettings()
{
    save();
}

#include "windows.moc"
