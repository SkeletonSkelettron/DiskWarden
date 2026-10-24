import QtQuick
import QtQuick.Controls

// A mounted file system in the list below the disks.
Rectangle {
    id: card

    required property var modelData

    height: 42
    color: Theme.card
    border.color: Theme.cardBorder

    Row {
        x: 6
        y: 3
        width: card.width - 12
        spacing: 6

        Label {
            id: name
            width: Math.min(implicitWidth, card.width - 24 - size.implicitWidth)
            text: card.modelData.name + (card.modelData.label ? "  [" + card.modelData.label + "]" : "")
            font.bold: true
            elide: Text.ElideMiddle
            color: Theme.strongText
        }
        Label {
            id: size
            text: "(" + card.modelData.sizeText + ")"
        }
    }

    Row {
        x: 6
        y: 21
        spacing: 6

        Label {
            width: 68
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Free Space")
        }
        GradientBar {
            width: 104
            height: 16
            mode: 3
            value: card.modelData.usedFraction
            text: card.modelData.freeText
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Disk: %1").arg(card.modelData.diskIndex)
            font.italic: true
            color: Theme.dimText
        }
    }

    ToolTip.visible: area.containsMouse
    ToolTip.delay: 600
    ToolTip.text: qsTr("%1\nMounted at: %2\nFile system: %3").arg(card.modelData.device).arg(card.modelData.mountPoints).arg(card.modelData.fsType)

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }
}
