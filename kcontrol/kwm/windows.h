/*
 * windows.h
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

#ifndef __KWINDOWCONFIG_H__
#define __KWINDOWCONFIG_H__

#include <kcmodule.h>

class QRadioButton;
class QCheckBox;
class QPushButton;
class QComboBox;
class QLCDNumber;
class QLabel;
class QSlider;
class QButtonGroup;
class QSpinBox;

class KIntNumInput;

#define TRANSPARENT 0
#define OPAQUE      1

#define CLICK_TO_FOCUS     0
#define FOCUS_FOLLOW_MOUSE 1

#define TITLEBAR_PLAIN  0
#define TITLEBAR_SHADED 1

#define RESIZE_TRANSPARENT  0
#define RESIZE_OPAQUE       1

#define SMART_PLACEMENT       0
#define CASCADE_PLACEMENT     1
#define RANDOM_PLACEMENT      2
#define INTERACTIVE_PLACEMENT 3
#define MANUAL_PLACEMENT      4

#define  CLICK_TO_FOCUS                0
#define  FOCUS_FOLLOWS_MOUSE           1
#define  FOCUS_UNDER_MOUSE   2
#define  FOCUS_STRICTLY_UNDER_MOUSE   3

class QSpinBox;

class KWindowConfig : public KCModule
{
  Q_OBJECT
public:
  KWindowConfig( QWidget *parent=0, const char* name=0 );
  ~KWindowConfig();

  void load();
  void save();
  void defaults();
  QString quickHelp() const;

  void loadSettings();
  void applySettings();

private slots:
  void setAutoRaiseEnabled();
  //  void ifPlacementIsInteractive();
  void autoRaiseOnTog(bool);//CT 23Oct1998
  void clickRaiseOnTog(bool);
  void slotChanged();

private:

  int getMove( void );
  bool getMinimizeAnim( void );
  int getMinimizeAnimSpeed( void );
  int getResizeOpaque ( void );
  int getPlacement( void ); //CT
  int getFocus( void );
  int getAutoRaiseInterval( void );

  void setMove(int);
  void setMinimizeAnim(bool,int);
  void setResizeOpaque(int);
  void setPlacement(int); //CT
  void setFocus(int);
  void setAutoRaiseInterval(int);
  void setAutoRaise(bool);
  void setClickRaise(bool);
  void setAnimateShade(bool);
  void setAltTabMode(bool);
  void setCtrlTab(bool);

  QButtonGroup *windowsBox;
  QCheckBox *opaque;

  QCheckBox *resizeOpaqueOn;
  QCheckBox* minimizeAnimOn;
  QSlider *minimizeAnimSlider;
  QLabel *minimizeAnimSlowLabel, *minimizeAnimFastLabel;
  QCheckBox *animateShade;

  //CT 19jan98; 21Oct1998
  //QButtonGroup *plcBox;
  QComboBox *placementCombo;
  //QSpinBox *interactiveTrigger;
  //QLabel *iTLabel;

  QButtonGroup *fcsBox;
  QComboBox *focusCombo;
  QCheckBox *autoRaiseOn;
  QCheckBox *clickRaiseOn;
  KIntNumInput *autoRaise;
  QLabel *alabel;
  QLCDNumber *s;
  //CT  QLabel *sec;

  QButtonGroup *kbdBox;
  QRadioButton *kdeMode;
  QRadioButton *cdeMode;
  QCheckBox    *ctrlTab;
};

#endif

