/*
    Copyright (C) 2014 Sialan Labs
    http://labs.sialan.org

    Sigram is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Sigram is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

import QtQuick 2.0

Rectangle {
    id: check_box
    width: 16
    height: 16
    color: "#00000000"
    border.width: 1
    border.color: "#333333"

    property bool checked: false

    Image {
        anchors.fill: parent
        anchors.margins: 2
        sourceSize: Qt.size(width,height)
        source: "files/sent.png"
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: check_box.checked
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: check_box.checked = !check_box.checked
    }
}
