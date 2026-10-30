import QtQuick
import QtQuick.Controls

// A disk in the list on the left: name, status, health and temperature.
Rectangle {
    id: card

    required property var modelData
    required property int index
    readonly property var disk: modelData
    readonly property bool selected: DiskManager.currentIndex === index

    height: 60
    color: selected ? Theme.selectedCard : area.containsMouse ? Qt.lighter(Theme.card, Theme.dark ? 1.08 : 1.02) : Theme.card
    border.color: selected ? Theme.selectedCardBorder : Theme.cardBorder

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: DiskManager.currentIndex = card.index
    }

    Row {
        id: header
        x: 6
        y: 4
        width: card.width - 12
        spacing: 4

        Image {
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            sourceSize: Qt.size(32, 32)
            source: Theme.kindIcon(card.disk.kind)
        }
        Label {
            id: name
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, header.width - 20 - size.implicitWidth - number.implicitWidth - 12)
            text: card.disk.model
            font.bold: true
            elide: Text.ElideRight
            color: Theme.strongText
        }
        Label {
            id: size
            anchors.verticalCenter: parent.verticalCenter
            text: "(" + card.disk.sizeText + ")"
        }
        Label {
            id: number
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Disk: %1").arg(card.disk.index)
            font.italic: true
            color: Theme.dimText
        }
    }

    StatusIcon {
        x: 6
        y: 31
        level: card.disk.statusLevel
    }

    Grid {
        x: 28
        y: 22
        columns: 3
        columnSpacing: 6
        rowSpacing: 2
        verticalItemAlignment: Grid.AlignVCenter

        Label {
            text: qsTr("Health:")
            width: 46
        }
        GradientBar {
            width: 92
            height: 16
            mode: 1
            value: card.disk.health >= 0 ? Math.max(0.05, card.disk.health / 100) : 1
            fillColor: card.disk.health < 0 ? Theme.cardBorder : Theme.levelColor(Math.max(0, card.disk.statusLevel))
            text: card.disk.health >= 0 ? card.disk.health + " %" : qsTr("unknown")
        }
        Label {
            width: card.width - 28 - 46 - 92 - 18
            text: card.disk.device
            font.italic: true
            color: Theme.dimText
            elide: Text.ElideRight
        }

        Label {
            text: qsTr("Temp.:")
            width: 46
        }
        GradientBar {
            width: 92
            height: 16
            mode: 1
            value: 1
            fillColor: card.disk.hasTemperature ? Theme.temperatureColor(card.disk.temperatureLevel) : Theme.cardBorder
            text: card.disk.hasTemperature ? Theme.temperatureText(card.disk.temperature)
                : card.disk.standby ? qsTr("standby") : qsTr("? °C")
        }
        Label {
            width: card.width - 28 - 46 - 92 - 18
            text: card.disk.mountSummary
            color: Theme.dimText
            elide: Text.ElideRight
        }
    }
}
