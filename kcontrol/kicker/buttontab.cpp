/*
 *  buttontab.cpp
 *
 *  Copyright (c) 2000 Matthias Elter <elter@kde.org>
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
 */

#include <qlayout.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qpixmap.h>
#include <qfileinfo.h>

#include <kconfig.h>
#include <kdialog.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <klocale.h>
#include <kcombobox.h>
#include <kimageio.h>

#include "buttontab.h"
#include "buttontab.moc"


ButtonTab::ButtonTab( QWidget *parent, const char* name )
  : QWidget (parent, name)
{
  layout = new QGridLayout(this,4 , 2,
                           KDialog::marginHint(),
                           KDialog::spacingHint());

  // general group
  general_group = new QGroupBox(i18n("General"), this);

  QVBoxLayout *vbox = new QVBoxLayout(general_group, KDialog::marginHint(),
                         KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  tiles_cb = new QCheckBox(i18n("Enable background tiles"), general_group);
  connect(tiles_cb, SIGNAL(clicked()), SLOT(tiles_clicked()));
  vbox->addWidget(tiles_cb);
  QWhatsThis::add( tiles_cb, i18n("If this option is enabled, the panel will display"
    " panel buttons using tile images instead of just drawing flat buttons. You can still"
    " enable or disable usage of tiles for the different kind of panel buttons, using the"
    " options below.") );

  layout->addMultiCellWidget(general_group, 0, 0, 0, 1);

  // k-menu button tiles group
  kmenu_group = new QGroupBox(i18n("K-Menu Tiles"), this);

  vbox = new QVBoxLayout(kmenu_group,KDialog::marginHint(),
                                      KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  QHBox *hbox = new QHBox(kmenu_group);
  hbox->setSpacing(KDialog::spacingHint());

  QVBox *vbox1 = new QVBox(hbox);
  kmenu_cb = new QCheckBox(i18n("Enabled"), vbox1);
  connect(kmenu_cb, SIGNAL(clicked()), SLOT(kmenu_clicked()));

  kmenu_input = new KComboBox(vbox1);
  kmenu_input->setInsertionPolicy(QComboBox::NoInsertion);
  connect(kmenu_input, SIGNAL(activated(const QString&)), SLOT(kmenu_changed(const QString&)));
  QWhatsThis::add( kmenu_cb, i18n("Enable or disable the usage of a tile image for the K menu.") );
  QWhatsThis::add( kmenu_input, i18n("Choose a tile image for the K menu."));

  kmenu_label = new QLabel(hbox);
  kmenu_label->setFixedSize(56,56);
  kmenu_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  kmenu_label->setAlignment(AlignCenter);
  QWhatsThis::add( kmenu_label, i18n("This is a preview of the tile that will be used for the K menu.") );

  vbox->addWidget(hbox);
  layout->addWidget(kmenu_group, 1, 0);

  // browser button tiles group
  browser_group = new QGroupBox(i18n("Quickbrowser Tiles"), this);

  vbox = new QVBoxLayout(browser_group,KDialog::marginHint(),
                                      KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  hbox = new QHBox(browser_group);
  hbox->setSpacing(KDialog::spacingHint());

  vbox1 = new QVBox(hbox);
  browser_cb = new QCheckBox(i18n("Enabled"), vbox1);
  connect(browser_cb, SIGNAL(clicked()), SLOT(browser_clicked()));

  browser_input = new KComboBox(vbox1);
  browser_input->setInsertionPolicy(QComboBox::NoInsertion);
  connect(browser_input, SIGNAL(activated(const QString&)), SLOT(browser_changed(const QString&)));
  QWhatsThis::add( browser_cb, i18n("Enable or disable the usage of tile images for Quick Browser buttons.") );
  QWhatsThis::add( browser_input, i18n("Choose a tile image for Quick Browser buttons."));

  browser_label = new QLabel(hbox);
  browser_label->setFixedSize(56,56);
  browser_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  browser_label->setAlignment(AlignCenter);
  QWhatsThis::add( browser_label, i18n("This is a preview of the tile that will be used for Quick Browser buttons.") );

  vbox->addWidget(hbox);
  layout->addWidget(browser_group, 1, 1);

  // url button tiles group
  url_group = new QGroupBox(i18n("Application Launcher Tiles"), this);

  vbox = new QVBoxLayout(url_group,KDialog::marginHint(),
                                      KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  hbox = new QHBox(url_group);
  hbox->setSpacing(KDialog::spacingHint());

  vbox1 = new QVBox(hbox);
  url_cb = new QCheckBox(i18n("Enabled"), vbox1);
  connect(url_cb, SIGNAL(clicked()), SLOT(url_clicked()));

  url_input = new KComboBox(vbox1);
  url_input->setInsertionPolicy(QComboBox::NoInsertion);
  connect(url_input, SIGNAL(activated(const QString&)), SLOT(url_changed(const QString&)));
  QWhatsThis::add( url_cb, i18n("Enable or disable the usage of a tile image for buttons that launch applications.") );
  QWhatsThis::add( url_input, i18n("Choose a tile image for buttons that launch applications."));

  url_label = new QLabel(hbox);
  url_label->setFixedSize(56,56);
  url_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  url_label->setAlignment(AlignCenter);
  QWhatsThis::add( url_label, i18n("This is a preview of the tile that will be used for buttons that launch applications.") );

  vbox->addWidget(hbox);
  layout->addWidget(url_group, 2, 0);

  // exe button tiles group
  exe_group = new QGroupBox(i18n("Legacy Application Launcher Tiles"), this);

  vbox = new QVBoxLayout(exe_group,KDialog::marginHint(),
                                      KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  hbox = new QHBox(exe_group);
  hbox->setSpacing(KDialog::spacingHint());

  vbox1 = new QVBox(hbox);
  exe_cb = new QCheckBox(i18n("Enabled"), vbox1);
  connect(exe_cb, SIGNAL(clicked()), SLOT(exe_clicked()));

  exe_input = new KComboBox(vbox1);
  exe_input->setInsertionPolicy(QComboBox::NoInsertion);
  connect(exe_input, SIGNAL(activated(const QString&)), SLOT(exe_changed(const QString&)));
  QWhatsThis::add( exe_cb, i18n("Enable or disable the usage of tile images for legacy application buttons.") );
  QWhatsThis::add( exe_input, i18n("Choose a tile image for legacy application buttons."));

  exe_label = new QLabel(hbox);
  exe_label->setFixedSize(56,56);
  exe_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  exe_label->setAlignment(AlignCenter);
  QWhatsThis::add( exe_label, i18n("This is a preview of the tile that will be used for legacy application buttons.") );

  vbox->addWidget(hbox);
  layout->addWidget(exe_group, 2, 1);

  // window list button tiles group
  wl_group = new QGroupBox(i18n("Window List Tiles"), this);

  vbox = new QVBoxLayout(wl_group,KDialog::marginHint(),
                                      KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  hbox = new QHBox(wl_group);
  hbox->setSpacing(KDialog::spacingHint());

  vbox1 = new QVBox(hbox);
  wl_cb = new QCheckBox(i18n("Enabled"), vbox1);
  connect(wl_cb, SIGNAL(clicked()), SLOT(wl_clicked()));

  wl_input = new KComboBox(vbox1);
  wl_input->setInsertionPolicy(QComboBox::NoInsertion);
  connect(wl_input, SIGNAL(activated(const QString&)), SLOT(wl_changed(const QString&)));
  QWhatsThis::add( wl_cb, i18n("Enable or disable the usage of tile images for window list buttons.") );
  QWhatsThis::add( wl_input, i18n("Choose a tile image for window list buttons."));

  wl_label = new QLabel(hbox);
  wl_label->setFixedSize(56,56);
  wl_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  wl_label->setAlignment(AlignCenter);
  QWhatsThis::add( wl_label, i18n("This is a preview of the tile that will be used for window list buttons.") );

  vbox->addWidget(hbox);
  layout->addWidget(wl_group, 3, 0);

  // drawer button tiles group
  drawer_group = new QGroupBox(i18n("Drawer Tiles"), this);

  vbox = new QVBoxLayout(drawer_group,KDialog::marginHint(),
                                      KDialog::spacingHint());
  vbox->addSpacing(fontMetrics().lineSpacing());

  hbox = new QHBox(drawer_group);
  hbox->setSpacing(KDialog::spacingHint());

  vbox1 = new QVBox(hbox);
  drawer_cb = new QCheckBox(i18n("Enabled"), vbox1);
  connect(drawer_cb, SIGNAL(clicked()), SLOT(drawer_clicked()));

  drawer_input = new KComboBox(vbox1);
  drawer_input->setInsertionPolicy(QComboBox::NoInsertion);
  connect(drawer_input, SIGNAL(activated(const QString&)), SLOT(drawer_changed(const QString&)));
  QWhatsThis::add( drawer_cb, i18n("Enable or disable the usage of tile images for drawer buttons.") );
  QWhatsThis::add( drawer_input, i18n("Choose a tile image for drawer buttons."));

  drawer_label = new QLabel(hbox);
  drawer_label->setFixedSize(56,56);
  drawer_label->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  drawer_label->setAlignment(AlignCenter);
  QWhatsThis::add( drawer_label, i18n("This is a preview of the tile that will be used for drawer buttons.") );

  vbox->addWidget(hbox);
  layout->addWidget(drawer_group, 3, 1);

  layout->setRowStretch(0, 1);
  layout->setRowStretch(1, 3);
  layout->setRowStretch(2, 3);
  layout->setRowStretch(3, 3);

  fill_tile_input();
  load();
}

void ButtonTab::tiles_clicked()
{
  bool enabled = tiles_cb->isChecked();

  kmenu_group->setEnabled(enabled);
  url_group->setEnabled(enabled);
  exe_group->setEnabled(enabled);
  browser_group->setEnabled(enabled);
  wl_group->setEnabled(enabled);
  drawer_group->setEnabled(enabled);
  emit changed();
}

void ButtonTab::kmenu_clicked()
{
  bool enabled = kmenu_cb->isChecked();
  kmenu_input->setEnabled(enabled);
  kmenu_label->setEnabled(enabled);
  emit changed();
}

void ButtonTab::kmenu_changed(const QString& t)
{
  QString tile = t + "_large_up.png";
  tile = KGlobal::dirs()->findResource("tiles", tile);

  if(!tile.isNull())
    {
      QPixmap pix(tile);
      if (!pix.isNull())
        kmenu_label->setPixmap(pix);
      else
        kmenu_label->clear();
    }
  else
    kmenu_label->clear();
  emit changed();
}

void ButtonTab::url_clicked()
{
  bool enabled = url_cb->isChecked();
  url_input->setEnabled(enabled);
  url_label->setEnabled(enabled);
  emit changed();
}
void ButtonTab::url_changed(const QString& t)
{
  QString tile = t + "_large_up.png";
  tile = KGlobal::dirs()->findResource("tiles", tile);

  if(!tile.isNull())
    {
      QPixmap pix(tile);
      if (!pix.isNull())
        url_label->setPixmap(pix);
      else
        url_label->clear();
    }
  else
    url_label->clear();
  emit changed();
}

void ButtonTab::browser_clicked()
{
  bool enabled = browser_cb->isChecked();
  browser_input->setEnabled(enabled);
  browser_label->setEnabled(enabled);
  emit changed();
}
void ButtonTab::browser_changed(const QString& t)
{
  QString tile = t + "_large_up.png";
  tile = KGlobal::dirs()->findResource("tiles", tile);

  if(!tile.isNull())
    {
      QPixmap pix(tile);
      if (!pix.isNull())
        browser_label->setPixmap(pix);
      else
        browser_label->clear();
    }
  else
    browser_label->clear();
  emit changed();
}

void ButtonTab::exe_clicked()
{
  bool enabled = exe_cb->isChecked();
  exe_input->setEnabled(enabled);
  exe_label->setEnabled(enabled);
  emit changed();
}
void ButtonTab::exe_changed(const QString& t)
{
  QString tile = t + "_large_up.png";
  tile = KGlobal::dirs()->findResource("tiles", tile);

  if(!tile.isNull())
    {
      QPixmap pix(tile);
      if (!pix.isNull())
        exe_label->setPixmap(pix);
      else
        exe_label->clear();
    }
  else
    exe_label->clear();
  emit changed();
}

void ButtonTab::wl_clicked()
{
  bool enabled = wl_cb->isChecked();
  wl_input->setEnabled(enabled);
  wl_label->setEnabled(enabled);
  emit changed();
}
void ButtonTab::wl_changed(const QString& t)
{
  QString tile = t + "_large_up.png";
  tile = KGlobal::dirs()->findResource("tiles", tile);

  if(!tile.isNull())
    {
      QPixmap pix(tile);
      if (!pix.isNull())
        wl_label->setPixmap(pix);
      else
        wl_label->clear();
    }
  else
    wl_label->clear();
  emit changed();
}

void ButtonTab::drawer_clicked()
{
  bool enabled = drawer_cb->isChecked();
  drawer_input->setEnabled(enabled);
  drawer_label->setEnabled(enabled);
  emit changed();
}
void ButtonTab::drawer_changed(const QString& t)
{
  QString tile = t + "_large_up.png";
  tile = KGlobal::dirs()->findResource("tiles", tile);

  if(!tile.isNull())
    {
      QPixmap pix(tile);
      if (!pix.isNull())
        drawer_label->setPixmap(pix);
      else
        drawer_label->clear();
    }
  else
    drawer_label->clear();
  emit changed();
}

void ButtonTab::load()
{
  KConfig *c = new KConfig("kickerrc", false, false);

  c->setGroup("buttons");

  bool tiles = c->readBoolEntry("EnableTileBackground", false);
  tiles_cb->setChecked(tiles);

  kmenu_group->setEnabled(tiles);
  url_group->setEnabled(tiles);
  exe_group->setEnabled(tiles);
  browser_group->setEnabled(tiles);
  wl_group->setEnabled(tiles);
  drawer_group->setEnabled(tiles);

  c->setGroup("button_tiles");

  bool kmenu_tiles = c->readBoolEntry("EnableKMenuTiles", true);
  kmenu_cb->setChecked(kmenu_tiles);
  kmenu_input->setEnabled(kmenu_tiles);
  kmenu_label->setEnabled(kmenu_tiles);

  bool url_tiles = c->readBoolEntry("EnableURLTiles", true);
  url_cb->setChecked(url_tiles);
  url_input->setEnabled(url_tiles);
  url_label->setEnabled(url_tiles);

  bool browser_tiles = c->readBoolEntry("EnableBrowserTiles", true);
  browser_cb->setChecked(browser_tiles);
  browser_input->setEnabled(browser_tiles);
  browser_label->setEnabled(browser_tiles);

  bool exe_tiles = c->readBoolEntry("EnableExeTiles", true);
  exe_cb->setChecked(exe_tiles);
  exe_input->setEnabled(exe_tiles);
  exe_label->setEnabled(exe_tiles);

  bool wl_tiles = c->readBoolEntry("EnableWindowListTiles", true);
  wl_cb->setChecked(wl_tiles);
  wl_input->setEnabled(wl_tiles);
  wl_label->setEnabled(wl_tiles);

  bool drawer_tiles = c->readBoolEntry("EnableDrawerTiles", true);
  drawer_cb->setChecked(drawer_tiles);
  drawer_input->setEnabled(drawer_tiles);
  drawer_label->setEnabled(drawer_tiles);

  // set kmenu tile
  QString tile = c->readEntry("KMenuTile", "solid_blue");
  int index = 0;

  for (int i = 0; i < kmenu_input->count(); i++) {
    if (tile == kmenu_input->text(i)) {
      index = i;
      break;
    }
  }
  kmenu_input->setCurrentItem(index);
  kmenu_changed(kmenu_input->text(index));

  // set url tile
  tile = c->readEntry("URLTile", "solid_gray");
  index = 0;

  for (int i = 0; i < url_input->count(); i++) {
    if (tile == url_input->text(i)) {
      index = i;
      break;
    }
  }
  url_input->setCurrentItem(index);
  url_changed(url_input->text(index));

  // set browser tile
  tile = c->readEntry("BrowserTile", "solid_green");
  index = 0;

  for (int i = 0; i < browser_input->count(); i++) {
    if (tile == browser_input->text(i)) {
      index = i;
      break;
    }
  }
  browser_input->setCurrentItem(index);
  browser_changed(browser_input->text(index));

  // set exe tile
  tile = c->readEntry("ExeTile", "solid_red");
  index = 0;

  for (int i = 0; i < exe_input->count(); i++) {
    if (tile == exe_input->text(i)) {
      index = i;
      break;
    }
  }
  exe_input->setCurrentItem(index);
  exe_changed(exe_input->text(index));

  // set window list tile
  tile = c->readEntry("WindowListTile", "solid_green");
  index = 0;

  for (int i = 0; i < wl_input->count(); i++) {
    if (tile == wl_input->text(i)) {
      index = i;
      break;
    }
  }
  wl_input->setCurrentItem(index);
  wl_changed(wl_input->text(index));

  // set drawer tile
  tile = c->readEntry("DrawerTile", "solid_orange");
  index = 0;

  for (int i = 0; i < drawer_input->count(); i++) {
    if (tile == drawer_input->text(i)) {
      index = i;
      break;
    }
  }
  drawer_input->setCurrentItem(index);
  drawer_changed(drawer_input->text(index));

  delete c;
}

void ButtonTab::save()
{
  KConfig *c = new KConfig("kickerrc", false, false);

  c->setGroup("buttons");

  c->writeEntry("EnableTileBackground", tiles_cb->isChecked());

  c->setGroup("button_tiles");
  c->writeEntry("EnableKMenuTiles", kmenu_cb->isChecked());
  c->writeEntry("EnableURLTiles", url_cb->isChecked());
  c->writeEntry("EnableBrowserTiles", browser_cb->isChecked());
  c->writeEntry("EnableExeTiles", exe_cb->isChecked());
  c->writeEntry("EnableWindowListTiles", wl_cb->isChecked());
  c->writeEntry("EnableDrawerTiles", drawer_cb->isChecked());

  c->writeEntry("KMenuTile", kmenu_input->currentText());
  c->writeEntry("URLTile", url_input->currentText());
  c->writeEntry("BrowserTile", browser_input->currentText());
  c->writeEntry("ExeTile", exe_input->currentText());
  c->writeEntry("WindowListTile", wl_input->currentText());
  c->writeEntry("DrawerTile", drawer_input->currentText());

  c->sync();

  delete c;
}

void ButtonTab::defaults()
{
  tiles_cb->setChecked(false);

  kmenu_group->setEnabled(false);
  url_group->setEnabled(false);
  exe_group->setEnabled(false);
  browser_group->setEnabled(false);
  wl_group->setEnabled(false);
  drawer_group->setEnabled(false);

  kmenu_cb->setChecked(true);
  url_cb->setChecked(true);
  browser_cb->setChecked(true);
  exe_cb->setChecked(true);
  wl_cb->setChecked(true);
  drawer_cb->setChecked(true);
}

void ButtonTab::fill_tile_input()
{
  tiles = queryAvailableTiles();

  kmenu_input->clear();
  url_input->clear();
  browser_input->clear();
  exe_input->clear();
  wl_input->clear();
  drawer_input->clear();

  kmenu_input->insertStringList(tiles);
  url_input->insertStringList(tiles);
  browser_input->insertStringList(tiles);
  exe_input->insertStringList(tiles);
  wl_input->insertStringList(tiles);
  drawer_input->insertStringList(tiles);
}

QStringList ButtonTab::queryAvailableTiles()
{
  QStringList list = KGlobal::dirs()->findAllResources("tiles","*_large_up.png");
  QStringList list2;

  for (QStringList::Iterator it = list.begin(); it != list.end(); ++it)
    {
      QString tile = (*it);
      QFileInfo fi(tile);
      tile = fi.fileName();
      tile.truncate(tile.find("_large_up.png"));
      list2.append(tile);
    }
  list2.sort();
  return list2;
}

QString ButtonTab::quickHelp() const
{
  return i18n("");
}
