/* This file is part of the KDE Display Manager Configuration package
    Copyright (C) 1997-1998 Thomas Tanghus (tanghus@earthling.net)

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/


#include <qbuttongroup.h>
#include <qcombobox.h>
#include <qdragobject.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qradiobutton.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <kdialog.h>
#include <kglobal.h>
#include <kio/job.h>
#include <klocale.h>
#include <klineedit.h>
#include <kmessagebox.h>
#include <ksimpleconfig.h>
#include <kstddirs.h>

#include "kdm-appear.moc"


KDMAppearanceWidget::KDMAppearanceWidget(QWidget *parent, const char *name)
  : KCModule(parent, name)
{
  QString wtstr;
  QVBoxLayout *vbox = new QVBoxLayout(this, KDialog::marginHint(), 
				      KDialog::spacingHint(), "vbox");
  QGroupBox *group = new QGroupBox(i18n("Appearance"), this);
  vbox->addWidget(group);

  QGridLayout *grid = new QGridLayout( group, 5, 3, KDialog::spacingHint(), 
				       KDialog::spacingHint(), "grid");
  grid->addRowSpacing(0,group->fontMetrics().height());
  grid->setColStretch(2,1);

  QLabel *label = new QLabel(i18n("Greeting string:"), group);
  greetstr_lined = new KLineEdit(group);
  connect(greetstr_lined, SIGNAL(textChanged(const QString&)), 
	  this, SLOT(changed()));
  grid->addWidget(label, 1,0 );
  grid->addMultiCellWidget(greetstr_lined, 1,1, 1,2);
  wtstr = i18n("This is the string KDM will display in the login window. "
	       "You may want to put here some nice greeting or information "
	       "about the operating system.<p> KDM will replace the string "
	       "[HOSTNAME] with the actual host name of the computer "
	       "running the X server. Especially in networks this is a good "
	       "idea." );
  QWhatsThis::add( label, wtstr );
  QWhatsThis::add( greetstr_lined, wtstr );


  label = new QLabel(i18n("Logo area:"), group);
  QWidget *helper = new QWidget( group );
  QHBoxLayout *hlay = new QHBoxLayout( helper, KDialog::spacingHint() );
  logoRadio = new QRadioButton( i18n("Show KDM logo"), helper );
  clockRadio = new QRadioButton( i18n("Show clock"), helper );
  QButtonGroup *buttonGroup = new QButtonGroup( helper );
  connect( buttonGroup, SIGNAL(clicked(int)), 
	   this, SLOT(slotRadioClicked(int)) );
  connect( buttonGroup, SIGNAL(clicked(int)), 
	   this, SLOT(changed()) );
  buttonGroup->insert(logoRadio, KdmLogo);
  buttonGroup->insert(clockRadio, KdmClock);
  buttonGroup->hide();
  hlay->addWidget(logoRadio);
  hlay->addWidget(clockRadio, 1, AlignLeft);
  grid->addWidget(label,2,0);
  grid->addMultiCellWidget(helper, 2,2, 1,2);
  wtstr = i18n("You can choose to display the KDM logo or a clock");
  QWhatsThis::add( label, wtstr );
  QWhatsThis::add( logoRadio, wtstr );
  QWhatsThis::add( clockRadio, wtstr );

  logoLabel = new QLabel(i18n("KDM logo:"), group);
  logopath = "kdelogo.png";
  logobutton = new KIconButton(group);
  logobutton->setAutoDefault(false);
  logobutton->setAcceptDrops(true);
  logobutton->installEventFilter(this); // for drag and drop
  logobutton->setMinimumSize(24,24);
  logobutton->setMaximumSize(80,80);
  logobutton->setIcon("kdelogo");
  connect(logobutton, SIGNAL(iconChanged(QString)), 
	  this, SLOT(slotLogoPixChanged(QString)));
  connect(logobutton, SIGNAL(iconChanged(const QString&)), 
	  this, SLOT(changed()));
  grid->addRowSpacing(3, 80);
  grid->addWidget(logoLabel, 3,0);
  grid->addWidget(logobutton, 3,1, AlignVCenter|AlignLeft);
  wtstr = i18n("Click here to choose an image that KDM will display. "
	       "You can also drag and drop an image onto this button "
	       "(e.g. from Konqueror).");
  QWhatsThis::add( logoLabel, wtstr );
  QWhatsThis::add( logobutton, wtstr );



  label = new QLabel(i18n("GUI Style:"), group);
  guicombo = new QComboBox(false, group);
  guicombo->insertItem(i18n("Motif"), 0);
  guicombo->insertItem(i18n("Windows"), 1);
  //guicombo->insertItem(i18n("Platinum"), 2);
  //guicombo->insertItem(i18n("CDE"), 3);
  connect(guicombo, SIGNAL(activated(int)), this, SLOT(changed()));
  grid->addWidget(label, 4,0);
  grid->addWidget(guicombo, 4, 1);
  wtstr = i18n("You can choose a basic GUI style here that will be "
		"used by KDM only.");
  QWhatsThis::add( label, wtstr );
  QWhatsThis::add( guicombo, wtstr );


  // The Language group box
  group = new QGroupBox(i18n("Language"), this);
  vbox->addWidget(group);

  QGridLayout *hbox = new QGridLayout(group, 2, 2);
  hbox->addRowSpacing(0, 15);
  hbox->addRowSpacing(5, 10);
  hbox->addColSpacing(0, 10);
  hbox->addColSpacing(3, 10);
  hbox->setColStretch(2, 1);

  label = new QLabel(i18n("Language:"), group);
  hbox->addWidget(label, 1, 1);

  langcombo = new KLanguageCombo(group);
  hbox->addWidget(langcombo, 1, 2);
  connect(langcombo, SIGNAL(activated(int)), this, SLOT(changed()));

  wtstr = i18n("Here you can choose the language used by KDM. This setting doesn't affect"
    " a user's personal settings that will take effect after login.");
  QWhatsThis::add( label, wtstr );
  QWhatsThis::add( langcombo, wtstr );

  label = new QLabel(i18n("Country:"), group);
  hbox->addWidget(label, 2, 1);

  countrycombo = new KLanguageCombo(group);
  hbox->addWidget(countrycombo, 2, 2);
  connect(countrycombo, SIGNAL(activated(int)), this, SLOT(changed()));

  wtstr = i18n("Here you can choose a country setting for KDM. This setting doesn't affect"
    " a user's personal settings that will take effect after login.");
  QWhatsThis::add( label, wtstr );
  QWhatsThis::add( countrycombo, wtstr );

  loadLocaleList(langcombo, QString::null, QStringList());
  loadLocaleList(countrycombo, QString::fromLatin1("l10n/"), QStringList());
  load();
}

void KDMAppearanceWidget::loadLocaleList(KLanguageCombo *combo, const QString &sub, const QStringList &first)
{
  // clear the list
  combo->clear();

  QStringList prilang;
  // add the primary languages for the country to the list
  for ( QStringList::ConstIterator it = first.begin(); it != first.end(); ++it )
    {
        QString str = locate("locale", sub + *it + QString::fromLatin1("/entry.desktop"));
        if (!str.isNull())
          prilang << str;
    }

  // add all languages to the list
  QStringList alllang = KGlobal::dirs()->findAllResources("locale",
							   sub + QString::fromLatin1("*/entry.desktop"));
  alllang.sort();
  QStringList langlist = prilang;
  if (langlist.count() > 0)
    langlist << QString::null; // separator
  langlist += alllang;

  QString submenu, name; // we are working on this menu
  for ( QStringList::ConstIterator it = langlist.begin();
	it != langlist.end(); ++it )
    {
        if ((*it).isNull())
        {
	  combo->insertSeparator();
	  submenu = QString::fromLatin1("other");
	  combo->insertSubmenu(i18n("Other"), submenu);
          continue;
        }
	KSimpleConfig entry(*it);
	entry.setGroup(QString::fromLatin1("KCM Locale"));
	name = entry.readEntry(QString::fromLatin1("Name"), i18n("without name"));
	
	QString path = *it;
	int index = path.findRev('/');
	path = path.left(index);
	index = path.findRev('/');
	path = path.mid(index+1);
	combo->insertLanguage(path, name, sub, submenu);
    }
}

void KDMAppearanceWidget::setLogo(QString logo)
{
  logopath = logo;
  logobutton->setIcon(logo);
  logobutton->adjustSize();
  resize(width(), height());
}


void KDMAppearanceWidget::slotRadioClicked(int id)
{
  logobutton->setEnabled( id == KdmLogo );
  logoLabel->setEnabled( id == KdmLogo );
}


void KDMAppearanceWidget::slotLogoPixChanged(QString iconstr)
{
  logopath = iconstr;
}


bool KDMAppearanceWidget::eventFilter(QObject *, QEvent *e)
{
  if (e->type() == QEvent::DragEnter) {
    iconLoaderDragEnterEvent((QDragEnterEvent *) e);
    return true;
  }

  if (e->type() == QEvent::Drop) {
    iconLoaderDropEvent((QDropEvent *) e);
    return true;
  }

  return false;
}

void KDMAppearanceWidget::iconLoaderDragEnterEvent(QDragEnterEvent *e)
{
  e->accept(QUriDrag::canDecode(e));
}


void KDMAppearanceWidget::iconLoaderDropEvent(QDropEvent *e)
{
  QStringList uris;
  if (QUriDrag::decodeToUnicodeUris( e, uris) && (uris.count() > 0)) {
    KURL url(*uris.begin());

    QString fileName = url.fileName();
    QString msg;
    QStringList dirs = KGlobal::dirs()->findDirs("data", "kdm/pics/");
    QString local = KGlobal::dirs()->saveLocation("data", "kdm/pics/", false);
    QStringList::ConstIterator it = dirs.begin();
    if ((*it).left(local.length()) == local)
      it++;
    QString pixurl("file:"+ *it);
    int last_dot_idx = fileName.findRev('.');
    bool istmp = false;

    // CC: Now check for the extension
    QString ext(".png .xpm .xbm");
    //#ifdef HAVE_LIBGIF
    ext += " .gif";
    //#endif
#ifdef HAVE_LIBJPEG
    ext += " .jpg";
#endif

    if( !ext.contains(fileName.right(fileName.length()-
				     last_dot_idx), false) ) {
      msg =  i18n("Sorry, but %1\n"
		  "does not seem to be an image file\n"
		  "Please use files with these extensions:\n"
		  "%2")
	.arg(fileName)
	.arg(ext);
      KMessageBox::sorry( this, msg);
    } else {
      // we gotta check if it is a non-local file and make a tmp copy at the hd.
      if(url.protocol() != "file") {
	pixurl += url.fileName();
	KIO::file_copy(url, pixurl);
	url = pixurl;
	istmp = true;
      }
      // By now url should be "file:/..."
      QPixmap p(url.path());
      if(!p.isNull()) {
	logobutton->setPixmap(p);
	logobutton->adjustSize();
	logopath = url.path();
      } else {
        msg  = i18n("There was an error loading the image:\n>");
        msg += url.path();
        msg += i18n("<\nIt will not be saved...");
        KMessageBox::sorry(this, msg);
      }
    }
  }
}


void KDMAppearanceWidget::save()
{
  KSimpleConfig *c = new KSimpleConfig(locate("config", "kdmrc"));

  c->setGroup("KDM");

  // write greeting string
  c->writeEntry("GreetString", greetstr_lined->text(), true);

  // write logo or clock
  QString logoArea = logoRadio->isChecked() ? "KdmLogo" : "KdmClock";
  c->writeEntry("LogoArea", logoArea );
  printf("WRITE: logoArea=%s\n", logoArea.latin1() );

  // write logo path
  c->writeEntry("LogoPixmap", logopath, true);

  // write GUI style
  if (guicombo->currentItem() == 0)
    c->writeEntry("GUIStyle", "Motif", true);
  else
    c->writeEntry("GUIStyle", "Windows", true);

  // write language
  c->setGroup("Locale");
  c->writeEntry("Language", langcombo->currentTag());
  c->writeEntry("Country",  countrycombo->currentTag());
  c->writeEntry("Time",     countrycombo->currentTag());
  // Not used..
  c->writeEntry("Money",    countrycombo->currentTag());
  c->writeEntry("Numbers",  countrycombo->currentTag());

  delete c;
}


void KDMAppearanceWidget::load()
{
  // Get config object
  KConfig *c = new KConfig("kdmrc");
  c->setGroup("KDM");

  // Read the greeting string
  greetstr_lined->setText(c->readEntry("GreetString", "KDE System at [HOSTNAME]"));

  // Regular logo or clock
  QString logoArea = c->readEntry("LogoArea", "KdmLogo" );
  printf("READ: logoArea=%s\n", logoArea.latin1() );

  int mode = logoArea == "KdmLogo" ? KdmLogo : KdmClock;
  logoRadio->setChecked( mode == KdmLogo );
  clockRadio->setChecked( mode == KdmClock );
  slotRadioClicked(mode);

  // See if we use alternate logo
  QString logopath = c->readEntry("LogoPixmap");
  if(!logopath.isEmpty())
  {
    QFileInfo fi(logopath);
    if(fi.exists())
    {
      QString lpath = fi.dirPath(true);
      KGlobal::dirs()->addResourceDir("icon", lpath);
    }
  }
  setLogo(logopath);

  // Check the GUI type
  QString guistr = c->readEntry("GUIStyle", "Motif");
  if(guistr == "Windows")
    guicombo->setCurrentItem(1);
  else
    guicombo->setCurrentItem(0);

  // get the language
  c->setGroup("Locale");
  QString lang = c->readEntry("Language", "C");
  int index = lang.find(':');
  if (index>0)
    lang.truncate(index);
  langcombo->setCurrentItem(lang);

  // get the country
  lang = c->readEntry("Country", "C");
  countrycombo->setCurrentItem(lang);
}


void KDMAppearanceWidget::defaults()
{
  greetstr_lined->setText("KDE System at [HOSTNAME]");
  logoRadio->setChecked( true );
  clockRadio->setChecked( false );
  slotRadioClicked( KdmLogo );
  setLogo("kdelogo");
  guicombo->setCurrentItem(0);
  langcombo->setCurrentItem("C");
  countrycombo->setCurrentItem("C");
}

QString KDMAppearanceWidget::quickHelp()
{
  return i18n("<h1>KDM - Appearance</h1> Here you can configure the basic appearance"
    " of the KDM login manager, i.e. a greeting string, an icon etc.<p>"
    " For further refinement of KDM's appearance, see the \"Font\" and \"Background\" "
    " tabs.");
}


void KDMAppearanceWidget::changed()
{
  emit KCModule::changed(true);
}
