/****************************************************************************
  accessibility.cpp
  KDE Control Accessibility module to control Bell, Keyboard and ?Mouse?
  -------------------
  Copyright : (c) 2000 Matthias H�lzer-Kl�pfel
  -------------------
  Original Author: Matthias H�lzer-Kl�pfel
  Contributors: Jos� Pablo Ezequiel "Pupeno" Fern�ndez <pupeno@kde.org>
  Current Maintainer: Jos� Pablo Ezequiel "Pupeno" Fern�ndez <pupeno@kde.org>
 ****************************************************************************/

/****************************************************************************
 *                                                                          *
 *   This program is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation; either version 2 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 ****************************************************************************/

// $Id$

// #include <stdlib.h>

// #include <dcopref.h>

#include <qtabwidget.h>
// #include <qlayout.h>
// #include <qgroupbox.h>
// #include <qlabel.h>
#include <qcheckbox.h>
// #include <qlineedit.h>
// #include <qpushbutton.h>
#include <qradiobutton.h>
// #include <qwhatsthis.h>

#include <kconfig.h>
// #include <kglobal.h>
// #include <kstandarddirs.h>
// #include <klocale.h>
// #include <krun.h>
// #include <kurl.h>
// #include <kinstance.h>
#include <kcolorbutton.h>
// #include <kfiledialog.h>
#include <knuminput.h>
// #include <kapplication.h>
#include <kaboutdata.h>
// #include <kdebug.h>
#include <kgenericfactory.h>
#include <kurlrequester.h>


#include "accessibility.moc"

typedef KGenericFactory<AccessibilityConfig, QWidget> AccessibilityFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_accessibility, AccessibilityFactory("kcmaccessibility") );

/**
 * This function checks if the kaccess daemon needs to be run
 * This function will be depricated since the kaccess daemon will be part of kded
 */
// static bool needToRunKAccessDaemon( KConfig *config ){
//    KConfigGroup group( config, "Bell" );
//
//    if(!group.readBoolEntry("SystemBell", true)){
//       return true;
//    }
//    if(group.readBoolEntry("ArtsBell", false)){
//       return true;
//    }
//    if(group.readBoolEntry("VisibleBell", false)){
//       return true;
//    }
//    return false; // don't need it
// }

AccessibilityConfig::AccessibilityConfig(QWidget *parent, const char *name, const QStringList &)
  : AccessibilityConfigWidget( parent, name){
   kdDebug() << "Running: AccessibilityConfig::AccessibilityConfig(QWidget *parent, const char *name, const QStringList &)" << endl;
   // TODO: set the KURL Dialog to open just audio files
   connect( mainTab, SIGNAL(currentChanged(QWidget*)), this, SIGNAL(quickHelpChanged()) );
   load();
}


AccessibilityConfig::~AccessibilityConfig(){
   kdDebug() << "Running: AccessibilityConfig::~AccessibilityConfig()" << endl;
}

void AccessibilityConfig::load(){
   kdDebug() << "Running: AccessibilityConfig::load()" << endl;
   
   KConfig *bell = new KConfig("bellrc", true);
   
   bell->setGroup("General");
   systemBell->setChecked(bell->readBoolEntry("SystemBell", false));
   customBell->setChecked(bell->readBoolEntry("CustomBell", false));
   visibleBell->setChecked(bell->readBoolEntry("VisibleBell", false));
   
   bell->setGroup("CustomBell");
   soundToPlay->setURL(bell->readPathEntry("Sound", ""));

   bell->setGroup("Visible");
   invertScreen->setChecked(bell->readBoolEntry("Invert", true));
   flashScreen->setChecked(bell->readBoolEntry("Flash", false));
   // TODO: There has to be a cleaner way.
   QColor *redColor = new QColor(Qt::red);
   flashScreenColor->setColor(bell->readColorEntry("FlashColor", redColor));
   delete redColor;
   visibleBellDuration->setValue(bell->readNumEntry("Duration", 500));
  
   delete bell;   
//    KConfig *config = new KConfig("kaccessrc", true);
//
//    config->setGroup("Bell");
//
//    systemBell->setChecked(config->readBoolEntry("SystemBell", true));
//    customBell->setChecked(config->readBoolEntry("ArtsBell", false));
//    soundEdit->setText(config->readEntry("ArtsBellFile"));
//
//    visibleBell->setChecked(config->readBoolEntry("VisibleBell", false));
//    invertScreen->setChecked(config->readBoolEntry("VisibleBellInvert", true));
//    flashScreen->setChecked(!invertScreen->isChecked());
//    QColor def(Qt::red);
//    colorButton->setColor(config->readColorEntry("VisibleBellColor", &def));
//
//    durationSlider->setValue(config->readNumEntry("VisibleBellPause", 500));
//
//
//    config->setGroup("Keyboard");
//
//    stickyKeys->setChecked(config->readBoolEntry("StickyKeys", false));
//    stickyKeysLock->setChecked(config->readBoolEntry("StickyKeysLatch", true));
//    slowKeys->setChecked(config->readBoolEntry("SlowKeys", false));
//    slowKeysDelay->setValue(config->readNumEntry("SlowKeysDelay", 500));
//    bounceKeys->setChecked(config->readBoolEntry("BounceKeys", false));
//    bounceKeysDelay->setValue(config->readNumEntry("BounceKeysDelay", 500));
//
//
//    delete config;
//
//    checkAccess();
//
//    emit changed(false);
}


void AccessibilityConfig::save(){
   kdDebug() << "Running: AccessibilityConfig::save()" << endl;
   
   KConfig *bell = new KConfig("bellrc");
   
   bell->setGroup("General");
   bell->writeEntry("SystemBell", systemBell->isChecked());
   bell->writeEntry("CustomBell", customBell->isChecked());
   bell->writeEntry("VisibleBell", visibleBell->isChecked());
   
   bell->setGroup("CustomBell");
   bell->writePathEntry("Sound", soundToPlay->url());

   bell->setGroup("Visible");
   bell->writeEntry("Invert", invertScreen->isChecked());
   bell->writeEntry("Flash", flashScreen->isChecked());
   bell->writeEntry("FlashColor", flashScreenColor->color());
   bell->writeEntry("Duration", visibleBellDuration->value());
   
   bell->sync();
   delete bell;
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
//       kapp->startServiceByDesktopName("kaccess");
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
   kdDebug() << "Running: AccessibilityConfig::defaults()" << endl;

   // Bell
   // Audibe Bell
   systemBell->setChecked(false);
   customBell->setChecked(false);
   soundToPlay->clear();

   // Visible Bell
   visibleBell->setChecked(false);
   invertScreen->setChecked(true);
   flashScreen->setChecked(false);
   flashScreenColor->setColor(QColor(Qt::red));
   visibleBellDuration->setValue(500);

   // Keyboard
   // Sticky Keys
   stickyKeys->setChecked(false);
   lockWithStickyKeys->setChecked(true);

   // Slow Keys
   slowKeys->setChecked(false);
   slowKeysDelay->setValue(500);

   // Bounce Keys
   bounceKeys->setChecked(false);
   bounceKeysDelay->setValue(500);

   // Mouse
   // Navigation
   accelerationDelay->setValue(160);
   repeatInterval->setValue(5);
   accelerationTime->setValue(1000);
   maximumSpeed->setValue(500);
   accelerationProfile->setValue(0);
}

QString AccessibilityConfig::quickHelp() const{
   kdDebug() << "Running: AccessibilityConfig::quickHelp()"<< endl;
   kdDebug() << "mainTab->currentPageIndex=" << mainTab->currentPageIndex() << endl;
   switch(mainTab->currentPageIndex()){
      case 0:
         return i18n("<h1>Bell</h1>"); // ### fixme
         break;
      case 1:
         return i18n("<h1>Keyboard</h1>"); // ### fixme
         break;
      case 2:
         return i18n("<h1>Mouse</h1>"); // ### fixme
         break;
      default:
         return QString::null;   // This should never happen
   }
}

const KAboutData* AccessibilityConfig::aboutData() const{
   kdDebug() << "Running: AccessibilityConfig::aboutData() const"<< endl;
   KAboutData *about =
   new KAboutData(I18N_NOOP("kcmaccessiblity"), I18N_NOOP("KDE Accessibility Tool"),
                  0, 0, KAboutData::License_GPL,
                  I18N_NOOP("(c) 2000, Matthias Hoelzer-Kluepfel"));

   about->addAuthor("Matthias Hoelzer-Kluepfel", I18N_NOOP("Author") , "hoelzer@kde.org");
   about->addAuthor("Jos� Pablo Ezequiel Fern�ndez", I18N_NOOP("Author") , "pupeno@kde.org");

   return about;
}
