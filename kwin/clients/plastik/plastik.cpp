/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#include <kconfig.h>

#include "misc.h"
#include "plastik.h"
#include "plastik.moc"
#include "plastikclient.h"

namespace KWinPlastik
{

// static bool pixmaps_created = false;

bool PlastikHandler::m_initialized    = false;
bool PlastikHandler::m_animateButtons = true;
bool PlastikHandler::m_titleShadow    = true;
bool PlastikHandler::m_shrinkBorders  = true;
bool PlastikHandler::m_menuClose      = false;
bool PlastikHandler::m_reverse        = false;
int  PlastikHandler::m_borderSize     = 4;
int  PlastikHandler::m_titleHeight    = 19;
int  PlastikHandler::m_titleHeightTool= 12;
QFont PlastikHandler::m_titleFont = QFont();
QFont PlastikHandler::m_titleFontTool = QFont();
Qt::AlignmentFlags PlastikHandler::m_titleAlign = Qt::AlignHCenter;

PlastikHandler::PlastikHandler()
{
    reset(0);
}

PlastikHandler::~PlastikHandler()
{
    m_initialized = false;
}

bool PlastikHandler::reset(unsigned long changed)
{
    // we assume the active font to be the same as the inactive font since the control
    // center doesn't offer different settings anyways.
    m_titleFont = KDecoration::options()->font(true, false); // not small
    m_titleFontTool = KDecoration::options()->font(true, true); // small

    switch(KDecoration::options()->preferredBorderSize( this )) {
        case BorderTiny:
            m_borderSize = 2;
            break;
        case BorderLarge:
            m_borderSize = 8;
            break;
        case BorderVeryLarge:
            m_borderSize = 12;
            break;
        case BorderHuge:
            m_borderSize = 18;
            break;
        case BorderVeryHuge:
            m_borderSize = 27;
            break;
        case BorderOversized:
            m_borderSize = 40;
            break;
        case BorderNormal:
        default:
            m_borderSize = 4;
    }

    // check if we are in reverse layout mode
    m_reverse = QApplication::reverseLayout();

    // read in the configuration
    readConfig();

    m_initialized = true;

    // Do we need to "hit the wooden hammer" ?
    bool needHardReset = true;
    // TODO: besides the Color and Font settings I can maybe handle more changes
    //       without a hard reset. I will do this later...
    if (changed & SettingColors || changed & SettingFont)
    {
        needHardReset = false;
    }

    if (needHardReset) {
        return true;
    } else {
        resetDecorations(changed);
        return false;
    }
}

KDecoration* PlastikHandler::createDecoration( KDecorationBridge* bridge )
{
        return new PlastikClient( bridge, this );
}

bool PlastikHandler::supports( Ability ability )
{
    switch( ability )
    {
        case AbilityAnnounceButtons:
        case AbilityButtonMenu:
        case AbilityButtonOnAllDesktops:
        case AbilityButtonSpacer:
        case AbilityButtonHelp:
        case AbilityButtonMinimize:
        case AbilityButtonMaximize:
        case AbilityButtonClose:
        case AbilityButtonAboveOthers:
        case AbilityButtonBelowOthers:
        case AbilityButtonShade:
            return true;
        default:
            return false;
    };
}

void PlastikHandler::readConfig()
{
    // create a config object
    KConfig config("kwinplastikrc");
    config.setGroup("General");

    // grab settings
    m_titleShadow    = config.readBoolEntry("TitleShadow", true);

    QFontMetrics fm(m_titleFont);  // active font = inactive font
    int titleHeightMin = config.readNumEntry("MinTitleHeight", 16);
    // The title should strech with bigger font sizes!
    m_titleHeight = QMAX(titleHeightMin, fm.height() + 4); // 4 px for the shadow etc.

    fm = QFontMetrics(m_titleFontTool);  // active font = inactive font
    int titleHeightToolMin = config.readNumEntry("MinTitleHeightTool", 13);
    // The title should strech with bigger font sizes!
    m_titleHeightTool = QMAX(titleHeightToolMin, fm.height() ); // don't care about the shadow etc.

    QString value = config.readEntry("TitleAlignment", "AlignHCenter");
    if (value == "AlignLeft")         m_titleAlign = Qt::AlignLeft;
    else if (value == "AlignHCenter") m_titleAlign = Qt::AlignHCenter;
    else if (value == "AlignRight")   m_titleAlign = Qt::AlignRight;

    m_animateButtons = config.readBoolEntry("AnimateButtons", true);
    m_menuClose = config.readBoolEntry("CloseOnMenuDoubleClick", true);
}

QColor PlastikHandler::getColor(KWinPlastik::ColorType type, const bool active)
{
    switch (type) {
        case WindowContour:
            return KDecoration::options()->color(ColorTitleBar, active).dark(190);
        case TitleGradientFrom:
            return KDecoration::options()->color(ColorTitleBar, active);
            break;
        case TitleGradientTo:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::white, active?210:220);
            break;
        case TitleGradientToTop:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::white, active?180:190);
            break;
        case TitleHighlightTop:
        case SideHighlightLeft:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::white, active?150:160);
            break;
        case SideHighlightRight:
        case SideHighlightBottom:
            return alphaBlendColors(KDecoration::options()->color(ColorTitleBar, active),
                    Qt::black, active?150:160);
            break;
        case Border:
            return KDecoration::options()->color(ColorFrame, active);
        case TitleFont:
            return KDecoration::options()->color(ColorFont, active);
        default:
            return Qt::black;
    }
}

QValueList< PlastikHandler::BorderSize >
PlastikHandler::borderSizes() const
{
    // the list must be sorted
    return QValueList< BorderSize >() << BorderTiny << BorderNormal <<
	BorderLarge << BorderVeryLarge <<  BorderHuge <<
	BorderVeryHuge << BorderOversized;
}

} // KWinPlastik

//////////////////////////////////////////////////////////////////////////////
// Plugin Stuff                                                             //
//////////////////////////////////////////////////////////////////////////////

static KWinPlastik::PlastikHandler *handler = 0;

extern "C"
{
    KDecorationFactory *create_factory()
    {
        handler = new KWinPlastik::PlastikHandler();
        return handler;
    }
}
