/* KDE Display color scheme setup module
 * Copyright (C) 2007 Matthew Woehlke <mw_triad@users.sourceforge.net>
 * Copyright (C) 2007 Jeremy Whiting <jeremy@scitools.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "colorscm.h"

#include "../krdb/krdb.h"

#include <QtCore/QFileInfo>
#include <QtGui/QHeaderView>
#include <QtGui/QStackedWidget>
#include <QtGui/QPainter>
#include <QtGui/QBitmap>
#include <QtDBus/QtDBus>

#include <KAboutData>
#include <KColorButton>
#include <KColorDialog>
#include <KFileDialog>
#include <KGenericFactory>
#include <KGlobal>
#include <KGlobalSettings>
#include <KInputDialog>
#include <KListWidget>
#include <KMessageBox>
#include <KStandardDirs>
#include <kio/netaccess.h>

K_PLUGIN_FACTORY( KolorFactory, registerPlugin<KColorCm>(); )
K_EXPORT_PLUGIN( KolorFactory("kcmcolors") )

//BEGIN WindecoColors
KColorCm::WindecoColors::WindecoColors(const KSharedConfigPtr &config)
{
    load(config);
}

void KColorCm::WindecoColors::load(const KSharedConfigPtr &config)
{
    KConfigGroup group(config, "WM");
    m_colors[ActiveBackground] = group.readEntry("activeBackground", KGlobalSettings::activeTitleColor());
    m_colors[ActiveForeground] = group.readEntry("activeForeground", KGlobalSettings::activeTextColor());
    m_colors[InactiveBackground] = group.readEntry("inactiveBackground", KGlobalSettings::inactiveTitleColor());
    m_colors[InactiveForeground] = group.readEntry("inactiveForeground", KGlobalSettings::activeTitleColor());
}

QColor KColorCm::WindecoColors::color(WindecoColors::Role role) const
{
    return m_colors[role];
}
//END WindecoColors

KColorCm::KColorCm(QWidget *parent, const QVariantList &)
    : KCModule( KolorFactory::componentData(), parent )
{
    KAboutData* about = new KAboutData(
        "kcmcolors", 0, ki18n("Colors"), 0, KLocalizedString(),
        KAboutData::License_GPL,
        ki18n("(c) 2007 Matthew Woehlke")
    );
    about->addAuthor( ki18n("Matthew Woehlke"), KLocalizedString(),
                     "mw_triad@users.sourceforge.net" );
    about->addAuthor( ki18n("Jeremy Whiting"), KLocalizedString(), "jeremy@scitools.com");
    setAboutData( about );

    m_config = KSharedConfig::openConfig("kdeglobals");

    setupUi(this);

    connect(colorSet, SIGNAL(currentIndexChanged(int)), this, SLOT(updateColorTable()));
    connect(schemeList, SIGNAL(currentRowChanged(int)), this, SLOT(loadScheme()));
    connect(applyToAlien, SIGNAL(toggled(bool)), this, SLOT(emitChanged()));

    // only needs to be called once
    setupColorTable();

    load();
}

KColorCm::~KColorCm()
{
    m_config->markAsClean();
}

void KColorCm::populateSchemeList()
{
    // clear the list in case this is being called from reset button click
    schemeList->clear();
    QStringList schemeFiles = KGlobal::dirs()->findAllResources("data", "color-schemes/*.colors", KStandardDirs::NoDuplicates);
    for (int i = 0; i < schemeFiles.size(); ++i)
    {
        // get the file name
        QString filename = schemeFiles[i];
        QFileInfo info(filename);

        // add the entry
        KSharedConfigPtr config = KSharedConfig::openConfig(filename);
        QIcon icon = createSchemePreviewIcon(KGlobalSettings::createApplicationPalette(config),
                                             WindecoColors(config));
        schemeList->addItem(new QListWidgetItem(icon, info.baseName()));
    }
}

void KColorCm::updatePreviews()
{
    schemePreview->setPalette(m_config);
    inactivePreview->setPalette(m_config, QPalette::Inactive);
    disabledPreview->setPalette(m_config, QPalette::Disabled);
}

void KColorCm::updateEffectsPage()
{
    // NOTE: keep this in sync with kdelibs/kdeui/colors/kcolorscheme.cpp
    KConfigGroup groupI(m_config, "ColorEffects:Inactive");
    inactiveIntensitySlider->setValue(int(groupI.readEntry("IntensityAmount", 0.0) * 20.0) + 20);
    inactiveIntensityBox->setCurrentIndex(groupI.readEntry("IntensityEffect", 0));
    inactiveColorSlider->setValue(int(groupI.readEntry("ColorAmount", 0.0) * 20.0) + 20);
    inactiveColorBox->setCurrentIndex(groupI.readEntry("ColorEffect", 0));
    inactiveContrastSlider->setValue(int(groupI.readEntry("ContrastAmount", 0.0) * 20.0));
    inactiveContrastBox->setCurrentIndex(groupI.readEntry("ContrastEffect", 1));

    // NOTE: keep this in sync with kdelibs/kdeui/colors/kcolorscheme.cpp
    KConfigGroup groupD(m_config, "ColorEffects:Disabled");
    disabledIntensitySlider->setValue(int(groupD.readEntry("IntensityAmount", 0.0) * 20.0) + 20);
    disabledIntensityBox->setCurrentIndex(groupD.readEntry("IntensityEffect", 0));
    disabledColorSlider->setValue(int(groupD.readEntry("ColorAmount", 0.0) * 20.0) + 20);
    disabledColorBox->setCurrentIndex(groupD.readEntry("ColorEffect", 0));
    disabledContrastSlider->setValue(int(groupD.readEntry("ContrastAmount", 0.7) * 20.0));
    disabledContrastBox->setCurrentIndex(groupD.readEntry("ContrastEffect", 1));
}

void KColorCm::loadScheme(const QString &path)
{
    KSharedConfigPtr temp = m_config;
    m_config = KSharedConfig::openConfig(path);
    updateColorSchemes();

    KConfigGroup groupG(m_config, "General");
    shadeSortedColumn->setChecked(groupG.readEntry("shadeSortColumn", true) ? Qt::Checked : Qt::Unchecked);

    KConfigGroup groupK(m_config, "KDE");
    contrastSlider->setValue(groupK.readEntry("contrast").toInt());

    updateEffectsPage(); // intentionally before swapping back m_config

    m_config = temp;
    updateFromColorSchemes();
    updateColorTable();
    updatePreviews();
}

void KColorCm::loadScheme()
{
    if (schemeList->currentItem() != NULL)
    {
        QString path = KGlobal::dirs()->findResource("data",
            "color-schemes/" + schemeList->currentItem()->text() + ".colors");

        int permissions = QFile(path).permissions();
        bool canWrite = (permissions & QFile::WriteUser);
        schemeRemoveButton->setEnabled(canWrite);

        loadScheme(path);

        emit changed(true);
    }
}

void KColorCm::on_schemeRemoveButton_clicked()
{
    if (schemeList->currentItem() != NULL)
    {
        QString path = KGlobal::dirs()->findResource("data",
            "color-schemes/" + schemeList->currentItem()->text() + ".colors");
        if (KIO::NetAccess::del(path, this))
        {
            delete schemeList->takeItem(schemeList->currentRow());
        }
        else
        {
            // deletion failed, so show an error message
            KMessageBox::error(this, i18n("You do not have permission to delete that scheme"), i18n("Error"));
        }
    }
}

void KColorCm::on_schemeImportButton_clicked()
{
    // get the path to the scheme to import
    KUrl url = KFileDialog::getOpenUrl(KUrl(), QString(), this, i18n("Import Color Scheme"));

    // TODO: possibly untar or uncompress it
    // open it

    // load the scheme
    loadScheme(url.path());

    // save it
    saveScheme(url.fileName());
}

void KColorCm::on_schemeSaveButton_clicked()
{
    QString previousName;
    if (schemeList->currentItem() != NULL)
    {
        previousName = schemeList->currentItem()->text();
    }
    // prompt for the name to save as
    bool ok;
    QString name = KInputDialog::getText(i18n("Save Color Scheme"),
        i18n("&Enter a name for the color scheme:"), previousName, &ok, this);
    if (ok)
    {
        saveScheme(name);
    }
}

void KColorCm::saveScheme(const QString &name)
{
    // check if that name is already in the list
    QString path = KGlobal::dirs()->saveLocation("data", "color-schemes/") +
        name + ".colors";

    QFile file(path);
    int permissions = file.permissions();
    bool canWrite = (permissions & QFile::WriteUser);
    // or if we can overwrite it if it exists
    if (path.isEmpty() || !file.exists() ||
        (canWrite && KMessageBox::questionYesNo(this,
        i18n("A color scheme with that name already exists.\nDo you want to overwrite it?"),
        i18n("Save Color Scheme"),
        KStandardGuiItem::overwrite(),
        KStandardGuiItem::cancel())
        == KMessageBox::Yes))
    {
        // go ahead and save it
        QString newpath = KGlobal::dirs()->saveLocation("data", "color-schemes/");
        newpath += name + ".colors";
        KSharedConfigPtr temp = m_config;
        m_config = KSharedConfig::openConfig(newpath);
        // then copy current colors into new config
        updateFromColorSchemes();
        KConfigGroup group(m_config, "General");
        group.writeEntry("shadeSortColumn", (bool)shadeSortedColumn->checkState());
        KConfigGroup group2(m_config, "KDE");
        group2.writeEntry("contrast", contrastSlider->value());
        // sync it
        m_config->sync();

        QList<QListWidgetItem*> foundItems = schemeList->findItems(name, Qt::MatchExactly);
        QIcon icon = createSchemePreviewIcon(KGlobalSettings::createApplicationPalette(m_config),
                                             WindecoColors(m_config));
        if (foundItems.size() < 1)
        {
            // add it to the list since it's not in there already
            schemeList->addItem(new QListWidgetItem(icon, name));

            // then select the new item
            schemeList->setCurrentRow(schemeList->count() - 1);
        }
        else
        {
            // update the icon of the one that's in the list
            foundItems[0]->setIcon(icon);
            schemeList->setCurrentRow(schemeList->row(foundItems[0]));
        }

        // set m_config back to the system one
        m_config = temp;
        emit changed(false);
    }
    else if (!canWrite && file.exists())
    {
        // give error message if !canWrite && file.exists()
        KMessageBox::error(this, i18n("You do not have permission to overwrite that scheme"), i18n("Error"));
    }
}

void KColorCm::createColorEntry(QString text, QString key, QList<KColorButton *> &list, int index)
{
    KColorButton *button = new KColorButton(this);
    button->setObjectName(QString::number(index));
    connect(button, SIGNAL(changed(const QColor &)), this, SLOT(colorChanged(const QColor &)));
    list.append(button);

    m_colorKeys.insert(index, key);

    QTableWidgetItem *label = new QTableWidgetItem(text);
    colorTable->setItem(index, 0, label);
    colorTable->setCellWidget(index, 1, button);
}

void KColorCm::variesClicked()
{
    // find which button was changed
    int row = sender()->objectName().toInt();

    QColor color;
    if(KColorDialog::getColor(color, this ) != QDialog::Rejected )
    {
        changeColor(row, color);
        m_stackedWidgets[row - 9]->setCurrentIndex(0);
    }
}

QPixmap KColorCm::createSchemePreviewIcon(const QPalette &pal, const WindecoColors &wm)
{
    const uchar bits1[] = { 0xff, 0xff, 0xff, 0x2c, 0x16, 0x0b };
    const uchar bits2[] = { 0x68, 0x34, 0x1a, 0xff, 0xff, 0xff };
    const QSize bitsSize(24,2);
    const QBitmap b1 = QBitmap::fromData(bitsSize, bits1);
    const QBitmap b2 = QBitmap::fromData(bitsSize, bits2);

    QPixmap pixmap(23, 16);
    pixmap.fill(Qt::black); // ### use some color other than black for borders?

    QPainter p(&pixmap);

    p.fillRect( 1,  1, 7, 7, pal.brush(QPalette::Window));
    p.fillRect( 2,  2, 5, 2, QBrush(pal.color(QPalette::WindowText), b1));

    p.fillRect( 8,  1, 7, 7, pal.brush(QPalette::Button));
    p.fillRect( 9,  2, 5, 2, QBrush(pal.color(QPalette::ButtonText), b1));

    p.fillRect(15,  1, 7, 7, wm.color(WindecoColors::ActiveBackground));
    p.fillRect(16,  2, 5, 2, QBrush(wm.color(WindecoColors::ActiveForeground), b1));

    p.fillRect( 1,  8, 7, 7, pal.brush(QPalette::Base));
    p.fillRect( 2, 12, 5, 2, QBrush(pal.color(QPalette::Text), b2));

    p.fillRect( 8,  8, 7, 7, pal.brush(QPalette::Highlight));
    p.fillRect( 9, 12, 5, 2, QBrush(pal.color(QPalette::HighlightedText), b2));

    p.fillRect(15,  8, 7, 7, wm.color(WindecoColors::InactiveBackground));
    p.fillRect(16, 12, 5, 2, QBrush(wm.color(WindecoColors::InactiveForeground), b2));

    p.end();

    return pixmap;
}

void KColorCm::updateColorSchemes()
{
    m_colorSchemes.clear();

    m_colorSchemes.append(KColorScheme(QPalette::Active, KColorScheme::View, m_config));
    m_colorSchemes.append(KColorScheme(QPalette::Active, KColorScheme::Window, m_config));
    m_colorSchemes.append(KColorScheme(QPalette::Active, KColorScheme::Button, m_config));
    m_colorSchemes.append(KColorScheme(QPalette::Active, KColorScheme::Selection, m_config));
    m_colorSchemes.append(KColorScheme(QPalette::Active, KColorScheme::Tooltip, m_config));

    m_wmColors.load(m_config);

    // TODO KConfigGroup KDEgroup(m_config, "KDE");
    // TODO m_contrast = KDEgroup.readEntry("contrast", contrastSlider->value());
    // TODO m_shadeSorted = KDEgroup.readEntry("shadeSortColumn", (bool)shadeSortedColumn->checkState());
}

void KColorCm::updateFromColorSchemes()
{
    for (int i = KColorScheme::View; i <= KColorScheme::Tooltip; ++i)
    {
        KConfigGroup group(m_config, colorSetGroupKey(i));
        group.writeEntry("BackgroundNormal", m_colorSchemes[i].background(KColorScheme::NormalBackground).color());
        group.writeEntry("BackgroundAlternate", m_colorSchemes[i].background(KColorScheme::AlternateBackground).color());
        group.writeEntry("ForegroundNormal", m_colorSchemes[i].foreground(KColorScheme::NormalText).color());
        group.writeEntry("ForegroundInactive", m_colorSchemes[i].foreground(KColorScheme::InactiveText).color());
        group.writeEntry("ForegroundActive", m_colorSchemes[i].foreground(KColorScheme::ActiveText).color());
        group.writeEntry("ForegroundLink", m_colorSchemes[i].foreground(KColorScheme::LinkText).color());
        group.writeEntry("ForegroundVisited", m_colorSchemes[i].foreground(KColorScheme::VisitedText).color());
        group.writeEntry("ForegroundNegative", m_colorSchemes[i].foreground(KColorScheme::NegativeText).color());
        group.writeEntry("ForegroundNeutral", m_colorSchemes[i].foreground(KColorScheme::NeutralText).color());
        group.writeEntry("ForegroundPositive", m_colorSchemes[i].foreground(KColorScheme::PositiveText).color());
        group.writeEntry("DecorationFocus", m_colorSchemes[i].decoration(KColorScheme::FocusColor).color());
        group.writeEntry("DecorationHover", m_colorSchemes[i].decoration(KColorScheme::HoverColor).color());
    }

    KConfigGroup WMGroup(m_config, "WM");
    WMGroup.writeEntry("activeBackground", m_wmColors.color(WindecoColors::ActiveBackground));
    WMGroup.writeEntry("activeForeground", m_wmColors.color(WindecoColors::ActiveForeground));
    WMGroup.writeEntry("inactiveBackground", m_wmColors.color(WindecoColors::InactiveBackground));
    WMGroup.writeEntry("inactiveForeground", m_wmColors.color(WindecoColors::InactiveForeground));

    KConfigGroup KDEgroup(m_config, "KDE");
    KDEgroup.writeEntry("contrast", contrastSlider->value());

    KConfigGroup generalGroup(m_config, "General");
    generalGroup.writeEntry("shadeSortColumn", (bool)shadeSortedColumn->checkState());
}

void KColorCm::setupColorTable()
{
    // first setup the common colors table
    commonColorTable->verticalHeader()->hide();
    commonColorTable->horizontalHeader()->hide();
    commonColorTable->setShowGrid(false);
    commonColorTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    int minWidth = QPushButton(i18n("Varies")).minimumSizeHint().width();
    commonColorTable->horizontalHeader()->setMinimumSectionSize(minWidth);
    commonColorTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    for (int i = 0; i < 24; ++i)
    {
        KColorButton * button = new KColorButton(this);
        button->setObjectName(QString::number(i));
        connect(button, SIGNAL(changed(const QColor &)), this, SLOT(colorChanged(const QColor &)));
        m_commonColorButtons << button;

        if (i > 8 && i < 18)
        {
            // Inactive Text row through Positive Text role all need a varies button
            KPushButton * variesButton = new KPushButton(NULL);
            variesButton->setText(i18n("Varies"));
            variesButton->setObjectName(QString::number(i));
            connect(variesButton, SIGNAL(clicked()), this, SLOT(variesClicked()));

            QStackedWidget * widget = new QStackedWidget(this);
            widget->addWidget(button);
            widget->addWidget(variesButton);
            m_stackedWidgets.append(widget);

            commonColorTable->setCellWidget(i, 1, widget);
        }
        else
        {
            commonColorTable->setCellWidget(i, 1, button);
        }
    }

    // then the colorTable that the colorSets will use
    colorTable->verticalHeader()->hide();
    colorTable->horizontalHeader()->hide();
    colorTable->setShowGrid(false);
    colorTable->setRowCount(12);
    colorTable->horizontalHeader()->setMinimumSectionSize(minWidth);
    colorTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    createColorEntry(i18n("Normal Background"),    "BackgroundNormal",    m_backgroundButtons, 0);
    createColorEntry(i18n("Alternate Background"), "BackgroundAlternate", m_backgroundButtons, 1);
    createColorEntry(i18n("Normal Text"),          "ForegroundNormal",    m_foregroundButtons, 2);
    createColorEntry(i18n("Inactive Text"),        "ForegroundInactive",  m_foregroundButtons, 3);
    createColorEntry(i18n("Active Text"),          "ForegroundActive",    m_foregroundButtons, 4);
    createColorEntry(i18n("Link Text"),            "ForegroundLink",      m_foregroundButtons, 5);
    createColorEntry(i18n("Visited Text"),         "ForegroundVisited",   m_foregroundButtons, 6);
    createColorEntry(i18n("Negative Text"),        "ForegroundNegative",  m_foregroundButtons, 7);
    createColorEntry(i18n("Neutral Text"),         "ForegroundNeutral",   m_foregroundButtons, 8);
    createColorEntry(i18n("Positive Text"),        "ForegroundPositive",  m_foregroundButtons, 9);
    createColorEntry(i18n("Focus Decoration"),     "DecorationFocus",     m_decorationButtons, 10);
    createColorEntry(i18n("Hover Decoration"),     "DecorationHover",     m_decorationButtons, 11);

    colorTable->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
    colorTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);

    updateColorSchemes();
    updateColorTable();
}

void KColorCm::setCommonForeground(KColorScheme::ForegroundRole role, int stackIndex,
                                   int buttonIndex)
{
    QColor color = m_colorSchemes[KColorScheme::View].foreground(role).color();
    for (int i = KColorScheme::Window; i < KColorScheme::Tooltip; ++i)
    {
        if (i == KColorScheme::Selection && role == KColorScheme::InactiveText)
            break;

        if (m_colorSchemes[i].foreground(role).color() != color)
        {
            m_stackedWidgets[stackIndex]->setCurrentIndex(1);
            return;
        }
    }

    m_stackedWidgets[stackIndex]->setCurrentIndex(0);
    m_commonColorButtons[buttonIndex]->setColor(color);
}

void KColorCm::setCommonDecoration(KColorScheme::DecorationRole role, int stackIndex,
                                   int buttonIndex)
{
    QColor color = m_colorSchemes[KColorScheme::View].decoration(role).color();
    for (int i = KColorScheme::Window; i < KColorScheme::Tooltip; ++i)
    {
        if (m_colorSchemes[i].decoration(role).color() != color)
        {
            m_stackedWidgets[stackIndex]->setCurrentIndex(1);
            return;
        }
    }

    m_stackedWidgets[stackIndex]->setCurrentIndex(0);
    m_commonColorButtons[buttonIndex]->setColor(color);
}

void KColorCm::updateColorTable()
{
    // subtract one here since the 0 item  is "Common Colors"
    int currentSet = colorSet->currentIndex() - 1;

    if (currentSet == -1)
    {
        // common colors is selected
        stackedWidget->setCurrentIndex(0);

        KColorButton * button;
        foreach (button, m_commonColorButtons)
        {
            button->blockSignals(true);
        }

        m_commonColorButtons[0]->setColor(m_colorSchemes[KColorScheme::View].background(KColorScheme::NormalBackground).color());
        m_commonColorButtons[1]->setColor(m_colorSchemes[KColorScheme::View].foreground(KColorScheme::NormalText).color());
        m_commonColorButtons[2]->setColor(m_colorSchemes[KColorScheme::Window].background(KColorScheme::NormalBackground).color());
        m_commonColorButtons[3]->setColor(m_colorSchemes[KColorScheme::Window].foreground(KColorScheme::NormalText).color());
        m_commonColorButtons[4]->setColor(m_colorSchemes[KColorScheme::Button].background(KColorScheme::NormalBackground).color());
        m_commonColorButtons[5]->setColor(m_colorSchemes[KColorScheme::Button].foreground(KColorScheme::NormalText).color());
        m_commonColorButtons[6]->setColor(m_colorSchemes[KColorScheme::Selection].background(KColorScheme::NormalBackground).color());
        m_commonColorButtons[7]->setColor(m_colorSchemes[KColorScheme::Selection].foreground(KColorScheme::NormalText).color());
        m_commonColorButtons[8]->setColor(m_colorSchemes[KColorScheme::Selection].foreground(KColorScheme::InactiveText).color());

        setCommonForeground(KColorScheme::InactiveText, 0,  9);
        setCommonForeground(KColorScheme::ActiveText,   1, 10);
        setCommonForeground(KColorScheme::LinkText,     2, 11);
        setCommonForeground(KColorScheme::VisitedText,  3, 12);
        setCommonForeground(KColorScheme::NegativeText, 4, 13);
        setCommonForeground(KColorScheme::NeutralText,  5, 14);
        setCommonForeground(KColorScheme::PositiveText, 6, 15);

        setCommonDecoration(KColorScheme::FocusColor, 7, 16);
        setCommonDecoration(KColorScheme::HoverColor, 8, 17);

        m_commonColorButtons[18]->setColor(m_colorSchemes[KColorScheme::Tooltip].background(KColorScheme::NormalBackground).color());
        m_commonColorButtons[19]->setColor(m_colorSchemes[KColorScheme::Tooltip].foreground(KColorScheme::NormalText).color());

        m_commonColorButtons[20]->setColor(m_wmColors.color(WindecoColors::ActiveBackground));
        m_commonColorButtons[21]->setColor(m_wmColors.color(WindecoColors::ActiveForeground));
        m_commonColorButtons[22]->setColor(m_wmColors.color(WindecoColors::InactiveBackground));
        m_commonColorButtons[23]->setColor(m_wmColors.color(WindecoColors::InactiveForeground));

        foreach (button, m_commonColorButtons)
        {
            button->blockSignals(false);
        }
    }
    else
    {
        // a real color set is selected
        stackedWidget->setCurrentIndex(1);

        for (int i = KColorScheme::NormalBackground; i <= KColorScheme::AlternateBackground; ++i)
        {
            m_backgroundButtons[i]->blockSignals(true);
            m_backgroundButtons[i]->setColor(m_colorSchemes[currentSet].background(KColorScheme::BackgroundRole(i)).color());
            m_backgroundButtons[i]->blockSignals(false);
        }

        for (int i = KColorScheme::NormalText; i <= KColorScheme::PositiveText; ++i)
        {
            m_foregroundButtons[i]->blockSignals(true);
            m_foregroundButtons[i]->setColor(m_colorSchemes[currentSet].foreground(KColorScheme::ForegroundRole(i)).color());
            m_foregroundButtons[i]->blockSignals(false);
        }

        for (int i = KColorScheme::FocusColor; i <= KColorScheme::HoverColor; ++i)
        {
            m_decorationButtons[i]->blockSignals(true);
            m_decorationButtons[i]->setColor(m_colorSchemes[currentSet].decoration(KColorScheme::DecorationRole(i)).color());
            m_decorationButtons[i]->blockSignals(false);
        }
    }
}

void KColorCm::colorChanged( const QColor &newColor )
{
    // find which button was changed
    int row = sender()->objectName().toInt();
    changeColor(row, newColor);
}

void KColorCm::changeColor(int row, const QColor &newColor)
{
    // update the m_colorSchemes for the selected colorSet
    int currentSet = colorSet->currentIndex() - 1;

    if (currentSet == -1)
    {
        // common colors is selected
        switch (row)
        {
            case 0:
                // View Background button
                KConfigGroup(m_config, "Colors:View").writeEntry("BackgroundNormal", newColor);
                break;
            case 1:
                // View Text button
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundNormal", newColor);
                break;
            case 2:
                // Window Background Button
                KConfigGroup(m_config, "Colors:Window").writeEntry("BackgroundNormal", newColor);
                break;
            case 3:
                // Window Text Button
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundNormal", newColor);
                break;
            case 4:
                // Button Background button
                KConfigGroup(m_config, "Colors:Button").writeEntry("BackgroundNormal", newColor);
                break;
            case 5:
                // Button Text button
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundNormal", newColor);
                break;
            case 6:
                // Selection Background Button
                KConfigGroup(m_config, "Colors:Selection").writeEntry("BackgroundNormal", newColor);
                break;
            case 7:
                // Selection Text Button
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundNormal", newColor);
                break;
            case 8:
                // Selection Inactive Text Button
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundInactive", newColor);
                break;

            // buttons that could have varies in their place
            case 9:
                // Inactive Text Button (set all but Selection Inactive Text color)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundInactive", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundInactive", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundInactive", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundInactive", newColor);
                break;
            case 10:
                // Active Text Button (set all active text colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundActive", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundActive", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundActive", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundActive", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundActive", newColor);
                break;
            case 11:
                // Link Text Button (set all link text colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundLink", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundLink", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundLink", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundLink", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundLink", newColor);
                break;
            case 12:
                // Visited Text Button (set all visited text colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundVisited", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundVisited", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundVisited", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundVisited", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundVisited", newColor);
                break;
            case 13:
                // Negative Text Button (set all negative text colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundNegative", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundNegative", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundNegative", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundNegative", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundNegative", newColor);
                break;
            case 14:
                // Neutral Text Button (set all neutral text colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundNeutral", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundNeutral", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundNeutral", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundNeutral", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundNeutral", newColor);
                break;
            case 15:
                // Positive Text Button (set all positive text colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("ForegroundPositive", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("ForegroundPositive", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("ForegroundPositive", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("ForegroundPositive", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundPositive", newColor);
                break;

            case 16:
                // Focus Decoration Button (set all focus decoration colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("DecorationFocus", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("DecorationFocus", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("DecorationFocus", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("DecorationFocus", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("DecorationFocus", newColor);
                break;
            case 17:
                // Hover Decoration Button (set all hover decoration colors)
                KConfigGroup(m_config, "Colors:View").writeEntry("DecorationHover", newColor);
                KConfigGroup(m_config, "Colors:Window").writeEntry("DecorationHover", newColor);
                KConfigGroup(m_config, "Colors:Selection").writeEntry("DecorationHover", newColor);
                KConfigGroup(m_config, "Colors:Button").writeEntry("DecorationHover", newColor);
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("DecorationHover", newColor);
                break;

            case 18:
                // Tooltip Background button
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("BackgroundNormal", newColor);
                break;
            case 19:
                // Tooltip Text button
                KConfigGroup(m_config, "Colors:Tooltip").writeEntry("ForegroundNormal", newColor);
                break;
            case 20:
                // Active Window
                KConfigGroup(m_config, "WM").writeEntry("activeBackground", newColor);
                break;
            case 21:
                // Active Window Text
                KConfigGroup(m_config, "WM").writeEntry("activeForeground", newColor);
                break;
            case 22:
                // Inactive Window
                KConfigGroup(m_config, "WM").writeEntry("inactiveBackground", newColor);
                break;
            case 23:
                // Inactive Window Text
                KConfigGroup(m_config, "WM").writeEntry("inactiveForeground", newColor);
                break;
        }
        m_commonColorButtons[row]->blockSignals(true);
        m_commonColorButtons[row]->setColor(newColor);
        m_commonColorButtons[row]->blockSignals(false);
    }
    else
    {
        QString group = colorSetGroupKey(currentSet);
        KConfigGroup(m_config, group).writeEntry(m_colorKeys[row], newColor);
    }

    updateColorSchemes();
    updatePreviews();

    emit changed(true);
}

QString KColorCm::colorSetGroupKey(int colorSet)
{
    QString group;
    switch (colorSet) {
        case KColorScheme::Window:
            group = "Colors:Window";
            break;
        case KColorScheme::Button:
            group = "Colors:Button";
            break;
        case KColorScheme::Selection:
            group = "Colors:Selection";
            break;
        case KColorScheme::Tooltip:
            group = "Colors:Tooltip";
            break;
        default:
            group = "Colors:View";
    }
    return group;
}

void KColorCm::on_contrastSlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "KDE");
    group.writeEntry("contrast", value);

    updatePreviews();

    emit changed(true);
}

void KColorCm::on_shadeSortedColumn_stateChanged(int state)
{
    KConfigGroup group(m_config, "General");
    group.writeEntry("shadeSortColumn", (bool)state);

    emit changed(true);
}

void KColorCm::load()
{
    // clean the config, in case we have changed the in-memory kconfig
    m_config->markAsClean();
    m_config->reparseConfiguration();

    // update the color table
    updateColorTable();

    // fill in the color scheme list
    populateSchemeList();

    contrastSlider->setValue(KGlobalSettings::contrast());
    shadeSortedColumn->setCheckState(KGlobalSettings::shadeSortColumn() ?
        Qt::Checked : Qt::Unchecked);

    updateEffectsPage();

    updatePreviews();

    emit changed(false);
}

void KColorCm::save()
{
    m_config->sync();
    KGlobalSettings::self()->emitChange(KGlobalSettings::PaletteChanged);
#ifdef Q_WS_X11
    // Send signal to all kwin instances
    QDBusMessage message =
       QDBusMessage::createSignal("/KWin", "org.kde.KWin", "reloadConfig");
    QDBusConnection::sessionBus().send(message);
#endif

    if (applyToAlien->isChecked())
    {
        runRdb(KRdbExportQtColors | KRdbExportColors);
    }

    emit changed(false);
}

void KColorCm::emitChanged()
{
    emit changed(true);
}

// inactive effects slots
void KColorCm::on_inactiveIntensityBox_currentIndexChanged(int index)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("IntensityEffect", index);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    // disable/enable slider as necessary
    if (index == 0)
    {
        inactiveIntensitySlider->setDisabled(true);
    }
    else
    {
        // update based on slider value since it's enabled now
        inactiveIntensitySlider->setDisabled(false);
        on_inactiveIntensitySlider_valueChanged(inactiveIntensitySlider->value());
    }

    emit changed(true);
}

void KColorCm::on_inactiveIntensitySlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("IntensityAmount", qreal(value - 20) * 0.05);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    emit changed(true);
}

void KColorCm::on_inactiveColorBox_currentIndexChanged(int index)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("ColorEffect", index);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    // disable/enable slider as necessary
    if (index == 0)
    {
        inactiveColorSlider->setDisabled(true);
    }
    else
    {
        // update based on slider value since it's enabled now
        inactiveColorSlider->setDisabled(false);
        on_inactiveColorSlider_valueChanged(inactiveColorSlider->value());
    }
    inactiveColorButton->setDisabled(index == 0 || index == 1);

    emit changed(true);
}

void KColorCm::on_inactiveColorSlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("ColorAmount", qreal(value - 20) * 0.05);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    emit changed(true);
}

void KColorCm::on_inactiveColorButton_changed(const QColor& color)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("Color", color);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    emit changed(true);
}

void KColorCm::on_inactiveContrastBox_currentIndexChanged(int index)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("ContrastEffect", index);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    // disable/enable slider as necessary
    if (index == 0)
    {
        inactiveContrastSlider->setDisabled(true);
    }
    else
    {
        // update based on slider value since it's enabled now
        inactiveContrastSlider->setDisabled(false);
        on_inactiveContrastSlider_valueChanged(inactiveContrastSlider->value());
    }

    emit changed(true);
}

void KColorCm::on_inactiveContrastSlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "ColorEffects:Inactive");
    group.writeEntry("ContrastAmount", qreal(value) * 0.05);
    inactivePreview->setPalette(m_config, QPalette::Inactive);

    emit changed(true);
}

// disabled effects slots
void KColorCm::on_disabledIntensityBox_currentIndexChanged(int index)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("IntensityEffect", index);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    // disable/enable slider as necessary
    if (index == 0)
    {
        disabledIntensitySlider->setDisabled(true);
    }
    else
    {
        // update based on slider value since it's enabled now
        disabledIntensitySlider->setDisabled(false);
        on_disabledIntensitySlider_valueChanged(disabledIntensitySlider->value());
    }

    emit changed(true);
}

void KColorCm::on_disabledIntensitySlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("IntensityAmount", qreal(value - 20) * 0.05);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    emit changed(true);
}

void KColorCm::on_disabledColorBox_currentIndexChanged(int index)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("ColorEffect", index);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    // disable/enable slider as necessary
    if (index == 0)
    {
        disabledColorSlider->setDisabled(true);
    }
    else
    {
        // update based on slider value since it's enabled now
        disabledColorSlider->setDisabled(false);
        on_disabledColorSlider_valueChanged(disabledColorSlider->value());
    }
    disabledColorButton->setDisabled(index == 0 || index == 1);

    emit changed(true);
}

void KColorCm::on_disabledColorSlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("ColorAmount", qreal(value - 20) * 0.05);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    emit changed(true);
}

void KColorCm::on_disabledColorButton_changed(const QColor& color)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("Color", color);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    emit changed(true);
}

void KColorCm::on_disabledContrastBox_currentIndexChanged(int index)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("ContrastEffect", index);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    // disable/enable slider as necessary
    if (index == 0)
    {
        disabledContrastSlider->setDisabled(true);
    }
    else
    {
        // update based on slider value since it's enabled now
        disabledContrastSlider->setDisabled(false);
        on_disabledContrastSlider_valueChanged(disabledContrastSlider->value());
    }

    emit changed(true);
}

void KColorCm::on_disabledContrastSlider_valueChanged(int value)
{
    KConfigGroup group(m_config, "ColorEffects:Disabled");
    group.writeEntry("ContrastAmount", qreal(value) * 0.05);
    disabledPreview->setPalette(m_config, QPalette::Disabled);

    emit changed(true);
}

#include "colorscm.moc"
