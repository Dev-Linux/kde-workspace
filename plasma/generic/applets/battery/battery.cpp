/***************************************************************************
 *   Copyright 2007-2008 by Riccardo Iaconelli <riccardo@kde.org>          *
 *   Copyright 2007-2009 by Sebastian Kuegler <sebas@kde.org>              *
 *   Copyright 2007 by Luka Renko <lure@kubuntu.org>                       *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "battery.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QFont>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsGridLayout>
#include <QGraphicsLinearLayout>
#include <QDBusPendingCall>
#include <QLabel>
#include <QPropertyAnimation>

#include <KDebug>
#include <KIcon>
#include <KSharedConfig>
#include <KToolInvocation>
#include <KColorScheme>
#include <KConfigDialog>
#include <KGlobalSettings>
#include <KPushButton>

#include <kworkspace/kworkspace.h>

#include <solid/control/powermanager.h>
#include <solid/powermanagement.h>

#include <Plasma/Svg>
#include <Plasma/Theme>
#include <Plasma/Animator>
#include <Plasma/Extender>
#include <Plasma/ExtenderItem>
#include <Plasma/PopupApplet>
#include <Plasma/Label>
#include <Plasma/Separator>
#include <Plasma/Slider>
#include <Plasma/PushButton>
#include <Plasma/CheckBox>
#include <Plasma/ComboBox>
#include <Plasma/IconWidget>


Battery::Battery(QObject *parent, const QVariantList &args)
    : Plasma::PopupApplet(parent, args),
      m_isEmbedded(false),
      m_extenderVisible(false),
      m_controls(0),
      m_controlsLayout(0),
      m_batteryLabelLabel(0),
      m_batteryInfoLabel(0),
      m_acLabelLabel(0),
      m_acInfoLabel(0),
      m_profileLabel(0),
      m_profileCombo(0),
      m_brightnessSlider(0),
      m_minutes(0),
      m_hours(0),
      m_theme(0),
      m_availableProfiles(QStringList()),
      m_numOfBattery(0),
      m_acAdapterPlugged(false),
      m_remainingMSecs(0),
      m_labelAlpha(0),
      m_labelAnimation(0),
      m_acAlpha(0),
      m_acAnimation(0)
{
    //kDebug() << "Loading applet battery";
    setAcceptsHoverEvents(true);
    setPopupIcon(QIcon());
    resize(128, 128);
    setAspectRatioMode(Plasma::ConstrainedSquare );
    m_textRect = QRectF();
    m_remainingMSecs = 0;
    m_extenderApplet = 0;
    m_theme = new Plasma::Svg(this);
    m_theme->setImagePath("widgets/battery-oxygen");
    m_theme->setContainsMultipleImages(true);
    setStatus(Plasma::ActiveStatus);
    //setPassivePopup(true);

    m_labelAnimation = new QPropertyAnimation(this, "labelAlpha");
    m_labelAnimation->setDuration(200);
    m_labelAnimation->setStartValue((qreal)(0.0));
    m_labelAnimation->setEndValue((qreal)(1.0));
    m_labelAnimation->setEasingCurve(QEasingCurve::OutQuad);
    connect(m_labelAnimation, SIGNAL(finished()), this, SLOT(updateBattery()));
    m_acAnimation = new QPropertyAnimation(this, "acAlpha");
    m_acAnimation->setDuration(500);
    m_acAnimation->setStartValue((qreal)(0.0));
    m_acAnimation->setEndValue((qreal)(1.0));
    m_acAnimation->setEasingCurve(QEasingCurve::OutBack);
    connect(m_acAnimation, SIGNAL(finished()), this, SLOT(updateBattery()));
}

void Battery::init()
{
    setHasConfigurationInterface(true);

    // read config
    configChanged();
    
    m_theme->resize(contentsRect().size());
    m_font = QApplication::font();
    m_font.setWeight(QFont::Bold);

    m_boxAlpha = 128;
    m_boxHoverAlpha = 192;

    readColors();
    connect(Plasma::Theme::defaultTheme(), SIGNAL(themeChanged()), SLOT(readColors()));
    connect(KGlobalSettings::self(), SIGNAL(kdisplayPaletteChanged()), SLOT(readColors()));
    connect(KGlobalSettings::self(), SIGNAL(appearanceChanged()), SLOT(setupFonts()));
    connect(KGlobalSettings::self(), SIGNAL(appearanceChanged()), SLOT(setupFonts()));

    const QStringList& battery_sources = dataEngine("powermanagement")->query("Battery")["sources"].toStringList();

    connectSources();

    foreach (const QString &battery_source, battery_sources) {
        dataUpdated(battery_source, dataEngine("powermanagement")->query(battery_source));
    }
    m_numOfBattery = battery_sources.size();

    dataUpdated("AC Adapter", dataEngine("powermanagement")->query("AC Adapter"));

    if (!m_isEmbedded && extender() && !extender()->hasItem("powermanagement")) {
        Plasma::ExtenderItem *eItem = new Plasma::ExtenderItem(extender());
        eItem->setName("powermanagement");
        initExtenderItem(eItem);
        extender()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    
    if (m_acAdapterPlugged) {
        showAcAdapter(true);
    }

}

void Battery::configChanged()
{
    KConfigGroup cg = config();
    m_showBatteryString = cg.readEntry("showBatteryString", false);
    m_showRemainingTime = cg.readEntry("showRemainingTime", false);
    m_showMultipleBatteries = cg.readEntry("showMultipleBatteries", false);
    
    if (m_showBatteryString) {
        showLabel(true);
    }
    
}

void Battery::updateBattery()
{
    update();
}

void Battery::constraintsEvent(Plasma::Constraints constraints)
{
    //kDebug() << "ConstraintsEvent, Dude." << contentsRect();
    if (!m_showMultipleBatteries || m_numOfBattery < 2) {
        setAspectRatioMode(Plasma::Square);
    } else {
        setAspectRatioMode(Plasma::KeepAspectRatio);
    }
    int minWidth;
    int minHeight;

    if (constraints & Plasma::FormFactorConstraint) {
        if (formFactor() == Plasma::Vertical ||
            formFactor() == Plasma::Horizontal) {
            m_theme->setImagePath("icons/battery");
        } else {
            m_theme->setImagePath("widgets/battery-oxygen");
        }
    }

    if (constraints & (Plasma::FormFactorConstraint | Plasma::SizeConstraint)) {
        if (formFactor() == Plasma::Vertical) {
            if (!m_showMultipleBatteries) {
                minHeight = qMax(m_textRect.height(), size().width());
            } else {
                minHeight = qMax(m_textRect.height(), size().width()*m_numOfBattery);
            }
            setMinimumWidth(0);
            setMinimumHeight(minHeight);
            //kDebug() << "Vertical FormFactor";
        } else if (formFactor() == Plasma::Horizontal) {
            if (!m_showMultipleBatteries) {
                minWidth = qMax(m_textRect.width(), size().height());
            } else {
                minWidth = qMax(m_textRect.width(), size().height()*m_numOfBattery);
            }
            setMinimumWidth(minWidth);
            setMinimumHeight(0);
            //kDebug() << "Horizontal FormFactor" << m_textRect.width() << contentsRect().height();
        } else {
            setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

            if (m_showMultipleBatteries) {
                setMinimumSize(KIconLoader::SizeSmall * m_numOfBattery, KIconLoader::SizeSmall);
            } else {
                setMinimumSize(KIconLoader::SizeSmall, KIconLoader::SizeSmall);
            }
        }

        QSize c(contentsRect().size().toSize());
        if (m_showMultipleBatteries) {
            if (formFactor() == Plasma::Vertical) {
                c.setHeight(size().height() / qMax(1, m_numOfBattery));
            } else if (formFactor() == Plasma::Horizontal) {
                c.setWidth(size().width() / qMax(1, m_numOfBattery));
            }
        }
        m_theme->resize(c);

        m_font.setPointSize(qMax(KGlobalSettings::smallestReadableFont().pointSize(),
                                 qRound(contentsRect().height() / 10)));
        update();
    }
}

void Battery::dataUpdated(const QString& source, const Plasma::DataEngine::Data &data)
{
    if (source.startsWith(QLatin1String("Battery"))) {
        m_batteries_data[source] = data;
        //kDebug() << "new battery source" << source;
    } else if (source == "AC Adapter") {
        m_acAdapterPlugged = data["Plugged in"].toBool();
        showAcAdapter(m_acAdapterPlugged);
    } else if (source == "PowerDevil") {
        m_availableProfiles = data["availableProfiles"].toStringList();
        m_currentProfile = data["currentProfile"].toString();
        //kDebug() << "PowerDevil profiles:" << m_availableProfiles << "[" << m_currentProfile << "]";
    } else {
        kDebug() << "Applet::Dunno what to do with " << source;
    }
    if (source == "Battery0") {
        m_remainingMSecs  = data["Remaining msec"].toInt();
        //kDebug() << "Remaining msecs on battery:" << m_remainingMSecs;
    }

    if (m_numOfBattery == 0) {
        setStatus(Plasma::PassiveStatus);
    } else if (m_batteries_data.count() > 0 && data["Percent"].toInt() < 10) {
        setStatus(Plasma::NeedsAttentionStatus);
    } else {
        setStatus(Plasma::ActiveStatus);
    }

    updateStatus();
    update();
}

void Battery::createConfigurationInterface(KConfigDialog *parent)
{
    QWidget *widget = new QWidget(parent);
    ui.setupUi(widget);
    parent->addPage(widget, i18n("General"), Applet::icon());
    connect(parent, SIGNAL(applyClicked()), this, SLOT(configAccepted()));
    connect(parent, SIGNAL(okClicked()), this, SLOT(configAccepted()));
    ui.showBatteryStringCheckBox->setChecked(m_showBatteryString ? Qt::Checked : Qt::Unchecked);
    ui.showMultipleBatteriesCheckBox->setChecked(m_showMultipleBatteries ? Qt::Checked : Qt::Unchecked);
}

void Battery::configAccepted()
{
    KConfigGroup cg = config();

    if (m_showBatteryString != ui.showBatteryStringCheckBox->isChecked()) {
        m_showBatteryString = !m_showBatteryString;
        cg.writeEntry("showBatteryString", m_showBatteryString);
        showLabel(m_showBatteryString);
    }

    if (m_showMultipleBatteries != ui.showMultipleBatteriesCheckBox->isChecked()) {
        m_showMultipleBatteries = !m_showMultipleBatteries;
        cg.writeEntry("showMultipleBatteries", m_showMultipleBatteries);
        //kDebug() << "Show multiple battery changed: " << m_showMultipleBatteries;
        emit sizeHintChanged(Qt::PreferredSize);
    }

    emit configNeedsSaving();
}

void Battery::readColors()
{
    m_textColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::TextColor);
    m_boxColor = Plasma::Theme::defaultTheme()->color(Plasma::Theme::BackgroundColor);
}

void Battery::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (!m_showBatteryString) {
        showLabel(true);
    }
    //showAcAdapter(true);
    Applet::hoverEnterEvent(event);
}

void Battery::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    if (!m_showBatteryString && !m_isEmbedded) {
        showLabel(false);
    }
    //showAcAdapter(false);
    Applet::hoverLeaveEvent(event);
}

Battery::~Battery()
{
}

void Battery::suspend()
{
    hidePopup();
    QDBusConnection dbus(QDBusConnection::sessionBus());
    QDBusInterface iface("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", dbus);
    iface.asyncCall("suspend", Solid::Control::PowerManager::ToRam);
}

void Battery::hibernate()
{
    hidePopup();
    QDBusConnection dbus(QDBusConnection::sessionBus());
    QDBusInterface iface("org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", dbus);
    iface.asyncCall("suspend", Solid::Control::PowerManager::ToDisk);
}

void Battery::brightnessChanged(const int brightness)
{
    Solid::Control::PowerManager::setBrightness(brightness);
}

void Battery::updateSlider(const float brightness)
{
    if (m_brightnessSlider->value() != (int)brightness) {
        m_brightnessSlider->setValue((int) brightness);
    }
}

void Battery::setFullBrightness()
{
    brightnessChanged(100);
    updateSlider(100);
}

void Battery::setEmbedded(const bool embedded)
{
    m_isEmbedded = embedded;
}

void Battery::initExtenderItem(Plasma::ExtenderItem *item)
{
    // We only show the extender for applets that are not embedded, as
    // that would create infinitve loops, you really don't want an applet
    // extender when the applet is embedded into another applet, such
    // as the battery applet is also embedded into the battery's extender.
    if (!m_isEmbedded && item->name() == "powermanagement") {
        int row = 0;

        m_controls = new QGraphicsWidget(item);
        m_controls->setMinimumWidth(360);
        m_controlsLayout = new QGraphicsGridLayout(m_controls);
        m_controlsLayout->setColumnStretchFactor(1, 3);

        Plasma::Label *spacer0 = new Plasma::Label(m_controls);
        spacer0->setMinimumHeight(8);
        spacer0->setMaximumHeight(8);
        m_controlsLayout->addItem(spacer0, row, 0);
        row++;

        m_batteryLabelLabel = new Plasma::Label(m_controls);
        m_batteryLabelLabel->setAlignment(Qt::AlignRight);
        m_batteryInfoLabel = new Plasma::Label(m_controls);
        m_controlsLayout->addItem(m_batteryLabelLabel, row, 0);
        m_controlsLayout->addItem(m_batteryInfoLabel, row, 1);
        row++;
        m_acLabelLabel = new Plasma::Label(m_controls);
        m_acLabelLabel->setAlignment(Qt::AlignRight);
        m_acInfoLabel = new Plasma::Label(m_controls);
        m_acInfoLabel->nativeWidget()->setWordWrap(false);
        m_controlsLayout->addItem(m_acLabelLabel, row, 0);
        m_controlsLayout->addItem(m_acInfoLabel, row, 1);
        row++;

        m_remainingTimeLabel = new Plasma::Label(m_controls);
        m_remainingTimeLabel->setAlignment(Qt::AlignRight);
        // FIXME: 4.5
        //m_remainingTimeLabel->setText(i18nc("Label for remaining time", "Time Remaining:"));
        m_remainingInfoLabel = new Plasma::Label(m_controls);
        m_remainingInfoLabel->nativeWidget()->setWordWrap(false);
        m_controlsLayout->addItem(m_remainingTimeLabel, row, 0);
        m_controlsLayout->addItem(m_remainingInfoLabel, row, 1);
        row++;

        Battery *m_extenderApplet = static_cast<Battery*>(Plasma::Applet::load("battery"));
        int s = 64;
        if (m_extenderApplet) {
            m_extenderApplet->setParent(m_controls);
            m_extenderApplet->setAcceptsHoverEvents(false);
            m_extenderApplet->setParentItem(m_controls);
            m_extenderApplet->setEmbedded(true);
            m_extenderApplet->resize(s, s);
            m_extenderApplet->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_extenderApplet->setBackgroundHints(NoBackground);
            m_extenderApplet->setFlag(QGraphicsItem::ItemIsMovable, false);
            m_extenderApplet->init();
            m_extenderApplet->showBatteryLabel(false);
            m_extenderApplet->setGeometry(QRectF(QPoint(m_controls->geometry().width()-s, 0), QSizeF(s, s)));
            m_extenderApplet->updateConstraints(Plasma::StartupCompletedConstraint);
        }

        m_brightnessLabel = new Plasma::Label(m_controls);
        m_brightnessLabel->setText(i18n("Screen Brightness"));
        // FIXME: 4.5
        //m_brightnessLabel->setText(i18n("Screen Brightness:"));
        m_brightnessLabel->nativeWidget()->setWordWrap(false);
        m_brightnessLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

        m_controlsLayout->addItem(m_brightnessLabel, row, 0);

        m_brightnessSlider = new Plasma::Slider(m_controls);
        m_brightnessSlider->setRange(0, 100);
        m_brightnessSlider->setValue(Solid::Control::PowerManager::brightness());
        m_brightnessSlider->nativeWidget()->setTickInterval(10);
        m_brightnessSlider->setOrientation(Qt::Horizontal);
        connect(m_brightnessSlider, SIGNAL(valueChanged(int)),
                this, SLOT(brightnessChanged(int)));

        Solid::Control::PowerManager::Notifier *notifier = Solid::Control::PowerManager::notifier();

        connect(notifier, SIGNAL(brightnessChanged(float)),
                this, SLOT(updateSlider(float)));
        m_controlsLayout->addItem(m_brightnessSlider, row, 1);
        row++;

        m_profileLabel = new Plasma::Label(m_controls);
        m_profileLabel->setText(i18n("Power Profile"));
        // FIXME: 4.5
        //m_profileLabel->setText(i18n("Power Profile:"));
        m_profileLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
        m_controlsLayout->addItem(m_profileLabel, row, 0);


        m_profileCombo = new Plasma::ComboBox(m_controls);
        // This is necessary until Qt task #217874 is fixed
        m_profileCombo->setZValue(110);
        connect(m_profileCombo, SIGNAL(activated(QString)),
                this, SLOT(setProfile(QString)));

        m_controlsLayout->addItem(m_profileCombo, row, 1);

        row++;

        QGraphicsWidget *buttonWidget = new QGraphicsWidget(m_controls);
        QGraphicsLinearLayout *buttonLayout = new QGraphicsLinearLayout(buttonWidget);
        buttonLayout->setSpacing(0.0);
        //buttonLayout->addItem(m_profileCombo);

        int buttonsize = KIconLoader::SizeMedium + 4;

        // Sleep and Hibernate buttons
        QSet<Solid::PowerManagement::SleepState> sleepstates = Solid::PowerManagement::supportedSleepStates();
        foreach (const Solid::PowerManagement::SleepState &sleepstate, sleepstates) {
            if (sleepstate == Solid::PowerManagement::StandbyState) {
                // Not interesting at this point ...

            } else if (sleepstate == Solid::PowerManagement::SuspendState) {
                Plasma::IconWidget *suspendButton = new Plasma::IconWidget(m_controls);
                suspendButton->setIcon("system-suspend");
                suspendButton->setText(i18n("Sleep"));
                suspendButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                suspendButton->setOrientation(Qt::Horizontal);
                suspendButton->setMaximumHeight(buttonsize);
                suspendButton->setMinimumHeight(buttonsize);
                suspendButton->setDrawBackground(true);
                suspendButton->setTextBackgroundColor(QColor(Qt::transparent));
                //suspendButton->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
                m_controlsLayout->addItem(suspendButton, row, 0, Qt::AlignRight|Qt::AlignVCenter);
                //row++;
                connect(suspendButton, SIGNAL(clicked()), this, SLOT(suspend()));
            } else if (sleepstate == Solid::PowerManagement::HibernateState) {
                Plasma::IconWidget *hibernateButton = new Plasma::IconWidget(m_controls);
                hibernateButton->setIcon("system-suspend-hibernate");
                hibernateButton->setText(i18n("Hibernate"));
                hibernateButton->setOrientation(Qt::Horizontal);
                hibernateButton->setMaximumHeight(buttonsize);
                hibernateButton->setMinimumHeight(buttonsize);
                hibernateButton->setDrawBackground(true);
                hibernateButton->setTextBackgroundColor(QColor(Qt::transparent));
                hibernateButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
                buttonLayout->addItem(hibernateButton);
                buttonLayout->setAlignment(hibernateButton, Qt::AlignLeft|Qt::AlignVCenter);
                connect(hibernateButton, SIGNAL(clicked()), this, SLOT(hibernate()));
            }
        }
        // Configure button
        Plasma::IconWidget *configButton = new Plasma::IconWidget(buttonWidget);
        configButton->setToolTip(i18nc("tooltip on the config button in the popup", "Configure Power Management..."));
        configButton->setOrientation(Qt::Horizontal);
        configButton->setMaximumHeight(buttonsize);
        configButton->setDrawBackground(false);
        configButton->setMinimumHeight(buttonsize);
        configButton->setMaximumWidth(buttonsize);
        configButton->setMinimumWidth(buttonsize);
        configButton->setDrawBackground(true);
        configButton->setTextBackgroundColor(QColor(Qt::transparent));
        configButton->setIcon("configure");
        connect(configButton, SIGNAL(clicked()), this, SLOT(openConfig()));
        configButton->setEnabled(hasAuthorization("LaunchApp"));

        buttonLayout->addItem(configButton);
        buttonLayout->setItemSpacing(0, 0.0);
        buttonLayout->setItemSpacing(1, 0.0);
        buttonLayout->setAlignment(configButton, Qt::AlignRight|Qt::AlignVCenter);

        buttonWidget->setLayout(buttonLayout);
        m_controlsLayout->addItem(buttonWidget, row, 1);
        m_controls->setLayout(m_controlsLayout);
        item->setWidget(m_controls);
        item->setTitle(i18n("Power Management"));

        setupFonts();
    }
}

void Battery::popupEvent(bool show)
{
    m_extenderVisible = show;
    if (m_extenderApplet) {
        int s = 64;
        m_extenderApplet->setGeometry(QRectF(QPoint(m_controls->geometry().width()-s, 0), QSizeF(s, s)));
    }
    updateStatus();
}

void Battery::setupFonts()
{
    if (m_batteryLabelLabel) {
        QFont infoFont = KGlobalSettings::generalFont();
        m_brightnessLabel->setFont(infoFont);
        m_profileLabel->setFont(infoFont);

        infoFont.setPointSize(infoFont.pointSize()+1);
        QFont boldFont = infoFont;
        boldFont.setBold(true);

        m_batteryLabelLabel->setFont(infoFont);
        m_acLabelLabel->setFont(infoFont);
        m_remainingTimeLabel->setFont(infoFont);

        m_batteryInfoLabel->setFont(boldFont);
        m_acInfoLabel->setFont(boldFont);

        m_remainingInfoLabel->setFont(infoFont);
    }
}
void Battery::updateStatus()
{
    if (!m_extenderVisible) {
        return;
    }

    QString batteriesLabel;
    QString batteriesInfo;
    if (m_numOfBattery && m_batteryLabelLabel) {
        QHashIterator<QString, QHash<QString, QVariant > > battery_data(m_batteries_data);
        int bnum = 0;
        QString state;
        while (battery_data.hasNext()) {
            bnum++;
            battery_data.next();
            state = battery_data.value()["State"].toString();
            if (m_numOfBattery == 1) {
                m_batteryLabelLabel->setText(i18n("Battery:"));
                if (battery_data.value()["Plugged in"].toBool()) {
                    if (state == "NoCharge") {
                        m_batteryInfoLabel->setText(i18n("%1% (charged)", battery_data.value()["Percent"].toString()));
                    } else if (state == "Discharging") {
                        m_batteryInfoLabel->setText(i18nc("Shown when a time estimate is not available", "%1% (discharging)", battery_data.value()["Percent"].toString()));
                    } else {
                        m_batteryInfoLabel->setText(i18n("%1% (charging)", battery_data.value()["Percent"].toString()));
                    }
                } else {
                    m_batteryInfoLabel->setText(i18nc("Battery is not plugged in", "Not present"));
                }
            } else {
                //kDebug() << "More batteries ...";
                if (bnum > 1) {
                    batteriesLabel.append("<br />");
                    batteriesInfo.append("<br />");
                }
                batteriesLabel.append(i18nc("Placeholder is the battery ID", "Battery %1:", bnum));
                if (state == "NoCharge") {
                    batteriesInfo.append(i18n("%1% (charged)", battery_data.value()["Percent"].toString()));
                } else if (state == "Discharging") {
                    batteriesInfo.append(i18n("%1% (discharging)", battery_data.value()["Percent"].toString()));
                } else {
                    batteriesInfo.append(i18n("%1% (charging)", battery_data.value()["Percent"].toString()));
                }
            }
        }
        m_acLabelLabel->setText(i18n("AC Adapter:")); // ouch ...
        if (m_acAdapterPlugged) {
            m_acInfoLabel->setText(i18n("Plugged in "));
        } else {
            m_acInfoLabel->setText(i18n("Not plugged in"));
        }
        if ((state == "Discharging" || state == "Charging") && m_remainingMSecs > 0 && m_showRemainingTime) {
            m_remainingTimeLabel->show();
            m_remainingInfoLabel->show();
            // we don't have too much accuracy so only give hours and minutes
            int remainingMinutes = m_remainingMSecs - (m_remainingMSecs % 60000);
            m_remainingInfoLabel->setText(KGlobal::locale()->prettyFormatDuration(remainingMinutes));
        } else {
            m_remainingTimeLabel->hide();
            m_remainingInfoLabel->hide();
        }
    } else {
        m_batteryLabelLabel->setText(i18n("<b>Battery:</b> "));
        m_batteryInfoLabel->setText(i18nc("Battery is not plugged in", "Not present"));
    }
    if (!batteriesInfo.isEmpty()) {
        m_batteryInfoLabel->setText(batteriesInfo);
        m_batteryLabelLabel->setText(batteriesLabel);
    }
    if (!m_availableProfiles.empty() && m_profileCombo) {
        m_profileCombo->clear();
        m_profileCombo->addItem(m_currentProfile);
        foreach (const QString &p, m_availableProfiles) {
            if (m_currentProfile != p) {
                m_profileCombo->addItem(p);
            }
        }
    }

    if (m_profileLabel && m_profileCombo) {
        if (m_availableProfiles.empty()) {
            m_profileCombo->hide();
            m_profileLabel->hide();
        } else {
            m_profileCombo->show();
            m_profileLabel->show();
        }
    }

    if (m_brightnessSlider) {
        m_brightnessSlider->setValue(Solid::Control::PowerManager::brightness());
    }
}

void Battery::openConfig()
{
    //kDebug() << "opening powermanagement configuration dialog";
    QStringList args;
    args << "powerdevilconfig";
    KToolInvocation::kdeinitExec("kcmshell4", args);
}

void Battery::setProfile(const QString &profile)
{
    if (m_currentProfile != profile) {
        kDebug() << "Changing power profile to " << profile;
        QDBusConnection dbus( QDBusConnection::sessionBus() );
        QDBusInterface iface( "org.kde.kded", "/modules/powerdevil", "org.kde.PowerDevil", dbus );
        iface.call( "setProfile", profile );
    }
}

void Battery::showLabel(bool show)
{
    if (show) {
        m_labelAnimation->setDirection(QAbstractAnimation::Forward);
        if (m_labelAnimation->state() != QAbstractAnimation::Running) {
            m_labelAnimation->start();
        }
    } else {
        m_labelAnimation->setDirection(QAbstractAnimation::Backward);
        if (m_labelAnimation->state() != QAbstractAnimation::Running) {
            m_labelAnimation->start();
        }
    }
}

void Battery::showAcAdapter(bool show)
{
    if (show) {
        m_acAnimation->setDirection(QAbstractAnimation::Forward);
        if (m_acAnimation->state() != QAbstractAnimation::Running) {
            m_acAnimation->start();
        }
    } else {
        m_acAnimation->setDirection(QAbstractAnimation::Backward);
        if (m_acAnimation->state() != QAbstractAnimation::Running) {
            m_acAnimation->start();
        }
    }
}

QRectF Battery::scaleRectF(const qreal progress, QRectF rect)
{
    if (qFuzzyCompare(progress, qreal(1))) {
        return rect;
    }

    // Scale
    qreal w = rect.width() * progress;
    qreal h = rect.width() * progress;

    // Position centered
    rect.setX(rect.x() + (rect.width() - w) / 2);
    rect.setY(rect.y() + (rect.height() - h) / 2);

    rect.setWidth(w);
    rect.setHeight(h);

    return rect;
}

void Battery::paintLabel(QPainter *p, const QRect &contentsRect, const QString& labelText)
{
    // Store font size, we want to restore it shortly
    int original_font_size = m_font.pointSize();

    // Fonts smaller than smallestReadableFont don't make sense.
    m_font.setPointSize(qMax(KGlobalSettings::smallestReadableFont().pointSize(), m_font.pointSize()));
    QFontMetrics fm(m_font);
    qreal text_width = fm.width(labelText);

    // Longer texts get smaller fonts
    if (labelText.length() > 4) {
        if (original_font_size/1.5 < KGlobalSettings::smallestReadableFont().pointSize()) {
            m_font.setPointSize((KGlobalSettings::smallestReadableFont().pointSize()));
        } else {
            m_font.setPointSizeF(original_font_size/1.5);
        }
        fm = QFontMetrics(m_font);
        text_width = (fm.width(labelText) * 1.2);
    } else {
        // Smaller texts get a wider box
        text_width = (text_width * 1.4);
    }
    if (formFactor() == Plasma::Horizontal ||
        formFactor() == Plasma::Vertical) {
        m_font = KGlobalSettings::smallestReadableFont();
        fm = QFontMetrics(m_font);
        text_width = (fm.width(labelText)+8);
    }
    p->setFont(m_font);

    // Let's find a good position for painting the percentage on top of the battery
    m_textRect = QRectF((contentsRect.left() + (contentsRect.width() - text_width) / 2),
                            contentsRect.top() + ((contentsRect.height() - (int)fm.height()) / 2 * 0.9),
                            (int)(text_width),
                            fm.height() * 1.1 );

    // Poor man's highlighting
    m_boxColor.setAlphaF(m_labelAlpha);
    p->setPen(m_boxColor);
    m_boxColor.setAlphaF(m_labelAlpha*0.5);
    p->setBrush(m_boxColor);

    // Find sensible proportions for the rounded corners
    float round_prop = m_textRect.width() / m_textRect.height();

    // Tweak the rounding edge a bit with the proportions of the textbox
    qreal round_radius = 35.0;
    p->drawRoundedRect(m_textRect, round_radius / round_prop, round_radius, Qt::RelativeSize);

    m_textColor.setAlphaF(m_labelAlpha);
    p->setPen(m_textColor);
    p->drawText(m_textRect, Qt::AlignCenter, labelText);

    // Reset font and box
    m_font.setPointSize(original_font_size);
}

void Battery::paintBattery(QPainter *p, const QRect &contentsRect, const int batteryPercent, const bool plugState)
{
    int minSize = qMin(contentsRect.height(), contentsRect.width());
    QRect contentsSquare = QRect(contentsRect.x() + (contentsRect.width() - minSize) / 2, contentsRect.y() + (contentsRect.height() - minSize) / 2, minSize, minSize);
  
    if (m_theme->hasElement("Battery")) {
        m_theme->paint(p, contentsSquare, "Battery");
    }

    QString fill_element;
    if (plugState) {
        if (batteryPercent > 95) {
            fill_element = "Fill100";
        } else if (batteryPercent > 80) {
            fill_element = "Fill80";
        } else if (batteryPercent > 50) {
            fill_element = "Fill60";
        } else if (batteryPercent > 20) {
            fill_element = "Fill40";
        } else if (batteryPercent > 10) {
            fill_element = "Fill20";
        } // Don't show a fillbar below 11% charged
    } else {
        fill_element = "Unavailable";
    }
    //kDebug() << "plugState:" << plugState;

    // Now let's find out which fillstate to show
    if (!fill_element.isEmpty()) {
        if (m_theme->hasElement(fill_element)) {
            m_theme->paint(p, contentsSquare, fill_element);
        } else {
            kDebug() << fill_element << " does not exist in svg";
        }
    }

    m_theme->paint(p, scaleRectF(m_acAlpha, contentsSquare), "AcAdapter");

    if (plugState && m_theme->hasElement("Overlay")) {
        m_theme->paint(p, contentsSquare, "Overlay");
    }
}

void Battery::paintInterface(QPainter *p, const QStyleOptionGraphicsItem *option, const QRect &contentsRect)
{
    Q_UNUSED( option );

    p->setRenderHint(QPainter::SmoothPixmapTransform);
    p->setRenderHint(QPainter::Antialiasing);

    if (m_numOfBattery == 0) {
        QRectF ac_contentsRect(contentsRect.topLeft(), QSizeF(qMax(qreal(0.0), contentsRect.width() * m_acAlpha), qMax(qreal(0.0), contentsRect.height() * m_acAlpha)));
        if (m_acAdapterPlugged) {
            m_theme->paint(p, ac_contentsRect, "AcAdapter");
        }
        paintBattery(p, contentsRect, 0, false);
        return;
    }

    if (m_isEmbedded || m_showMultipleBatteries) {
        // paint each battery with own charge level
        int battery_num = 0;
        int height = contentsRect.height();
        int width = contentsRect.width();
        int xdelta = 0;
        int ydelta = 0;
        if (m_showMultipleBatteries) {
            if (formFactor() == Plasma::Vertical) {
                height = height / m_numOfBattery;
                ydelta = height;
            } else {
                width = width / m_numOfBattery;
                xdelta = width;
            }
        }
        QHashIterator<QString, QHash<QString, QVariant > > battery_data(m_batteries_data);
        while (battery_data.hasNext()) {
            battery_data.next();
            QRect corect = QRect(contentsRect.left() + battery_num * xdelta,
                                 contentsRect.top() + battery_num * ydelta,
                                 width, height);
            // paint battery with appropriate charge level
            paintBattery(p, corect, battery_data.value()["Percent"].toInt(), battery_data.value()["Plugged in"].toBool());

            // Show the charge percentage with a box on top of the battery
            QString batteryLabel;
            if (battery_data.value()["Plugged in"].toBool()) {
                int hours = m_remainingMSecs/1000/3600;
                int minutes = qRound(m_remainingMSecs/60000) % 60;
                if (!(minutes==0 && hours==0)) {
                    m_minutes= minutes;
                    m_hours= hours;
                }
                QString state = battery_data.value()["State"].toString();

                m_remainingMSecs = battery_data.value()["Remaining msec"].toInt();
                if (m_remainingMSecs > 0 && (m_showRemainingTime && (state=="Charging" || state=="Discharging"))) {
                    QTime t = QTime(m_hours, m_minutes);
                    KLocale tmpLocale(*KGlobal::locale());
                    tmpLocale.setTimeFormat("%k:%M");
                    batteryLabel = tmpLocale.formatTime(t, false, true); // minutes, hours as duration
                } else {
                    batteryLabel = i18nc("overlay on the battery, needs to be really tiny", "%1%", battery_data.value()["Percent"].toString());
                }
                paintLabel(p, corect, batteryLabel);
            }
            ++battery_num;
        }
    } else {
        // paint only one battery and show cumulative charge level
        int battery_num = 0;
        int battery_charge = 0;
        bool has_battery = false;
        QHashIterator<QString, QHash<QString, QVariant > > battery_data(m_batteries_data);
        while (battery_data.hasNext()) {
            battery_data.next();
            if (battery_data.value()["Plugged in"].toBool()) {
                battery_charge += battery_data.value()["Percent"].toInt();
                has_battery = true;
                ++battery_num;
            }
        }
        if (battery_num > 0) {
            battery_charge = battery_charge / battery_num;
        }
        // paint battery with appropriate charge level
        paintBattery(p, contentsRect,  battery_charge, has_battery);
        // Show the charge percentage with a box on top of the battery
        QString batteryLabel;
        if (has_battery) {
            batteryLabel = i18nc("overlay on the battery, needs to be really tiny", "%1%", battery_charge);
            paintLabel(p, contentsRect, batteryLabel);
        }
    }
}

void Battery::showBatteryLabel(bool show)
{
    if (show != m_showBatteryString) {
        showLabel(show);
        m_showBatteryString = show;
    }
}

void Battery::connectSources()
{
    const QStringList& battery_sources = dataEngine("powermanagement")->query("Battery")["sources"].toStringList();

    foreach (const QString &battery_source, battery_sources) {
        dataEngine("powermanagement")->connectSource(battery_source, this);
    }

    dataEngine("powermanagement")->connectSource("AC Adapter", this);
    dataEngine("powermanagement")->connectSource("PowerDevil", this);

    connect(dataEngine("powermanagement"), SIGNAL(sourceAdded(QString)),
            this,                          SLOT(sourceAdded(QString)));
    connect(dataEngine("powermanagement"), SIGNAL(sourceRemoved(QString)),
            this,                          SLOT(sourceRemoved(QString)));
}

void Battery::sourceAdded(const QString& source)
{
    if (source.startsWith(QLatin1String("Battery")) && source != "Battery") {
        dataEngine("powermanagement")->connectSource(source, this);
        m_numOfBattery++;
        constraintsEvent(Plasma::SizeConstraint);
        update();
    }
    if (source == "PowerDevil") {
        dataEngine("powermanagement")->connectSource(source, this);
    }
}

void Battery::sourceRemoved(const QString& source)
{
    if (m_batteries_data.remove(source)) {
        m_numOfBattery--;
        constraintsEvent(Plasma::SizeConstraint);
        update();
    }
    if (source == "PowerDevil") {
        dataEngine("powermanagement")->disconnectSource(source, this);
    }
}

void Battery::setLabelAlpha(qreal alpha)
{
    m_labelAlpha = alpha;
    update();
}

qreal Battery::labelAlpha()
{
    return m_labelAlpha;
}

void Battery::setAcAlpha(qreal alpha)
{
    m_acAlpha = alpha;
    update();
}

qreal Battery::acAlpha()
{
    return m_acAlpha;
}

#include "battery.moc"
