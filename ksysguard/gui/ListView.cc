/*
    KTop, the KDE Task Manager and System Monitor
   
	Copyright (c) 2001 Tobias Koenig <tokoe82@yahoo.de>
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	KTop is currently maintained by Chris Schlaeger <cs@kde.org>. Please do
	not commit any changes without consulting me first. Thanks!
*/
#include <stdio.h>
#include <qdom.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlistview.h>
#include <qpainter.h>
#include <qtextstream.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <kcolorbtn.h>

#include "SensorManager.h"
#include "ListViewSettings.h"
#include "ListView.moc"

ListViewItem::ListViewItem(MyListView *parent)
	: QListViewItem( parent )
{
	gridColor = (QColor)parent->getGridColor();
	textColor = (QColor)parent->getTextColor();
}

void ListViewItem::paintCell(QPainter *p, const QColorGroup &cg, int column, int width, int alignment)
{
	QColorGroup my_cg(cg);

	my_cg.setColor(QColorGroup::Text, textColor);
	my_cg.setColor(QColorGroup::Base, Qt::black);

	QListViewItem::paintCell(p, my_cg, column, width, alignment);
	p->setPen((QColor)gridColor);
	p->drawLine(0, height() - 1, width - 1, height() - 1);
}

void ListViewItem::paintFocus(QPainter *, const QColorGroup &, const QRect &)
{
	// dummy function
}

MyListView::MyListView(QWidget *parent)
	: QListView(parent)
{
	gridColor = QColor(Qt::green);
	textColor = QColor(Qt::green);

	// it's the only way to paint a black background :(
	QColorGroup cg = this->colorGroup();
	cg.setBrush(QColorGroup::Base, Qt::black);

	QPalette pal(cg, cg, cg);
	
	this->setPalette(pal);
}

ListView::ListView(QWidget* parent, const char* name, const QString& title, int, int)
	: SensorDisplay(parent, name)
{
	frame = new QGroupBox(1, Qt::Vertical, title, this, "frame"); 
	CHECK_PTR(frame);

	mainList = new MyListView(frame);
	CHECK_PTR(mainList);
	mainList->setBackgroundColor(Qt::black);
	mainList->setSelectionMode(QListView::NoSelection);

	KIconLoader iconLoader;
	QPixmap errorIcon = iconLoader.loadIcon("connect_creating", KIcon::Desktop, KIcon::SizeSmall);

	errorLabel = new QLabel(mainList);
	CHECK_PTR(errorLabel);

	errorLabel->setPixmap(errorIcon);
	errorLabel->resize(errorIcon.size());
	errorLabel->move(2, 2);

	/* All RMB clicks to the lcd widget will be handled by 
	 * SensorDisplay::eventFilter. */
	frame->installEventFilter(this);

	setMinimumSize(50, 25);
	modified = FALSE;
}

bool
ListView::addSensor(const QString& hostName, const QString& sensorName, const QString& title)
{
	registerSensor(hostName, sensorName, title);

	/* To differentiate between answers from value requests and info
	 * requests we use 100 for info requests. */
	sendRequest(hostName, sensorName + "?", 100);

	modified = TRUE;
	return (TRUE);
}

void
ListView::updateList()
{
	sendRequest(sensors.at(0)->hostName, sensors.at(0)->name, 19);
}

void
ListView::answerReceived(int id, const QString& answer)
{
	/* We received something, so the sensor is probably ok. */
	sensorError(false);

	switch (id)
	{
		case 100: {
			/* We have received the answer to a '?' command that contains
			 * the information about the table headers. */
			SensorTokenizer lines(answer, '\n');
			SensorTokenizer headers(lines[0], '\t');
		
			/* Remove all columns from list */
			for (int i = mainList->columns() - 1; i >= 0; --i) {
				mainList->removeColumn(i);
			}

			int width = (mainList->width() / headers.numberOfTokens() - 1);

			/* Add the new columns */
			for (unsigned int i = 0; i < headers.numberOfTokens(); i++)
			{
				mainList->addColumn(headers[i], -1);
				mainList->setColumnWidthMode(i, QListView::Maximum);
				if (i == (headers.numberOfTokens() - 1)) {
					width -= 15;
				}
				mainList->setColumnWidth(i, width);
			}

			timerOn();
			break;
		}
		case 19: {
			SensorTokenizer lines(answer, '\n');

			mainList->clear();
			for (unsigned int i = 0; i < lines.numberOfTokens(); i++) {
				ListViewItem *item = new ListViewItem(mainList);
				SensorTokenizer records(lines[i], '\t');
				for (unsigned int j = 0; j < records.numberOfTokens(); j++) {
					item->setText(j, records[j]);
				}
				mainList->insertItem(item);
			}

			timerOn();
			break;
		}
	}
}

void
ListView::resizeEvent(QResizeEvent*)
{
	frame->setGeometry(0, 0, this->width(), this->height());
}

bool
ListView::load(QDomElement& el)
{
	modified = false;

	title = el.attribute("title");
	mainList->setGridColor(QColor(el.attribute("gridcolor")));
	mainList->setTextColor(QColor(el.attribute("textcolor")));
	addSensor(el.attribute("hostName"), el.attribute("sensorName"), "");

	return (TRUE);
}

bool
ListView::save(QDomDocument&, QDomElement& display)
{
	display.setAttribute("hostName", sensors.at(0)->hostName);
	display.setAttribute("sensorName", sensors.at(0)->name);
	display.setAttribute("title", title);
	display.setAttribute("gridcolor", mainList->getGridColor().name());
	display.setAttribute("textcolor", mainList->getTextColor().name());
	modified = FALSE;

	return (TRUE);
}

void
ListView::settings()
{
	lvs = new ListViewSettings(this, "ListViewSettings", TRUE);
	CHECK_PTR(lvs);
	connect(lvs->applyButton, SIGNAL(clicked()), this, SLOT(applySettings()));

	lvs->gridColorButton->setColor(mainList->getGridColor());
	lvs->textColorButton->setColor(mainList->getTextColor());

	if (lvs->exec())
		applySettings();

	delete lvs;
	lvs = 0;
}

void
ListView::applySettings()
{
	mainList->setGridColor(lvs->gridColorButton->color());
	mainList->setTextColor(lvs->textColorButton->color());
	modified = TRUE;

	mainList->update(mainList->x(), mainList->y(), mainList->width(), mainList->height());
}

void
ListView::sensorError(bool err)
{
	if (err == sensors.at(0)->ok) {
		// this happens only when the sensorOk status needs to be changed.
		sensors.at(0)->ok = !err;
	}

	if (err)
		errorLabel->show();
	else
		errorLabel->hide();
}
