/*
 *   Copyright 2009 by Artur Duque de Souza <morpheuz@gmail.com>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2,
 *   or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef STRIPWIDGET_H
#define STRIPWIDGET_H


#include <KConfigGroup>

#include <QPair>
#include <QGraphicsLinearLayout>
#include <QGraphicsSceneResizeEvent>
#include <QGraphicsWidget>


#include <Plasma/RunnerContext>

#include "itemview.h"

namespace Plasma
{
    class IconWidget;
    class ToolButton;
    class QueryMatch;
    class RunnerManager;
    class ScrollWidget;
}

class QGraphicsGridLayout;
class QTimer;
class IconActionCollection;

class StripWidget : public QGraphicsWidget
{
    Q_OBJECT

public:
    StripWidget(Plasma::RunnerManager *rm, QGraphicsWidget *parent = 0);
    ~StripWidget();

    void add(Plasma::QueryMatch match, const QString &query);
    void remove(Plasma::IconWidget *favourite);

    void save(KConfigGroup &cg);
    void restore(KConfigGroup &cg);

    void setIconSize(int iconSize);
    int iconSize() const;

    //TODO: geter and setter?
    void setImmutability(Plasma::ImmutabilityType immutability);

protected:
    void createIcon(Plasma::QueryMatch *match, int idx);
    void focusInEvent(QFocusEvent *event);

private Q_SLOTS:
    void removeFavourite();
    void launchFavourite();
    void launchFavourite(Plasma::IconWidget *icon);
    void arrowsNeededChanged(ItemView::ScrollBarFlags flags);
    void goLeft();
    void goRight();
    void scrollTimeout();
    void itemReordered(Plasma::IconWidget *icon, int index);
    //FIXME: this needs to go
    void highlightCurrentItem();

private:
    Plasma::ToolButton *m_leftArrow;
    Plasma::ToolButton *m_rightArrow;
    QGraphicsLinearLayout *m_arrowsLayout;

    Plasma::RunnerManager *m_runnermg;
    QList<Plasma::QueryMatch*> m_favouritesMatches;
    QHash<Plasma::QueryMatch*, QString> m_favouritesQueries;
    QHash<Plasma::IconWidget*, Plasma::QueryMatch*> m_favouritesIcons;
    ItemView *m_itemView;
    Plasma::RunnerContext *m_context;
    QTimer *m_scrollTimer;
    //FIXME: connecting to an animationcomplete signal from the icon?
    QTimer *m_setCurrentTimer;
    IconActionCollection *m_iconActionCollection;
    int m_shownIcons;
    int m_offset;
    bool m_startupCompleted;
};

#endif
