// KDE Display fonts setup tab
//
// Copyright (c)  Mark Donohoe 1997
//                lars Knoll 1999
//                Rik Hemsley 2000
//
// Ported to kcontrol2 by Geert Jansen.

#include <qlayout.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qwhatsthis.h>
#include <qtooltip.h>
#include <qcheckbox.h>
#include <qsettings.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <knuminput.h>
#include <qpushbutton.h>
#include <qdir.h>

#include <dcopclient.h>
#include <kapplication.h>
#include <ksimpleconfig.h>
#include <kipc.h>
#include <kstandarddirs.h>
#include <kmessagebox.h>
#include <kgenericfactory.h>
#include <stdlib.h>
#include "../krdb/krdb.h"

#include "fonts.h"
#include "fonts.moc"

#include <kdebug.h>

#include <X11/Xlib.h>

// X11 headers
#undef Bool
#undef Unsorted
#undef None

static const char *aa_rgb_xpm[]={
"12 12 3 1",
"a c #0000ff",
"# c #00ff00",
". c #ff0000",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa"};
static const char *aa_bgr_xpm[]={
"12 12 3 1",
". c #0000ff",
"# c #00ff00",
"a c #ff0000",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa",
"....####aaaa"};
static const char *aa_vrgb_xpm[]={
"12 12 3 1",
"a c #0000ff",
"# c #00ff00",
". c #ff0000",
"............",
"............",
"............",
"............",
"############",
"############",
"############",
"############",
"aaaaaaaaaaaa",
"aaaaaaaaaaaa",
"aaaaaaaaaaaa",
"aaaaaaaaaaaa"};
static const char *aa_vbgr_xpm[]={
"12 12 3 1",
". c #0000ff",
"# c #00ff00",
"a c #ff0000",
"............",
"............",
"............",
"............",
"############",
"############",
"############",
"############",
"aaaaaaaaaaaa",
"aaaaaaaaaaaa",
"aaaaaaaaaaaa",
"aaaaaaaaaaaa"};

static QPixmap aaPixmaps[]={ QPixmap(aa_rgb_xpm), QPixmap(aa_bgr_xpm),
                             QPixmap(aa_vrgb_xpm), QPixmap(aa_vbgr_xpm) };

/**** DLL Interface ****/
typedef KGenericFactory<KFonts, QWidget> FontFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_fonts, FontFactory("kcmfonts") )

/**** FontUseItem ****/

FontUseItem::FontUseItem(
  QWidget * parent,
  const QString &name,
  const QString &grp,
  const QString &key,
  const QString &rc,
  const QFont &default_fnt,
  bool f
)
  : KFontRequester(parent, 0L, f),
    _rcfile(rc),
    _rcgroup(grp),
    _rckey(key),
    _default(default_fnt)
{
  setTitle( name );
  readFont();
}

void FontUseItem::setDefault()
{
  setFont( _default, isFixedOnly() );
}

void FontUseItem::readFont()
{
  KConfigBase *config;

  bool deleteme = false;
  if (_rcfile.isEmpty())
    config = KGlobal::config();
  else
  {
    config = new KSimpleConfig(locate("config", _rcfile), true);
    deleteme = true;
  }

  config->setGroup(_rcgroup);
  QFont tmpFnt(_default);
  setFont( config->readFontEntry(_rckey, &tmpFnt), isFixedOnly() );
  if (deleteme) delete config;
}

void FontUseItem::writeFont()
{
  KConfigBase *config;

  if (_rcfile.isEmpty()) {
    config = KGlobal::config();
    config->setGroup(_rcgroup);
    config->writeEntry(_rckey, font(), true, true);
  } else {
    config = new KSimpleConfig(locateLocal("config", _rcfile));
    config->setGroup(_rcgroup);
    config->writeEntry(_rckey, font());
    config->sync();
    delete config;
  }
}

void FontUseItem::applyFontDiff( const QFont &fnt, int fontDiffFlags )
{
  QFont _font( font() );

  if (fontDiffFlags & KFontChooser::FontDiffSize) {
    _font.setPointSize( fnt.pointSize() );
  }
  if (fontDiffFlags & KFontChooser::FontDiffFamily) {
    if (!isFixedOnly()) _font.setFamily( fnt.family() );
  }
  if (fontDiffFlags & KFontChooser::FontDiffStyle) {
    _font.setBold( fnt.bold() );
    _font.setItalic( fnt.italic() );
    _font.setUnderline( fnt.underline() );
  }

  setFont( _font, isFixedOnly() );
}

/**** FontAASettings ****/

static const char * xftHintStrings[]={ "hintnone", "hintslight", "hintmedium", "hintfull" };
static const int    defaultXftHint=2;

static const char * getHintString(int index)
{
  return index>=0 && index<4 ? xftHintStrings[index] : NULL;
}

static int getHintIndex(const QString &str)
{
  for(int i=0; i<4; ++i)
    if(str==xftHintStrings[i])
      return i;
  return 0;
}

FontAASettings::FontAASettings(QWidget *parent)
              : KDialogBase(parent, "FontAASettings", true, i18n("Configure Anti-Alias Settings"), Ok|Cancel, Ok, true)
{
  QWidget     *mw=new QWidget(this);
  QGridLayout *layout=new QGridLayout(mw, 1, 1, KDialog::marginHint(), KDialog::spacingHint());

  excludeRange=new QCheckBox(i18n("E&xclude range:"), mw),
  layout->addWidget(excludeRange, 0, 0);
  excludeFrom=new KDoubleNumInput(0, 72, 8.0, 1, 1, mw),
  excludeFrom->setSuffix(i18n(" pt"));
  layout->addWidget(excludeFrom, 0, 1);
  excludeToLabel=new QLabel(i18n(" to "), mw);
  layout->addWidget(excludeToLabel, 0, 2);
  excludeTo=new KDoubleNumInput(0, 72, 15.0, 1, 1, mw);
  excludeTo->setSuffix(i18n(" pt"));
  layout->addWidget(excludeTo, 0, 3);

  useSubPixel=new QCheckBox(i18n("&Use sub-pixel hinting:"), mw);
  layout->addWidget(useSubPixel, 1, 0);

  QWhatsThis::add(useSubPixel, i18n("If you have a TFT or LCD screen you"
       " can further improve the quality of displayed fonts by selecting"
       " this option.<br>Sub-pixel hinting is also known as ClearType(tm).<br>"
       "<br><b>This will not work with CRT monitors.</b>"));

  subPixelType=new QComboBox(false, mw);
  layout->addMultiCellWidget(subPixelType, 1, 1, 1, 3);

  QWhatsThis::add(subPixelType, i18n("In order for sub-pixel hinting to"
       " work correctly you need to know how the sub-pixels of your display"
       " are aligned.<br>"
       " On TFT or LCD displays a single pixel is actually composed of"
       " three sub-pixels, red, green and blue. Most displays"
       " have a linear ordering of RGB sub-pixel, some have BGR."));

  for(int t=KXftConfig::SubPixel::None+1; t<=KXftConfig::SubPixel::Vbgr; ++t)
    subPixelType->insertItem(aaPixmaps[t-1], KXftConfig::description((KXftConfig::SubPixel::Type)t));

  QLabel *hintingLabel=new QLabel(i18n("Hinting style: "), mw);
  layout->addWidget(hintingLabel, 2, 0);
  hintingStyle=new QComboBox(false, mw);
  layout->addMultiCellWidget(hintingStyle, 2, 2, 1, 3);
  hintingStyle->insertItem(i18n("None"));
  hintingStyle->insertItem(i18n("Slight"));
  hintingStyle->insertItem(i18n("Medium"));
  hintingStyle->insertItem(i18n("Full"));

  QString hintingText(i18n("Hinting is a process used to enhance the quality of fonts at small sizes."));
  QWhatsThis::add(hintingStyle, hintingText);
  QWhatsThis::add(hintingLabel, hintingText);
  load();
  enableWidgets();
  setMainWidget(mw);

  connect(excludeRange, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(useSubPixel, SIGNAL(toggled(bool)), SLOT(changed()));
  connect(excludeFrom, SIGNAL(valueChanged(double)), SLOT(changed()));
  connect(excludeTo, SIGNAL(valueChanged(double)), SLOT(changed()));
  connect(subPixelType, SIGNAL(activated(const QString &)), SLOT(changed()));
  connect(hintingStyle, SIGNAL(activated(const QString &)), SLOT(changed()));
}

void FontAASettings::load()
{
  double     from, to;
  KXftConfig xft(KXftConfig::ExcludeRange|KXftConfig::SubPixelType);

  if(xft.getExcludeRange(from, to))
     excludeRange->setChecked(true);
  else
  {
    excludeRange->setChecked(false);
    from=8.0;
    to=15.0;
  }

  excludeFrom->setValue(from);
  excludeTo->setValue(to);

  KXftConfig::SubPixel::Type spType;

  if(!xft.getSubPixelType(spType) || KXftConfig::SubPixel::None==spType)
    useSubPixel->setChecked(false);
  else
  {
    int idx=getIndex(spType);

    if(idx>-1)
    {
      useSubPixel->setChecked(true);
      subPixelType->setCurrentItem(idx);
    }
    else
      useSubPixel->setChecked(false);
  }

  KConfig kglobals("kdeglobals", true, false);

  kglobals.setGroup("General");
  hintingStyle->setCurrentItem(getHintIndex(kglobals.readEntry("XftHintStyle", getHintString(defaultXftHint))));
  enableWidgets();
}

bool FontAASettings::save()
{
  KXftConfig xft(KXftConfig::ExcludeRange|KXftConfig::SubPixelType);
  KConfig    kglobals("kdeglobals", false, false);

  kglobals.setGroup("General");

  if(excludeRange->isChecked())
    xft.setExcludeRange(excludeFrom->value(), excludeTo->value());
  else
    xft.setExcludeRange(0, 0);

  if(useSubPixel->isChecked())
  {
    KXftConfig::SubPixel::Type spType(getSubPixelType());

    kglobals.writeEntry("XftSubPixel", KXftConfig::toStr(spType));
    xft.setSubPixelType(spType);
  }
  else
  {
    kglobals.writeEntry("XftSubPixel", "");
    xft.setSubPixelType(KXftConfig::SubPixel::None);
  }

  bool mod=xft.changed();
  xft.apply();

  QString hs(getHintString(hintingStyle->currentItem()));

  if(!hs.isEmpty() && hs!=kglobals.readEntry("XftHintStyle"))
  {
    kglobals.writeEntry("XftHintStyle", hs);
    kglobals.sync();
    mod=true;
  }

  return mod;
}

void FontAASettings::defaults()
{
  excludeRange->setChecked(true);
  excludeFrom->setValue(8.0);
  excludeTo->setValue(15.0);
  useSubPixel->setChecked(false);
  hintingStyle->setCurrentItem(defaultXftHint);
  enableWidgets();
}

int FontAASettings::getIndex(KXftConfig::SubPixel::Type spType)
{
  int pos=-1;
  int index;

  for(index=0; index<subPixelType->count(); ++index)
    if(subPixelType->text(index)==KXftConfig::description(spType))
    {
      pos=index;
      break;
    }

  return pos;
}

KXftConfig::SubPixel::Type FontAASettings::getSubPixelType()
{
  int t;

  for(t=KXftConfig::SubPixel::None; t<=KXftConfig::SubPixel::Vbgr; ++t)
    if(subPixelType->currentText()==KXftConfig::description((KXftConfig::SubPixel::Type)t))
      return (KXftConfig::SubPixel::Type)t;

  return KXftConfig::SubPixel::None;
}

void FontAASettings::enableWidgets()
{
  excludeFrom->setEnabled(excludeRange->isChecked());
  excludeTo->setEnabled(excludeRange->isChecked());
  excludeToLabel->setEnabled(excludeRange->isChecked());
  subPixelType->setEnabled(useSubPixel->isChecked());
}

void FontAASettings::changed()
{
    enableButtonOK(true);
}

int FontAASettings::exec()
{
    enableButtonOK(false);
    if(KDialogBase::Cancel==KDialogBase::exec())
    {
        load(); // Reset settings...
        return 0;
    }

    return 1;
}

/**** KFonts ****/

static QCString desktopConfigName()
{
  int desktop=0;
  if (qt_xdisplay())
    desktop = DefaultScreen(qt_xdisplay());
  QCString name;
  if (desktop == 0)
    name = "kdesktoprc";
  else
    name.sprintf("kdesktop-screen-%drc", desktop);

  return name;
}

KFonts::KFonts(QWidget *parent, const char *name, const QStringList &)
    :   KCModule(FontFactory::instance(), parent, name),
        _changed(false)
{
  QStringList nameGroupKeyRc;

  nameGroupKeyRc
    << i18n("General")        << "General"    << "font"         << ""
    << i18n("Fixed width")    << "General"    << "fixed"        << ""
    << i18n("Toolbar")        << "General"    << "toolBarFont"  << ""
    << i18n("Menu")           << "General"    << "menuFont"     << ""
    << i18n("Window title")   << "WM"         << "activeFont"   << ""
    << i18n("Taskbar")        << "General"    << "taskbarFont"  << ""
    << i18n("Desktop")        << "FMSettings" << "StandardFont" << desktopConfigName();

  QValueList<QFont> defaultFontList;

  // Keep in sync with kdelibs/kdecore/kglobalsettings.cpp

  QFont f0("helvetica", 12);
  QFont f1("courier", 12);
  QFont f2("helvetica", 10);
  QFont f3("helvetica", 12, QFont::Bold);
  QFont f4("helvetica", 11);

  f0.setPointSize(12);
  f1.setPointSize(12);
  f2.setPointSize(10);
  f3.setPointSize(12);
  f4.setPointSize(11);

  defaultFontList << f0 << f1 << f2 << f0 << f3 << f4 << f0;

  QValueList<bool> fixedList;

  fixedList
    <<  false
    <<  true
    <<  false
    <<  false
    <<  false
    <<  false
    <<  false;

  QStringList quickHelpList;

  quickHelpList
    << i18n("Used for normal text (e.g. button labels, list items).")
    << i18n("A non-proportional font (i.e. typewriter font).")
    << i18n("Used to display text beside toolbar icons.")
    << i18n("Used by menu bars and popup menus.")
    << i18n("Used by the window titlebar.")
    << i18n("Used by the taskbar.")
    << i18n("Used for desktop icons.");

  QVBoxLayout * layout =
    new QVBoxLayout(this, 0, KDialog::spacingHint());

  QGridLayout * fontUseLayout =
    new QGridLayout(layout, nameGroupKeyRc.count() / 4, 3);

  fontUseLayout->setColStretch(0, 0);
  fontUseLayout->setColStretch(1, 1);
  fontUseLayout->setColStretch(2, 0);

  QValueList<QFont>::ConstIterator defaultFontIt(defaultFontList.begin());
  QValueList<bool>::ConstIterator fixedListIt(fixedList.begin());
  QStringList::ConstIterator quickHelpIt(quickHelpList.begin());
  QStringList::ConstIterator it(nameGroupKeyRc.begin());

  unsigned int count = 0;

  while (it != nameGroupKeyRc.end()) {

    QString name = *it; it++;
    QString group = *it; it++;
    QString key = *it; it++;
    QString file = *it; it++;

    FontUseItem * i =
      new FontUseItem(
        this,
        name,
        group,
        key,
        file,
        *defaultFontIt++,
        *fixedListIt++
      );

    fontUseList.append(i);
    connect(i, SIGNAL(fontSelected(const QFont &)), SLOT(fontSelected()));

    QLabel * fontUse = new QLabel(name+":", this);
    QWhatsThis::add(fontUse, *quickHelpIt++);

    fontUseLayout->addWidget(fontUse, count, 0);
    fontUseLayout->addWidget(i, count, 1);

    ++count;
  }

   QHBoxLayout *lay = new QHBoxLayout(layout, KDialog::spacingHint());
   lay->addStretch();
   QPushButton * fontAdjustButton = new QPushButton(i18n("Ad&just All Fonts..."), this);
   QWhatsThis::add(fontAdjustButton, i18n("Click to change all fonts"));
   lay->addWidget( fontAdjustButton );
   connect(fontAdjustButton, SIGNAL(clicked()), SLOT(slotApplyFontDiff()));

   layout->addSpacing(KDialog::spacingHint());

   lay = new QHBoxLayout(layout, KDialog::spacingHint());
   cbAA = new QCheckBox( i18n( "Use a&nti-aliasing for fonts" ), this);
   QWhatsThis::add(cbAA, i18n("If this option is selected, KDE will smooth the edges of curves in "
                              "fonts."));
   lay->addStretch();
   QPushButton *aaSettingsButton = new QPushButton( i18n( "Configure..." ), this);
   connect(aaSettingsButton, SIGNAL(clicked()), SLOT(slotCfgAa()));
   connect(cbAA, SIGNAL(toggled(bool)), aaSettingsButton, SLOT(setEnabled(bool)));
   lay->addWidget( cbAA );
   lay->addWidget( aaSettingsButton );

   layout->addStretch(1);

   aaSettings=new FontAASettings(this);

   connect(cbAA, SIGNAL(clicked()), SLOT(slotUseAntiAliasing()));

   useAA = QSettings().readBoolEntry("/qt/useXft");
   useAA_original = useAA;

   cbAA->setChecked(useAA);
}

KFonts::~KFonts()
{
  fontUseList.setAutoDelete(true);
  fontUseList.clear();
}

void KFonts::fontSelected()
{
  _changed = true;
  emit changed(true);
}

void KFonts::defaults()
{
  for ( int i = 0; i < (int) fontUseList.count(); i++ )
    fontUseList.at( i )->setDefault();

  useAA = false;
  cbAA->setChecked(useAA);
  aaSettings->defaults();
  _changed = true;
  emit changed(true);
}

QString KFonts::quickHelp() const
{
    return i18n( "<h1>Fonts</h1> This module allows you to choose which"
      " fonts will be used to display text in KDE. You can select not only"
      " the font family (for example, <em>helvetica</em> or <em>times</em>),"
      " but also the attributes that make up a specific font (for example,"
      " <em>bold</em> style and <em>12 points</em> in height.)<p>"
      " Just click the \"Choose\" button that is next to the font you want to"
      " change."
      " You can ask KDE to try and apply font and color settings to non-KDE"
      " applications as well. See the \"Style\" control module for more"
			" information.");
}

void KFonts::load()
{
  for ( uint i = 0; i < fontUseList.count(); i++ )
    fontUseList.at( i )->readFont();

  useAA = QSettings().readBoolEntry("/qt/useXft");
  useAA_original = useAA;
  kdDebug(1208) << "AA:" << useAA << endl;
  cbAA->setChecked(useAA);

  aaSettings->load();

  _changed = true;
  emit changed(false);
}

void KFonts::save()
{
  if ( !_changed )
    return;

  _changed = false;

  for ( FontUseItem* i = fontUseList.first(); i; i = fontUseList.next() )
      i->writeFont();

  KGlobal::config()->sync();

  // KDE-1.x support
  KSimpleConfig* config = new KSimpleConfig( QDir::homeDirPath() + "/.kderc" );
  config->setGroup( "General" );
  for ( FontUseItem* i = fontUseList.first(); i; i = fontUseList.next() ) {
      if("font"==i->rcKey())
          QSettings().writeEntry("/qt/font", i->font().toString());
      kdDebug(1208) << "write entry " <<  i->rcKey() << endl;
      config->writeEntry( i->rcKey(), i->font() );
  }
  config->sync();
  delete config;

  QSettings().writeEntry("/qt/useXft", useAA);

  if (useAA)
    QSettings().writeEntry("/qt/enableXft", useAA);

  KIPC::sendMessageAll(KIPC::FontChanged);

  kapp->processEvents(); // Process font change ourselves

  if(aaSettings->save() || (useAA != useAA_original) ) {
    KMessageBox::information(this,
      i18n(
        "<p>You have changed anti-aliasing related settings. This change will only affect newly started applications.</p>"
      ), i18n("Anti-Aliasing Settings Changed"), "AAsettingsChanged", false);
    useAA_original = useAA;
  }

  runRdb(KRdbExportXftSettings);

  emit changed(false);
}

int KFonts::buttons()
{
  return KCModule::Help | KCModule::Default | KCModule::Apply;
}

void KFonts::slotApplyFontDiff()
{
  QFont font = QFont(fontUseList.first()->font());
  int fontDiffFlags = 0;
  int ret = KFontDialog::getFontDiff(font,fontDiffFlags);

  if (ret == KDialog::Accepted && fontDiffFlags)
  {
    for ( int i = 0; i < (int) fontUseList.count(); i++ )
      fontUseList.at( i )->applyFontDiff( font,fontDiffFlags );
    _changed = true;
    emit changed(true);
  }
}

void KFonts::slotUseAntiAliasing()
{
    useAA = cbAA->isChecked();
    _changed = true;
    emit changed(true);
}

void KFonts::slotCfgAa()
{
  if(aaSettings->exec())
  {
    _changed = true;
    emit changed(true);
  }
}

// vim:ts=2:sw=2:tw=78
