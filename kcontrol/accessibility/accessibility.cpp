/****************************************************************************
  accessibility.cpp
  KDE Control Accessibility module to control Bell, Keyboard and ?Mouse?
  -------------------
  Copyright : (c) 2000 Matthias Hölzer-Klüpfel
  -------------------
  Original Author: Matthias Hölzer-Klüpfel
  Contributors: José Pablo Ezequiel "Pupeno" Fernández <pupeno@kde.org>
  Current Maintainer: José Pablo Ezequiel "Pupeno" Fernández <pupeno@kde.org>
 ****************************************************************************/

/****************************************************************************
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 2 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 ****************************************************************************/

#include <QCheckBox>
#include <QRadioButton>
#include <QTabWidget>

#include <kaboutdata.h>
#include <kcolorbutton.h>
#include <kconfig.h>
#include <kgenericfactory.h>
#include <knuminput.h>
#include <kurlrequester.h>
#include <ktoolinvocation.h>

#include "accessibility.moc"

typedef KGenericFactory<AccessibilityConfig, QWidget> AccessibilityFactory;
K_EXPORT_COMPONENT_FACTORY( accessibility, AccessibilityFactory("kcmaccessibility") )

/**
 * This function checks if the kaccess daemon needs to be run
 * This function will be deprecated since the kaccess daemon will be part of kded
 */
// static bool needToRunKAccessDaemon( KConfig *config ){
//    KConfigGroup group( config, "Bell" );
//
//    if(!group.readEntry("SystemBell", true)){
//       return true;
//    }
//    if(group.readEntry("ArtsBell", false)){
//       return true;
//    }
//    if(group.readEntry("VisibleBell", false)){
//       return true;
//    }
//    return false; // don't need it
// }

AccessibilityConfig::AccessibilityConfig(QWidget *parent, const QStringList &args)
  : KCModule( AccessibilityFactory::componentData(), parent)
{
   Q_UNUSED( args )

   widget = new AccessibilityConfigWidget(parent, 0L);
   KAboutData *about =
   new KAboutData(I18N_NOOP("kcmaccessiblity"), 0, ki18n("KDE Accessibility Tool"),
                  0, KLocalizedString(), KAboutData::License_GPL,
                  ki18n("(c) 2000, Matthias Hoelzer-Kluepfel"));

   about->addAuthor(ki18n("Matthias Hoelzer-Kluepfel"), ki18n("Author") , "hoelzer@kde.org");
   about->addAuthor(ki18n("José Pablo Ezequiel Fernández"), ki18n("Author") , "pupeno@kde.org");
   setAboutData( about );

   kDebug() << "Running: AccessibilityConfig::AccessibilityConfig(QWidget *parent, const char *name, const QStringList &)" << endl;
   // TODO: set the KUrl Dialog to open just audio files
   connect( widget->mainTab, SIGNAL(currentChanged(QWidget*)), this, SIGNAL(quickHelpChanged()) );
   load();
}


AccessibilityConfig::~AccessibilityConfig(){
   kDebug() << "Running: AccessibilityConfig::~AccessibilityConfig()" << endl;
}

void AccessibilityConfig::load(){
   kDebug() << "Running: AccessibilityConfig::load()" << endl;

   KConfig bell("bellrc");
   KConfigGroup general( &bell, "General" );
   widget->systemBell->setChecked(general.readEntry("SystemBell", false));
   widget->customBell->setChecked(general.readEntry("CustomBell", false));
   widget->visibleBell->setChecked(general.readEntry("VisibleBell", false));

   KConfigGroup custom( &bell, "CustomBell" );
   widget->soundToPlay->setPath(custom.readPathEntry("Sound", ""));

   KConfigGroup visible( &bell, "Visible" );
   widget->invertScreen->setChecked(visible.readEntry("Invert", true));
   widget->flashScreen->setChecked(visible.readEntry("Flash", false));
   widget->flashScreenColor->setColor(visible.readEntry("FlashColor", QColor(Qt::red)));
   widget->visibleBellDuration->setValue(visible.readEntry("Duration", 500));

//    KConfig *config = new KConfig("kaccessrc", true);
//
//    config->setGroup("Bell");
//
//    systemBell->setChecked(config->readEntry("SystemBell", true));
//    customBell->setChecked(config->readEntry("ArtsBell", false));
//    soundEdit->setText(config->readPathEntry("ArtsBellFile"));
//
//    visibleBell->setChecked(config->readEntry("VisibleBell", false));
//    invertScreen->setChecked(config->readEntry("VisibleBellInvert", true));
//    flashScreen->setChecked(!invertScreen->isChecked());
//    QColor def(Qt::red);
//    colorButton->setColor(config->readEntry("VisibleBellColor", &def));
//
//    durationSlider->setValue(config->readEntry("VisibleBellPause", 500));
//
//
//    config->setGroup("Keyboard");
//
//    stickyKeys->setChecked(config->readEntry("StickyKeys", false));
//    stickyKeysLock->setChecked(config->readEntry("StickyKeysLatch", true));
//    slowKeys->setChecked(config->readEntry("SlowKeys", false));
//    slowKeysDelay->setValue(config->readEntry("SlowKeysDelay", 500));
//    bounceKeys->setChecked(config->readEntry("BounceKeys", false));
//    bounceKeysDelay->setValue(config->readEntry("BounceKeysDelay", 500));
//
//
//    delete config;
//
//    checkAccess();
//
//    emit changed(false);
}


void AccessibilityConfig::save(){
   kDebug() << "Running: AccessibilityConfig::save()" << endl;

   KConfig bell("bellrc");

   KConfigGroup general( &bell, "General" );
   general.writeEntry("SystemBell", widget->systemBell->isChecked());
   general.writeEntry("CustomBell", widget->customBell->isChecked());
   general.writeEntry("VisibleBell", widget->visibleBell->isChecked());

   KConfigGroup custom( &bell, "CustomBell" );
   custom.writePathEntry("Sound", widget->soundToPlay->url().url());

   KConfigGroup visible( &bell, "Visible" );
   visible.writeEntry("Invert", widget->invertScreen->isChecked());
   visible.writeEntry("Flash", widget->flashScreen->isChecked());
   visible.writeEntry("FlashColor", widget->flashScreenColor->color());
   visible.writeEntry("Duration", widget->visibleBellDuration->value());

   bell.sync();
//
//
//    config->setGroup("Keyboard");
//
//    config->writeEntry("StickyKeys", stickyKeys->isChecked());
//    config->writeEntry("StickyKeysLatch", stickyKeysLock->isChecked());
//
//    config->writeEntry("SlowKeys", slowKeys->isChecked());
//    config->writeEntry("SlowKeysDelay", slowKeysDelay->value());
//
//    config->writeEntry("BounceKeys", bounceKeys->isChecked());
//    config->writeEntry("BounceKeysDelay", bounceKeysDelay->value());
//
//
//    config->sync();
//
//    if(systemBell->isChecked() || customBell->isChecked() || visibleBell->isChecked()){
//       KConfig cfg("kdeglobals", false, false);
//       cfg.setGroup("General");
//       cfg.writeEntry("UseSystemBell", true);
//       cfg.sync();
//    }
//
//
//    if( needToRunKAccessDaemon( config ) ){
//       KToolInvocation::startServiceByDesktopName("kaccess");
//    }else{
//    // don't need it -> kill it
//       DCOPRef kaccess( "kaccess", "qt/kaccess" );
//       kaccess.send( "quit" );
//    }
//
//    delete config;
//
//    emit changed(false);
}


void AccessibilityConfig::defaults(){
   kDebug() << "Running: AccessibilityConfig::defaults()" << endl;

   // Bell
   // Audibe Bell
   widget->systemBell->setChecked(false);
   widget->customBell->setChecked(false);
   widget->soundToPlay->clear();

   // Visible Bell
   widget->visibleBell->setChecked(false);
   widget->invertScreen->setChecked(true);
   widget->flashScreen->setChecked(false);
   widget->flashScreenColor->setColor(QColor(Qt::red));
   widget->visibleBellDuration->setValue(500);

   // Keyboard
   // Sticky Keys
   widget->stickyKeys->setChecked(false);
   widget->lockWithStickyKeys->setChecked(true);

   // Slow Keys
   widget->slowKeys->setChecked(false);
   widget->slowKeysDelay->setValue(500);

   // Bounce Keys
   widget->bounceKeys->setChecked(false);
   widget->bounceKeysDelay->setValue(500);

   // Mouse
   // Navigation
   widget->accelerationDelay->setValue(160);
   widget->repeatInterval->setValue(5);
   widget->accelerationTime->setValue(1000);
   widget->maximumSpeed->setValue(500);
   widget->accelerationProfile->setValue(0);
}

