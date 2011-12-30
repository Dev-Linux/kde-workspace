/*
 *   Copyright 2011 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
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

import Qt 4.7
import org.kde.plasma.graphicswidgets 0.1 as PlasmaWidgets
import org.kde.plasma.core 0.1 as PlasmaCore
import org.kde.draganddrop 1.0

PlasmaCore.FrameSvgItem {
    id: background
    width: list.width / Math.floor(list.width / 180)
    height: list.height
    imagePath: "widgets/tasks"
    prefix: Current? "focus" : "normal"

    property variant icon: decoration
    property string title: name
    property string description: model.description
    property string author: model.author
    property string email: model.email
    property string license: model.license

    ListView.onRemove: SequentialAnimation {
        PropertyAction {
            target: background
            property: "ListView.delayRemove"
            value: true
        }
        NumberAnimation {
            target: background
            property: "y"
            to: background.height
            duration: 250
            easing.type: Easing.InOutQuad
        }
        PropertyAction {
            target: background
            property: "ListView.delayRemove"
            value: false
        }
    }

    ListView.onAdd: NumberAnimation {
        target: background
        property: "y"
        from: -background.height
        to: 0
        duration: 250
        easing.type: Easing.InOutQuad
    }

    Behavior on x {
        NumberAnimation { duration: 250 }
    }


    PlasmaWidgets.IconWidget {
        id:iconWidget
        anchors {
            left: parent.left
            top: parent.top
            bottom: parent.bottom
            leftMargin: background.margins.left
            topMargin: background.margins.top
            bottomMargin: background.margins.bottom
        }
        width: Math.min(64, height)
        icon: QIcon(Icon)
    }
    Column {
        anchors {
            left: iconWidget.right
            top: parent.top
            right: parent.right
            bottom: parent.bottom

            leftMargin: background.margins.left
            topMargin: background.margins.top
            rightMargin: background.margins.right
            bottomMargin: background.margins.bottom
        }
        Text {
            id: titleText
            color: theme.textColor
            text: Name
            anchors.left: parent.left
            anchors.right: parent.right
            height: paintedHeight
            clip: true
            wrapMode: Text.Wrap
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: {
            var activityId = model["DataEngineSource"]
            print(activityId)
            var service = activitySource.serviceForSource(activityId)
            var operation = service.operationDescription("setCurrent")
            service.startOperationCall(operation)
        }
    }
}
