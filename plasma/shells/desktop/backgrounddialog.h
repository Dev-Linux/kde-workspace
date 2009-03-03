/*
  Copyright (c) 2007 Paolo Capriotti <p.capriotti@gmail.com>
  Copyright (c) 2008 by Petri Damsten <damu@iki.fi>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
*/

#ifndef BACKGROUNDDIALOG_H
#define BACKGROUNDDIALOG_H

#include <KConfigDialog>
#include "ui_BackgroundDialog.h"

namespace Plasma {
    class Wallpaper;
    class Containment;
    class View;
}
class ThemeModel;
class WallpaperPreview;
class QStandardItemModel;
class ScreenPreviewWidget;

class BackgroundDialog : public KConfigDialog, public Ui::BackgroundDialog
{
    Q_OBJECT
public:
    BackgroundDialog(const QSize &res, Plasma::Containment *containment,
                     Plasma::View *view, QWidget* parent, const QString &id,
                     KConfigSkeleton *s);
    ~BackgroundDialog();

    void reloadConfig();

public slots:
    void saveConfig();

private:
    KConfigGroup wallpaperConfig(const QString &plugin);

private slots:
    void getNewThemes();
    void changeBackgroundMode(int mode);
    void cleanup();

private:
    ThemeModel* m_themeModel;
    QStandardItemModel* m_containmentModel;
    Plasma::Wallpaper* m_wallpaper;
    Plasma::View* m_view;
    Plasma::Containment* m_containment;
    ScreenPreviewWidget* m_preview;
};

#endif // BACKGROUNDDIALOG_H
