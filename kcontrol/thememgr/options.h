/*
 * Copyright (c) 1998 Stefan Taferner <taferner@kde.org>
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#include <qwidget.h>

class QLabel;
class QCheckBox;
class QComboBox;
class QPushButton;
class QBoxLayout;
class QGridLayout;

#define OptionsInherited QWidget
class Options : public QWidget
{
  Q_OBJECT
public:
  Options(QWidget *parent=0, const char* name=0, bool init=FALSE);
  ~Options();

  virtual void load();
  virtual void save();

  /** Update status information on available groups of current theme. */
  virtual void updateStatus(void);

signals:
  void changed( bool state );

protected slots:
  virtual void slotThemeChanged();
  virtual void slotThemeApply();
  virtual void slotCbxClicked();
  virtual void slotDetails();
  virtual void slotInvert();
  virtual void slotClear();

protected:
  /** Creates a new options line */
  virtual QCheckBox* newLine(const char* groupName, const QString& text,
			     QLabel** statusPtr);

  virtual void readConfig();
  virtual void writeConfig();

  virtual void updateStatus(const char* groupName, QLabel* status);

protected:
  QCheckBox *mCbxColors;
  QCheckBox *mCbxStyle;
  QCheckBox *mCbxWallpapers;
  QCheckBox *mCbxSounds;
  QCheckBox *mCbxIcons;
  QCheckBox *mCbxWM;
  QCheckBox *mCbxPanel;
  QCheckBox *mCbxKmenu;
  QCheckBox *mCbxOverwrite;
  QLabel *mStatColors;
  QLabel *mStatStyle;
  QLabel *mStatWallpapers;
  QLabel *mStatSounds;
  QLabel *mStatIcons;
  QLabel *mStatWM;
  QLabel *mStatPanel;
  QLabel *mStatKmenu;
  QGridLayout *mGrid;
  bool mGui;
  int mGridRow;
};

#endif /*OPTIONS_H*/

