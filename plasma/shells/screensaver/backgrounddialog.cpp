/*
  Copyright (c) 2007 Paolo Capriotti <p.capriotti@gmail.com>
  Copyright (C) 2007 Ivan Cukic <ivan.cukic+kde@gmail.com>
  Copyright (c) 2008 by Petri Damsten <damu@iki.fi>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
*/

#include "backgrounddialog.h"

#include <QPainter>
//#include <QFile>
//#include <QAbstractItemView>
//#include <QStandardItemModel>

//#include <KStandardDirs>
//#include <KDesktopFile>
//#include <KColorScheme>

#include <Plasma/Containment>
#include <Plasma/FrameSvg>
#include <Plasma/Wallpaper>
#include <Plasma/View>
//#include <Plasma/Corona>

#include "wallpaperpreview.h"

typedef QPair<QString, QString> WallpaperInfo;
Q_DECLARE_METATYPE(WallpaperInfo)


BackgroundDialog::BackgroundDialog(const QSize& res, Plasma::Containment *c, Plasma::View* view, QWidget* parent)
    : KDialog(parent),
      m_wallpaper(0),
      m_view(view),
      m_containment(c),
      m_preview(0)
{
    //setWindowIcon(KIcon("preferences-desktop-wallpaper"));
    setCaption(i18n("Background Settings"));
    showButtonSeparator(true);
    setButtons(Ok | Cancel | Apply);

    QWidget * main = new QWidget(this);
    setupUi(main);

    // Size of monitor image: 200x186
    // Geometry of "display" part of monitor image: (23,14)-[151x115]
    qreal previewRatio = (qreal)res.height() / (qreal)res.width();
    QSize monitorSize(200, int(200 * previewRatio));

    Plasma::FrameSvg *svg = new Plasma::FrameSvg(this);
    svg->setImagePath("widgets/monitor");
    svg->resizeFrame(monitorSize);
    QPixmap monitorPix(monitorSize + QSize(0, svg->elementSize("base").height() - svg->marginSize(Plasma::BottomMargin)));
    monitorPix.fill(Qt::transparent);

    QPainter painter(&monitorPix);
    QPoint standPosition(monitorSize.width()/2 - svg->elementSize("base").width()/2, svg->contentsRect().bottom());
    svg->paint(&painter, QRect(standPosition, svg->elementSize("base")), "base");
    svg->paintFrame(&painter);
    painter.end();

    m_monitor->setPixmap(monitorPix);
    m_monitor->setWhatsThis(i18n(
        "This picture of a monitor contains a preview of "
        "what the current settings will look like on your desktop."));
    m_preview = new WallpaperPreview(m_monitor);
    m_preview->setGeometry(svg->contentsRect().toRect());

    connect(this, SIGNAL(finished(int)), this, SLOT(cleanup()));
    connect(this, SIGNAL(okClicked()), this, SLOT(saveConfig()));
    connect(this, SIGNAL(applyClicked()), this, SLOT(saveConfig()));

    setMainWidget(main);
    reloadConfig();
    adjustSize();
}

BackgroundDialog::~BackgroundDialog()
{
    cleanup();
}

void BackgroundDialog::cleanup()
{
    //FIXME could be bad if we get hidden and reshown
    delete m_wallpaper;
    m_wallpaper = 0;
}

void BackgroundDialog::reloadConfig()
{
    disconnect(m_wallpaperMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changeBackgroundMode(int)));
    int wallpaperIndex = 0;

    // Wallpaper
    bool doWallpaper = m_containment->drawWallpaper();
    m_wallpaperLabel->setVisible(doWallpaper);
    m_wallpaperGroup->setVisible(doWallpaper);
    if (doWallpaper) {
        // Load wallpaper plugins
        QString currentPlugin;
        QString currentMode;

        Plasma::Wallpaper *currentWallpaper = m_containment->wallpaper();
        if (currentWallpaper) {
            currentPlugin = currentWallpaper->pluginName();
            currentMode = currentWallpaper->renderingMode().name();
        }

        KPluginInfo::List plugins = Plasma::Wallpaper::listWallpaperInfo();
        m_wallpaperMode->clear();
        int i = 0;
        foreach (KPluginInfo info, plugins) {
            bool matches = info.pluginName() == currentPlugin;
            const QList<KServiceAction>& modes = info.service()->actions();
            if (modes.count() > 0) {
                foreach (const KServiceAction& mode, modes) {
                    m_wallpaperMode->addItem(KIcon(mode.icon()), mode.text(),
                                    QVariant::fromValue(WallpaperInfo(info.pluginName(), mode.name())));
                    if (matches && mode.name() == currentMode) {
                        wallpaperIndex = i;
                    }
                    ++i;
                }
            } else {
                m_wallpaperMode->addItem(KIcon(info.icon()), info.name(),
                                QVariant::fromValue(WallpaperInfo(info.pluginName(), QString())));
                if (matches) {
                    wallpaperIndex = i;
                }
                ++i;
            }
        }
        m_wallpaperMode->setCurrentIndex(wallpaperIndex);
        changeBackgroundMode(wallpaperIndex);
    }

    connect(m_wallpaperMode, SIGNAL(currentIndexChanged(int)), this, SLOT(changeBackgroundMode(int)));
}

void BackgroundDialog::changeBackgroundMode(int mode)
{
    kDebug();
    QWidget* w = 0;
    WallpaperInfo wallpaperInfo = m_wallpaperMode->itemData(mode).value<WallpaperInfo>();

    if (m_wallpaperGroup->layout()->count() > 1) {
        delete dynamic_cast<QWidgetItem*>(m_wallpaperGroup->layout()->takeAt(1))->widget();
    }

    if (m_wallpaper && m_wallpaper->pluginName() != wallpaperInfo.first) {
        delete m_wallpaper;
        m_wallpaper = 0;
    }

    if (!m_wallpaper) {
        m_wallpaper = Plasma::Wallpaper::load(wallpaperInfo.first);
        m_preview->setWallpaper(m_wallpaper);
    }

    if (m_wallpaper) {
        m_wallpaper->setRenderingMode(wallpaperInfo.second);
        KConfigGroup cfg = wallpaperConfig(wallpaperInfo.first);
        kDebug() << "making a" << wallpaperInfo.first << "in mode" << wallpaperInfo.second;
        m_wallpaper->restore(cfg);
        w = m_wallpaper->createConfigurationInterface(m_wallpaperGroup);
    }

    if (!w) {
        w = new QWidget(m_wallpaperGroup);
    }

    m_wallpaperGroup->layout()->addWidget(w);
}

KConfigGroup BackgroundDialog::wallpaperConfig(const QString &plugin)
{
    Q_ASSERT(m_containment);

    //FIXME: we have details about the structure of the containment config duplicated here!
    KConfigGroup cfg = m_containment->config();
    cfg = KConfigGroup(&cfg, "Wallpaper");
    return KConfigGroup(&cfg, plugin);
}

void BackgroundDialog::saveConfig()
{
    QString wallpaperPlugin = m_wallpaperMode->itemData(m_wallpaperMode->currentIndex()).value<WallpaperInfo>().first;
    QString wallpaperMode = m_wallpaperMode->itemData(m_wallpaperMode->currentIndex()).value<WallpaperInfo>().second;

    // Wallpaper
    Plasma::Wallpaper *currentWallpaper = m_containment->wallpaper();
    if (currentWallpaper) {
        KConfigGroup cfg = wallpaperConfig(currentWallpaper->pluginName());
        currentWallpaper->save(cfg);
    }

    if (m_wallpaper) {
        KConfigGroup cfg = wallpaperConfig(m_wallpaper->pluginName());
        m_wallpaper->save(cfg);
    }

    m_containment->setWallpaper(wallpaperPlugin, wallpaperMode);
}
