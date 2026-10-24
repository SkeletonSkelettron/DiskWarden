import QtQuick
import QtQuick.Controls

TabButton {
    id: control

    property string iconSource

    width: implicitWidth
    implicitWidth: content.implicitWidth + 18
    implicitHeight: 25
    padding: 0

    contentItem: Item {
        Row {
            id: content
            anchors.centerIn: parent
            spacing: 5

            Image {
                visible: control.iconSource.length > 0
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                sourceSize: Qt.size(32, 32)
                source: control.iconSource
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                color: Theme.text
            }
        }
    }

    background: Rectangle {
        color: control.checked ? Theme.panel : control.hovered ? Theme.tabHover : Theme.tabNormal
        border.color: Theme.border

        // Merge the selected tab with the page below.
        Rectangle {
            visible: control.checked
            anchors.bottom: parent.bottom
            x: 1
            width: parent.width - 2
            height: 1
            color: Theme.panel
        }
    }
}
