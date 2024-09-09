import QtQuick 2.0
import QtQuick.Controls 2.12

import bemu.gb 1.0

ApplicationWindow {
    visible: true

    width: 800
    height: 800

    Rectangle {
        color: "orange"
        anchors.fill: parent
    }

    Screen {
        width: 160
        height: 144
        anchors.centerIn: parent
        scale: 4
    }
}
