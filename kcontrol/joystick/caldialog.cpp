/***************************************************************************
 *   Copyright (C) 2003 by Martin Koller                                   *
 *   m.koller@surfeu.at                                                    *
 *   This file is part of the KDE Control Center Module for Joysticks      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "caldialog.h"
#include "joydevice.h"

#include <qlabel.h>
#include <qtimer.h>
#include <qapplication.h>
#include <qvbox.h>

#include <klocale.h>
#include <kmessagebox.h>
#include <kdebug.h>

//--------------------------------------------------------------

CalDialog::CalDialog(QWidget *parent, JoyDevice *joy)
  : KDialogBase(parent, "calibrateDialog", true,
      i18n("Calibration"),
      KDialogBase::Cancel, KDialogBase::Cancel, true),
    joydev(joy)
{
  QVBox *main = makeVBoxMainWidget();

  text = new QLabel(main);
  text->setMinimumHeight(200);
  valueLbl = new QLabel(main);
}

//--------------------------------------------------------------

void CalDialog::calibrate()
{
  text->setText(i18n("Please wait a moment to calculate the precision"));
  setResult(-1);
  show();

  // calibrate precision (which min,max delivers the joystick in its center position)
  // get values through the normal idle procedure
  QTimer ti;
  ti.start(2000, true); // single shot in 2 seconds

  do
  {
    qApp->processEvents(2000);
  }
  while ( ti.isActive() && (result() != QDialog::Rejected) );

  joydev->calcPrecision();

  int i, lastVal;
  int min[2], center[2], max[2];

  for (i = 0; i < joydev->numAxes(); i++)
  {
    // minimum position
    text->setText(i18n("<qt>Calibration is about to check the value range your device delivers.<br><br>"
                       "Please move <b>axis %1</b> on your device to the <b>minimum</b> position.<br><br>"
                       "Press any button on the device to continue with the next step.</qt>").arg(i+1));
    waitButton(i, true, lastVal);
    joydev->resetMinMax(i, lastVal);
    waitButton(i, false, lastVal);

    min[0] = joydev->axisMin(i);
    min[1] = joydev->axisMax(i);

    if ( result() == QDialog::Rejected ) return;  // user cancelled the dialog

    // center position
    text->setText(i18n("<qt>Calibration is about to check the value range your device delivers.<br><br>"
                       "Please move <b>axis %1</b> on your device to the <b>center</b> position.<br><br>"
                       "Press any button on the device to continue with the next step.</qt>").arg(i+1));
    waitButton(i, true, lastVal);
    joydev->resetMinMax(i, lastVal);
    waitButton(i, false, lastVal);

    center[0] = joydev->axisMin(i);
    center[1] = joydev->axisMax(i);

    if ( result() == QDialog::Rejected ) return;  // user cancelled the dialog

    // maximum position
    text->setText(i18n("<qt>Calibration is about to check the value range your device delivers.<br><br>"
                       "Please move <b>axis %1</b> on your device to the <b>maximum</b> position.<br><br>"
                       "Press any button on the device to continue with the next step.</qt>").arg(i+1));
    waitButton(i, true, lastVal);
    joydev->resetMinMax(i, lastVal);
    waitButton(i, false, lastVal);

    max[0] = joydev->axisMin(i);
    max[1] = joydev->axisMax(i);

    if ( result() == QDialog::Rejected ) return;  // user cancelled the dialog

    joydev->calcCorrection(i, min, center, max);
  }

  JoyDevice::ErrorCode ret = joydev->applyCalibration();

  if ( ret != JoyDevice::SUCCESS )
  {
    KMessageBox::error(this, joydev->errText(ret), i18n("Communication Error"));
    reject();
  }

  KMessageBox::information(this, i18n("You have successfully calibrated your device"), i18n("Calibration Success"));
  accept();
}

//--------------------------------------------------------------

void CalDialog::waitButton(int axis, bool press, int &lastVal)
{
  JoyDevice::EventType type;
  int number, value;
  bool button = false;
  lastVal = 0;

  // loop until the user presses a button on the device
  do
  {
    qApp->processEvents(100);

    if ( joydev->getEvent(type, number, value) )
    {
      button = ( (type == JoyDevice::BUTTON) && (press ? (value == 1) : (value == 0)) );

      if ( (type == JoyDevice::AXIS) && (number == axis) )
        valueLbl->setText(i18n("Value Axis %1: %2").arg(axis+1).arg(lastVal = value));
    }
  }
  while ( !button && (result() != QDialog::Rejected) );
}

//--------------------------------------------------------------
