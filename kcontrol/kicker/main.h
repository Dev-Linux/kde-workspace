/*
 *  main.h
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

#ifndef __main_h__
#define __main_h__

#include <kcmodule.h>

class QTabWidget;
class PanelTab;
class LnFTab;
class MenuTab;
class ButtonTab;
class AppletTab;

class KickerConfig : public KCModule
{
  Q_OBJECT

 public:
  KickerConfig(QWidget *parent = 0L, const char *name = 0L);
  
  void load();
  void save();
  void defaults();
  QString quickHelp() const;

 public slots:
  void configChanged();
 
 private:
  QTabWidget   *tab;
  PanelTab     *paneltab;
  LnFTab       *lnftab;
  MenuTab      *menutab;
  ButtonTab    *buttontab;
  AppletTab    *applettab;
};

#endif // __main_h__
