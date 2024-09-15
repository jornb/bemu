import QtQuick 2.0
import QtQuick.Controls 2.12

import bemu.gb 1.0

ApplicationWindow {
    visible: true

    width: 800
    height: 800

    Rectangle {
        color: "white"
        anchors.fill: parent
    }

    Screen {
        width: 160*4
        height: 144*4
        anchors.centerIn: parent

        image: ctxEmulator.image
    }
}
