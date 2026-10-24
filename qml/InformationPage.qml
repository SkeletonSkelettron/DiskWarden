import QtQuick
import QtQuick.Controls

Rectangle {
    id: page

    property var disk

    color: Theme.panel

    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        model: page.disk?.infoModel ?? null
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        section.property: "section"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            id: sectionItem
            required property string section
            width: list.width
            height: 30

            Row {
                x: 8
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                spacing: 6
                Image {
                    width: 16
                    height: 16
                    sourceSize: Qt.size(32, 32)
                    source: Theme.kindIcon(page.disk?.kind ?? 1)
                }
                Label {
                    text: sectionItem.section
                    font.bold: true
                    color: Theme.strongText
                }
            }
            Rectangle {
                x: 8
                anchors.bottom: parent.bottom
                width: parent.width - 16
                height: 1
                color: Theme.lightBorder
            }
        }

        delegate: Item {
            id: row
            required property string name
            required property string value
            width: list.width
            height: Math.max(20, valueText.implicitHeight + 4)

            Label {
                x: 32
                width: 300
                height: 20
                verticalAlignment: Text.AlignVCenter
                text: row.name
                elide: Text.ElideRight
            }
            TextEdit {
                id: valueText
                x: 340
                y: 2
                width: row.width - 350
                text: row.value
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                color: Theme.text
                selectionColor: Theme.paletteHighlight
                selectedTextColor: "#ffffff"
                font.pixelSize: 12
            }
        }
    }
}
