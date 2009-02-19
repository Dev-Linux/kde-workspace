/*
 *  Copyright 2007 Andreas Pakulat <apaku@gmx.de>
 *  Copyright 2008 Michael Jansen <kde@michael-jansen.biz>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "globalshortcuts.h"

#include "kglobalshortcutseditor.h"
#include "select_scheme_dialog.h"

#include <KDebug>
#include <KFileDialog>
#include <KLocale>
#include <KMessageBox>
#include <KPluginFactory>
#include <KPushButton>

#include <QLayout>


K_PLUGIN_FACTORY(GlobalShortcutsModuleFactory, registerPlugin<GlobalShortcutsModule>();)
K_EXPORT_PLUGIN(GlobalShortcutsModuleFactory("kcmkeys"))


GlobalShortcutsModule::GlobalShortcutsModule(QWidget *parent, const QVariantList &args)
 : KCModule(GlobalShortcutsModuleFactory::componentData(), parent, args),
   editor(0)
{
    KCModule::setButtons(KCModule::Buttons(KCModule::Default | KCModule::Apply));

    // Add import scheme button
    KPushButton *importButton = new KPushButton(this);
    importButton->setText(i18n("Import Scheme..."));
    connect(importButton, SIGNAL(clicked()), this, SLOT(importScheme()));

    // Add export scheme button
    KPushButton *exportButton = new KPushButton(this);
    exportButton->setText(i18n("Export Scheme..."));
    connect(exportButton, SIGNAL(clicked()), this, SLOT(exportScheme()));

    // Layout for the buttons
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(exportButton);
    hbox->addWidget(importButton);

    // Create the kglobaleditor
    editor = new KGlobalShortcutsEditor(this, KShortcutsEditor::GlobalAction);
    connect(editor, SIGNAL(changed(bool)), this, SIGNAL(changed(bool)));

    // Layout the hole bunch
    QVBoxLayout *global = new QVBoxLayout;
    global->addLayout(hbox);
    global->addWidget(editor);
    setLayout(global);
}

GlobalShortcutsModule::~GlobalShortcutsModule()
{}


void GlobalShortcutsModule::load()
{
    editor->load();
}


void GlobalShortcutsModule::defaults()
{
    editor->allDefault();
}


void GlobalShortcutsModule::save()
{
    editor->save();
}


void GlobalShortcutsModule::importScheme()
{
    // Check for unsaved modifications
    if (editor->isModified()) {
        int choice = KMessageBox::warningContinueCancel(
                         parentWidget(),
                         i18n("Your current changes will be lost if you load another scheme before saving this one"),
                         i18n("Load Shortcurt Scheme"),
                         KGuiItem(i18n("Load")));
        if (choice != KMessageBox::Continue) {
            return;
        }
    }

    SelectSchemeDialog dialog(this);
    if (dialog.exec() != KDialog::Accepted) {
        return;
    }

    KUrl url = dialog.selectedScheme();
    if (!url.isLocalFile()) {
        KMessageBox::sorry(this, i18n("This file (%1) does not exist. You can only select local files.",
                           url.url()));
        return;
    }
    kDebug() << url.path();
    KConfig config(url.path());
    editor->importConfiguration(&config);
}


void GlobalShortcutsModule::exportScheme()
{
    KUrl url = KFileDialog::getSaveFileName(KUrl(), "*.kksrc", parentWidget());
    if (!url.isEmpty()) {
        KConfig config(url.path());
        config.deleteGroup("Shortcuts");
        config.deleteGroup("Global Shortcuts");
        editor->exportConfiguration(&config);
    }
}

#include "globalshortcuts.moc"
