/*
 *   Copyright (C) 2007 Barış Metin <baris@pardus.org.tr>
 *   Copyright (C) 2006 David Faure <faure@kde.org>
 *   Copyright (C) 2007 Richard Moore <rich@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License version 2 as
 *   published by the Free Software Foundation
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

#include "calculatorrunner.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QScriptEngine>

#include <KIcon>

CalculatorRunner::CalculatorRunner( QObject* parent, const QVariantList &args )
    : Plasma::AbstractRunner(parent)
{
    Q_UNUSED(args)

    setObjectName(i18n("Calculator"));
}

CalculatorRunner::~CalculatorRunner()
{
}

void CalculatorRunner::match(Plasma::SearchContext *search)
{
    QString cmd = search->searchTerm();

    if (cmd[0] != '=') {
        return;
    }

    cmd = cmd.remove(0, 1).trimmed();

    if (cmd.isEmpty()) {
        return;
    }

    cmd.replace(QRegExp("([a-zA-Z]+)"), "Math.\\1");
    QString result = calculate(cmd);

    if (!result.isEmpty() && result != cmd) {
        Plasma::SearchAction *action = search->addInformationalMatch(this);
        action->setIcon(KIcon("accessories-calculator"));
        action->setText(QString("%1 = %2").arg(cmd, result));
        action->setData("= " + result);
    }
}

QString CalculatorRunner::calculate( const QString& term )
{
    //kDebug() << "calculating" << term;
    QScriptEngine eng;
    QScriptValue result = eng.evaluate(term);

    if (result.isError()) {
        return QString();
    }

    return result.toString();
}

#include "calculatorrunner.moc"
