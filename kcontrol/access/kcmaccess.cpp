/**
 *  kcmaccess.cpp
 *
 *  Copyright (c) 2000 Matthias H�lzer-Kl�pfel
 *
 */


#include <stdlib.h>


#include <dcopref.h>

#include <qtabwidget.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qlineedit.h>
#include <qradiobutton.h>
#include <qwhatsthis.h>


#include <kstandarddirs.h>
#include <kcolorbutton.h>
#include <kfiledialog.h>
#include <knuminput.h>
#include <kapplication.h>
#include <kaboutdata.h>

#include "kcmaccess.moc"

static bool needToRunKAccessDaemon( KConfig *config )
{
    KConfigGroup bell( config, "Bell" );
    
    if (!bell.readBoolEntry("SystemBell", true))
        return true;
    if (bell.readBoolEntry("ArtsBell", false))
        return true;
    if (bell.readBoolEntry("VisibleBell", false))
        return true;
    
  KConfigGroup keyboard( config, "Keyboard" );

  if (keyboard.readBoolEntry("StickyKeys", false))
        return true;
  if (keyboard.readBoolEntry("SlowKeys", false))
        return true;
  if (keyboard.readBoolEntry("BounceKeys", false))
        return true;

    return false; // don't need it
}

KAccessConfig::KAccessConfig(QWidget *parent, const char *)
  : KCModule(parent, "kcmaccess")
{

  KAboutData *about =
  new KAboutData(I18N_NOOP("kaccess"), I18N_NOOP("KDE Accessibility Tool"),
                 0, 0, KAboutData::License_GPL,
                 I18N_NOOP("(c) 2000, Matthias Hoelzer-Kluepfel"));

  about->addAuthor("Matthias Hoelzer-Kluepfel", I18N_NOOP("Author") , "hoelzer@kde.org");

  setAboutData( about );

  QVBoxLayout *main = new QVBoxLayout(this, 0, KDialogBase::spacingHint());
  QTabWidget *tab = new QTabWidget(this);
  main->addWidget(tab);

  // bell settings ---------------------------------------
  QWidget *bell = new QWidget(this);

  QVBoxLayout *vbox = new QVBoxLayout(bell, KDialogBase::marginHint(), 
      KDialogBase::spacingHint());

  QGroupBox *grp = new QGroupBox(i18n("Audible Bell"), bell);
  grp->setColumnLayout( 0, Qt::Horizontal );
  vbox->addWidget(grp);

  QVBoxLayout *vvbox = new QVBoxLayout(grp->layout(), 
      KDialogBase::spacingHint());

  systemBell = new QCheckBox(i18n("Use &system bell"), grp);
  vvbox->addWidget(systemBell);
  customBell = new QCheckBox(i18n("Us&e customized bell"), grp);
  vvbox->addWidget(customBell);
  QWhatsThis::add( systemBell, i18n("If this option is checked, the default system bell will be used. See the"
    " \"System Bell\" control module for how to customize the system bell."
    " Normally, this is just a \"beep\".") );
  QWhatsThis::add( customBell, i18n("Check this option if you want to use a customized bell, playing"
    " a sound file. If you do this, you will probably want to turn off the system bell.<p> Please note"
    " that on slow machines this may cause a \"lag\" between the event causing the bell and the sound being played.") );

  QHBoxLayout *hbox = new QHBoxLayout(vvbox, KDialogBase::spacingHint());
  hbox->addSpacing(24);
  soundEdit = new QLineEdit(grp);
  soundLabel = new QLabel(soundEdit, i18n("Sound &to play:"), grp);
  hbox->addWidget(soundLabel);
  hbox->addWidget(soundEdit);
  soundButton = new QPushButton(i18n("Browse..."), grp);
  hbox->addWidget(soundButton);
  QString wtstr = i18n("If the option \"Use customized bell\" is enabled, you can choose a sound file here."
    " Click \"Browse...\" to choose a sound file using the file dialog.");
  QWhatsThis::add( soundEdit, wtstr );
  QWhatsThis::add( soundLabel, wtstr );
  QWhatsThis::add( soundButton, wtstr );

  connect(soundButton, SIGNAL(clicked()), this, SLOT(selectSound()));

  connect(customBell, SIGNAL(clicked()), this, SLOT(checkAccess()));

  connect(systemBell, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(customBell, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(soundEdit, SIGNAL(textChanged(const QString&)), this, SLOT(configChanged()));

  // -----------------------------------------------------

  // visible bell ----------------------------------------
  grp = new QGroupBox(i18n("Visible Bell"), bell);
  grp->setColumnLayout( 0, Qt::Horizontal );
  vbox->addWidget(grp);

  vvbox = new QVBoxLayout(grp->layout(), KDialog::spacingHint());

  visibleBell = new QCheckBox(i18n("&Use visible bell"), grp);
  vvbox->addWidget(visibleBell);
  QWhatsThis::add( visibleBell, i18n("This option will turn on the \"visible bell\", i.e. a visible"
    " notification shown every time that normally just a bell would occur. This is especially useful"
    " for deaf people.") );

  hbox = new QHBoxLayout(vvbox, KDialog::spacingHint());
  hbox->addSpacing(24);
  invertScreen = new QRadioButton(i18n("I&nvert screen"), grp);
  hbox->addWidget(invertScreen);
  hbox = new QHBoxLayout(vvbox, KDialog::spacingHint());
  QWhatsThis::add( invertScreen, i18n("All screen colors will be inverted for the amount of time specified below.") );
  hbox->addSpacing(24);
  flashScreen = new QRadioButton(i18n("F&lash screen"), grp);
  hbox->addWidget(flashScreen);
  QWhatsThis::add( flashScreen, i18n("The screen will turn to a custom color for the amount of time specified below.") );
  hbox->addSpacing(12);
  colorButton = new KColorButton(grp);
  colorButton->setFixedWidth(colorButton->sizeHint().height()*2);
  hbox->addWidget(colorButton);
  hbox->addStretch();
  QWhatsThis::add( colorButton, i18n("Click here to choose the color used for the \"flash screen\" visible bell.") );

  hbox = new QHBoxLayout(vvbox, KDialog::spacingHint());
  hbox->addSpacing(24);

  durationSlider = new KIntNumInput(grp);
  durationSlider->setRange(100, 2000, 100);
  durationSlider->setLabel(i18n("Duration:"));
  durationSlider->setSuffix(i18n(" msec"));
  hbox->addWidget(durationSlider);
  QWhatsThis::add( durationSlider, i18n("Here you can customize the duration of the \"visible bell\" effect being shown.") );

  connect(invertScreen, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(flashScreen, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(visibleBell, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(visibleBell, SIGNAL(clicked()), this, SLOT(checkAccess()));
  connect(colorButton, SIGNAL(clicked()), this, SLOT(changeFlashScreenColor()));

  connect(invertScreen, SIGNAL(clicked()), this, SLOT(invertClicked()));
  connect(flashScreen, SIGNAL(clicked()), this, SLOT(flashClicked()));

  connect(durationSlider, SIGNAL(valueChanged(int)), this, SLOT(configChanged()));

  vbox->addStretch();

  // -----------------------------------------------------

  tab->addTab(bell, i18n("&Bell"));


  // keys settings ---------------------------------------
  QWidget *keys = new QWidget(this);

  vbox = new QVBoxLayout(keys, KDialog::marginHint(), KDialog::spacingHint());

  grp = new QGroupBox(i18n("S&ticky Keys"), keys);
  grp->setColumnLayout( 0, Qt::Horizontal );
  vbox->addWidget(grp);

  vvbox = new QVBoxLayout(grp->layout(), KDialog::spacingHint());

  stickyKeys = new QCheckBox(i18n("Use &sticky keys"), grp);
  vvbox->addWidget(stickyKeys);

  hbox = new QHBoxLayout(vvbox, KDialog::spacingHint());
  hbox->addSpacing(24);
  stickyKeysLock = new QCheckBox(i18n("&Lock sticky keys"), grp);
  hbox->addWidget(stickyKeysLock);

  grp = new QGroupBox(i18n("Slo&w Keys"), keys);
  grp->setColumnLayout( 0, Qt::Horizontal );
  vbox->addWidget(grp);

  vvbox = new QVBoxLayout(grp->layout(), KDialog::spacingHint());

  slowKeys = new QCheckBox(i18n("&Use slow keys"), grp);
  vvbox->addWidget(slowKeys);

  hbox = new QHBoxLayout(vvbox, KDialog::spacingHint());
  hbox->addSpacing(24);
  slowKeysDelay = new KIntNumInput(grp);
  slowKeysDelay->setSuffix(i18n(" msec"));
  slowKeysDelay->setRange(100, 2000, 100);
  slowKeysDelay->setLabel(i18n("Dela&y:"));
  hbox->addWidget(slowKeysDelay);


  grp = new QGroupBox(i18n("Bounce Keys"), keys);
  grp->setColumnLayout( 0, Qt::Horizontal );
  vbox->addWidget(grp);

  vvbox = new QVBoxLayout(grp->layout(), KDialog::spacingHint());

  bounceKeys = new QCheckBox(i18n("Use bou&nce keys"), grp);
  vvbox->addWidget(bounceKeys);

  hbox = new QHBoxLayout(vvbox, KDialog::spacingHint());
  hbox->addSpacing(24);
  bounceKeysDelay = new KIntNumInput(grp);
  bounceKeysDelay->setSuffix(i18n(" msec"));
  bounceKeysDelay->setRange(100, 2000, 100);
  bounceKeysDelay->setLabel(i18n("D&elay:"));
  hbox->addWidget(bounceKeysDelay);


  connect(stickyKeys, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(stickyKeysLock, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(slowKeysDelay, SIGNAL(valueChanged(int)), this, SLOT(configChanged()));
  connect(slowKeys, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(slowKeys, SIGNAL(clicked()), this, SLOT(checkAccess()));
  connect(stickyKeys, SIGNAL(clicked()), this, SLOT(checkAccess()));

  connect(bounceKeysDelay, SIGNAL(valueChanged(int)), this, SLOT(configChanged()));
  connect(bounceKeys, SIGNAL(clicked()), this, SLOT(configChanged()));
  connect(bounceKeys, SIGNAL(clicked()), this, SLOT(checkAccess()));

  vbox->addStretch();

  tab->addTab(keys, i18n("&Keyboard"));

  load();
}


KAccessConfig::~KAccessConfig()
{
}

void KAccessConfig::changeFlashScreenColor()
{
    invertScreen->setChecked(false);
    flashScreen->setChecked(true);
    configChanged();
}

void KAccessConfig::selectSound()
{
  QStringList list = KGlobal::dirs()->findDirs("sound", "");
  QString start;
  if (list.count()>0)
    start = list[0];
  // TODO: Why only wav's? How can I find out what artsd supports?
  QString fname = KFileDialog::getOpenFileName(start, i18n("*.wav|WAV Files"));
  if (!fname.isEmpty())
    soundEdit->setText(fname);
}


void KAccessConfig::configChanged()
{
  emit changed(true);
}


void KAccessConfig::load()
{
  KConfig *config = new KConfig("kaccessrc", true);

  config->setGroup("Bell");

  systemBell->setChecked(config->readBoolEntry("SystemBell", true));
  customBell->setChecked(config->readBoolEntry("ArtsBell", false));
  soundEdit->setText(config->readPathEntry("ArtsBellFile"));

  visibleBell->setChecked(config->readBoolEntry("VisibleBell", false));
  invertScreen->setChecked(config->readBoolEntry("VisibleBellInvert", true));
  flashScreen->setChecked(!invertScreen->isChecked());
  QColor def(Qt::red);
  colorButton->setColor(config->readColorEntry("VisibleBellColor", &def));

  durationSlider->setValue(config->readNumEntry("VisibleBellPause", 500));


  config->setGroup("Keyboard");

  stickyKeys->setChecked(config->readBoolEntry("StickyKeys", false));
  stickyKeysLock->setChecked(config->readBoolEntry("StickyKeysLatch", true));
  slowKeys->setChecked(config->readBoolEntry("SlowKeys", false));
  slowKeysDelay->setValue(config->readNumEntry("SlowKeysDelay", 500));
  bounceKeys->setChecked(config->readBoolEntry("BounceKeys", false));
  bounceKeysDelay->setValue(config->readNumEntry("BounceKeysDelay", 500));


  delete config;

  checkAccess();

  emit changed(false);
}


void KAccessConfig::save()
{
  KConfig *config= new KConfig("kaccessrc", false);

  config->setGroup("Bell");

  config->writeEntry("SystemBell", systemBell->isChecked());
  config->writeEntry("ArtsBell", customBell->isChecked());
  config->writePathEntry("ArtsBellFile", soundEdit->text());

  config->writeEntry("VisibleBell", visibleBell->isChecked());
  config->writeEntry("VisibleBellInvert", invertScreen->isChecked());
  config->writeEntry("VisibleBellColor", colorButton->color());

  config->writeEntry("VisibleBellPause", durationSlider->value());


  config->setGroup("Keyboard");

  config->writeEntry("StickyKeys", stickyKeys->isChecked());
  config->writeEntry("StickyKeysLatch", stickyKeysLock->isChecked());

  config->writeEntry("SlowKeys", slowKeys->isChecked());
  config->writeEntry("SlowKeysDelay", slowKeysDelay->value());

  config->writeEntry("BounceKeys", bounceKeys->isChecked());
  config->writeEntry("BounceKeysDelay", bounceKeysDelay->value());


  config->sync();

  if (systemBell->isChecked() ||
      customBell->isChecked() ||
      visibleBell->isChecked())
  {
    KConfig cfg("kdeglobals", false, false);
    cfg.setGroup("General");
    cfg.writeEntry("UseSystemBell", true);
    cfg.sync();
  }

  // make kaccess reread the configuration
  if ( needToRunKAccessDaemon( config ) )
      kapp->startServiceByDesktopName("kaccess");

  else // don't need it -> kill it
  {
      DCOPRef kaccess( "kaccess", "qt/kaccess" );
      kaccess.send( "quit" );
  }

  delete config;
  
  emit changed(false);
}


void KAccessConfig::defaults()
{
  systemBell->setChecked(true);
  customBell->setChecked(false);
  soundEdit->setText("");

  visibleBell->setChecked(false);
  invertScreen->setChecked(true);
  flashScreen->setChecked(false);
  colorButton->setColor(QColor(Qt::red));

  durationSlider->setValue(500);

  slowKeys->setChecked(false);
  slowKeysDelay->setValue(500);

  bounceKeys->setChecked(false);
  bounceKeysDelay->setValue(500);

  stickyKeys->setChecked(true);
  stickyKeysLock->setChecked(true);

  checkAccess();

  emit changed(true);
}


void KAccessConfig::invertClicked()
{
  flashScreen->setChecked(false);
}


void KAccessConfig::flashClicked()
{
  invertScreen->setChecked(false);
}


void KAccessConfig::checkAccess()
{
  bool custom = customBell->isChecked();
  soundEdit->setEnabled(custom);
  soundButton->setEnabled(custom);
  soundLabel->setEnabled(custom);

  bool visible = visibleBell->isChecked();
  invertScreen->setEnabled(visible);
  flashScreen->setEnabled(visible);
  colorButton->setEnabled(visible);

  durationSlider->setEnabled(visible);

  stickyKeysLock->setEnabled(stickyKeys->isChecked());

  slowKeysDelay->setEnabled(slowKeys->isChecked());

  bounceKeysDelay->setEnabled(bounceKeys->isChecked());
}

extern "C"
{
  KCModule *create_access(QWidget *parent, const char *name)
  {
    return new KAccessConfig(parent, name);
  }

  /* This one gets called by kcminit

   */
  void init_access()
  {
    KConfig *config = new KConfig("kaccessrc", true, false);
    bool run = needToRunKAccessDaemon( config );

    delete config;
    if (run)
      kapp->startServiceByDesktopName("kaccess");
  }
}


