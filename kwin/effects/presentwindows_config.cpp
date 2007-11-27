/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Rivo Laks <rivolaks@hot.ee>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#include "presentwindows_config.h"

#include <kwineffects.h>

#include <klocale.h>
#include <kdebug.h>
#include <kconfiggroup.h>
#include <KActionCollection>
#include <kaction.h>
#include <KShortcutsEditor>
#include <KGlobalAccel>

#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QComboBox>

KWIN_EFFECT_CONFIG_FACTORY

namespace KWin
{

PresentWindowsEffectConfig::PresentWindowsEffectConfig(QWidget* parent, const QVariantList& args) :
        KCModule(EffectFactory::componentData(), parent, args)
    {
    kDebug() ;

    QGridLayout* layout = new QGridLayout(this);

    layout->addWidget(new QLabel(i18n("Activate when cursor is at a specific edge "
            "or corner of the screen:"), this), 0, 0, 1, 3);
    layout->addItem(new QSpacerItem(20, 20, QSizePolicy::Fixed), 1, 0, 2, 1);

    layout->addWidget(new QLabel(i18n("for windows on current desktop: "), this), 1, 1);
    mActivateCombo = new QComboBox;
    addItems(mActivateCombo);
    connect(mActivateCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    layout->addWidget(mActivateCombo, 1, 2);

    layout->addWidget(new QLabel(i18n("for windows on all desktops: "), this), 2, 1);
    mActivateAllCombo = new QComboBox;
    addItems(mActivateAllCombo);
    connect(mActivateAllCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changed()));
    layout->addWidget(mActivateAllCombo, 2, 2);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Expanding), 3, 0, 1, 3);

    // Shortcut config
    KGlobalAccel::self()->overrideMainComponentData(componentData());
    KActionCollection* actionCollection = new KActionCollection( this );
    KAction* a = (KAction*)actionCollection->addAction( "Expose" );
    a->setText( i18n("Toggle Expose Effect" ));
    a->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F10));
    KAction* b = (KAction*)actionCollection->addAction( "ExposeAll" );
    b->setText( i18n("Toggle Expose effect (incl other desktops)" ));
    b->setGlobalShortcut(KShortcut(Qt::CTRL + Qt::Key_F11));
    mShortcutEditor = new KShortcutsEditor(actionCollection, this,
            KShortcutsEditor::GlobalAction, KShortcutsEditor::LetterShortcutsDisallowed);
    connect(mShortcutEditor, SIGNAL(keyChange()), this, SLOT(changed()));
    layout->addWidget(mShortcutEditor, 4, 0, 1, 3);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 5, 0, 1, 3);


    load();
    }

PresentWindowsEffectConfig::~PresentWindowsEffectConfig()
    {
    kDebug() ;
    }

void PresentWindowsEffectConfig::addItems(QComboBox* combo)
    {
    combo->addItem(i18n("Top"));
    combo->addItem(i18n("Top-right"));
    combo->addItem(i18n("Right"));
    combo->addItem(i18n("Bottom-right"));
    combo->addItem(i18n("Bottom"));
    combo->addItem(i18n("Bottom-left"));
    combo->addItem(i18n("Left"));
    combo->addItem(i18n("Top-left"));
    combo->addItem(i18n("None"));
    }

void PresentWindowsEffectConfig::load()
    {
    kDebug() ;
    KCModule::load();

    KConfigGroup conf = EffectsHandler::effectConfig("PresentWindows");
    int activateBorder = conf.readEntry("BorderActivate", (int)ElectricNone);
    if(activateBorder == (int)ElectricNone)
        activateBorder--;
    mActivateCombo->setCurrentIndex(activateBorder);

    int activateAllBorder = conf.readEntry("BorderActivateAll", (int)ElectricTopLeft);
    if(activateAllBorder == (int)ElectricNone)
        activateAllBorder--;
    mActivateAllCombo->setCurrentIndex(activateAllBorder);

    emit changed(false);
    }

void PresentWindowsEffectConfig::save()
    {
    kDebug() ;
    KCModule::save();

    KConfigGroup conf = EffectsHandler::effectConfig("PresentWindows");

    int activateBorder = mActivateCombo->currentIndex();
    if(activateBorder == (int)ELECTRIC_COUNT)
        activateBorder = (int)ElectricNone;
    conf.writeEntry("BorderActivate", activateBorder);

    int activateAllBorder = mActivateAllCombo->currentIndex();
    if(activateAllBorder == (int)ELECTRIC_COUNT)
        activateAllBorder = (int)ElectricNone;
    conf.writeEntry("BorderActivateAll", activateAllBorder);

    conf.sync();

    emit changed(false);
    EffectsHandler::sendReloadMessage( "presentwindows" );
    }

void PresentWindowsEffectConfig::defaults()
    {
    kDebug() ;
    mActivateCombo->setCurrentIndex( (int)ElectricNone - 1 );
    mActivateAllCombo->setCurrentIndex( (int)ElectricTopLeft );
    mShortcutEditor->allDefault();
    emit changed(true);
    }


} // namespace

#include "presentwindows_config.moc"
