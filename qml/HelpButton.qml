import QtQuick
import QtQuick.Controls

Image {
    id: help

    property string text

    width: 16
    height: 16
    sourceSize: Qt.size(16, 16)
    source: "icons/help.svg"

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: tip.visible = !tip.visible
    }

    ToolTip {
        id: tip
        visible: area.containsMouse
        delay: 300
        timeout: 30000
        width: Math.min(implicitWidth, 420)
        contentItem: Label {
            text: help.text
            wrapMode: Text.Wrap
        }
    }
}
