import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property var disk
    readonly property var model: disk?.smartModel ?? null
    readonly property bool nvme: model?.nvme ?? false
    property int selectedRow: 0
    readonly property int selectedId: model && model.count > 0 ? model.attributeId(Math.min(selectedRow, model.count - 1)) : -1

    // Column widths; the attribute name takes the rest.
    readonly property var fixedWidths: nvme ? [26, 40, 0, 52, 52, 52, 100, 250, 0, 0]
                                            : [26, 38, 0, 46, 46, 46, 128, 104, 80, 52]
    readonly property int nameWidth: Math.max(150, table.width - 2 - fixedWidths.reduce((a, b) => a + b, 0))

    function columnWidth(i) {
        return i === 2 ? nameWidth : fixedWidths[i]
    }

    onDiskChanged: selectedRow = 0

    component HeaderCell: Rectangle {
        property alias text: label.text
        property int column
        width: page.columnWidth(column)
        height: 22
        visible: width > 0
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.headerTop }
            GradientStop { position: 1; color: Theme.headerBottom }
        }
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.lightBorder
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.lightBorder
        }
        Label {
            id: label
            anchors.fill: parent
            anchors.leftMargin: 4
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component SmallButton: AbstractButton {
        id: small
        property string symbol
        width: 18
        height: 16
        contentItem: Label {
            text: small.symbol
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: Theme.dark ? "#1b1e2b" : "#ffffff"
        }
        background: Rectangle {
            radius: 2
            color: small.pressed ? Qt.darker(Theme.good, 1.3) : small.hovered ? Qt.lighter(Theme.good, 1.1) : Qt.darker(Theme.good, 1.12)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Rectangle {
            id: table
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 3
            color: Theme.panel
            border.color: Theme.border

            Column {
                id: header
                x: 1
                y: 1
                Row {
                    HeaderCell { column: 0; text: "" }
                    HeaderCell { column: 1; text: qsTr("No.") }
                    HeaderCell { column: 2; text: qsTr("Attribute") }
                    HeaderCell { column: 3; text: qsTr("Thre..."); ToolTip.text: qsTr("Threshold"); ToolTip.visible: thresholdHover.hovered; HoverHandler { id: thresholdHover } }
                    HeaderCell { column: 4; text: qsTr("Value") }
                    HeaderCell { column: 5; text: qsTr("Worst") }
                    HeaderCell { column: 6; text: qsTr("Status") }
                    HeaderCell { column: 7; text: qsTr("Data") }
                    HeaderCell { column: 8; text: qsTr("Offset") }
                    HeaderCell { column: 9; text: qsTr("Enable") }
                }
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.topMargin: header.height + 1
                anchors.margins: 1
                clip: true
                model: page.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: row
                    required property var model
                    required property int index
                    readonly property bool selected: index === page.selectedRow
                    readonly property color textColor: selected ? Theme.rowSelectionText
                                                     : model.enabled ? Theme.text : Theme.dimText

                    width: list.width
                    height: 21
                    color: selected ? Theme.rowSelection : rowHover.hovered ? Theme.rowHover : "transparent"

                    HoverHandler {
                        id: rowHover
                    }
                    TapHandler {
                        onTapped: page.selectedRow = row.index
                    }

                    Row {
                        height: parent.height

                        Item {
                            width: page.columnWidth(0)
                            height: parent.height
                            StatusIcon {
                                anchors.centerIn: parent
                                level: row.model.level
                            }
                        }
                        Label {
                            width: page.columnWidth(1)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            text: row.model.number
                            color: row.textColor
                        }
                        Label {
                            width: page.columnWidth(2)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: row.model.name
                            color: row.textColor
                        }
                        Label {
                            width: page.columnWidth(3)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            text: row.model.threshold
                            color: row.textColor
                        }
                        Label {
                            width: page.columnWidth(4)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            text: row.model.value
                            color: row.textColor
                        }
                        Label {
                            width: page.columnWidth(5)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            text: row.model.worst
                            color: row.textColor
                        }
                        Label {
                            width: page.columnWidth(6)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: row.model.status
                            color: row.selected ? Theme.rowSelectionText
                                 : row.model.level >= 2 ? Theme.critical
                                 : row.model.level === 1 ? (Theme.dark ? Theme.warning : "#9a6a00") : row.textColor
                        }
                        Label {
                            width: page.columnWidth(7)
                            height: parent.height
                            leftPadding: 4
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: row.model.data
                            font.family: "monospace"
                            color: row.textColor
                        }
                        Item {
                            width: page.columnWidth(8)
                            height: parent.height
                            visible: width > 0
                            Row {
                                anchors.centerIn: parent
                                spacing: 3
                                SmallButton {
                                    symbol: "−"
                                    onClicked: page.disk.changeAttributeOffset(row.model.number, -1)
                                }
                                Label {
                                    width: 26
                                    horizontalAlignment: Text.AlignHCenter
                                    text: row.model.offset
                                    color: row.textColor
                                }
                                SmallButton {
                                    symbol: "+"
                                    onClicked: page.disk.changeAttributeOffset(row.model.number, 1)
                                }
                            }
                        }
                        Item {
                            width: page.columnWidth(9)
                            height: parent.height
                            visible: width > 0
                            CheckBox {
                                anchors.centerIn: parent
                                padding: 0
                                checked: row.model.enabled
                                onToggled: page.disk.setAttributeEnabled(row.model.number, checked)
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 2
            spacing: 8

            ChartPanel {
                id: chart
                Layout.fillWidth: true
                Layout.fillHeight: true
                tabs: [page.model && page.model.count > 0
                       ? (page.model.count, page.model.attributeName(Math.min(page.selectedRow, page.model.count - 1)))
                       : qsTr("Attribute")]
                chart.points: page.disk && page.selectedId >= 0
                              ? (page.disk.temperatureStats, page.disk.attributeSeries(page.selectedId, graphMode.currentIndex === 1))
                              : []
                chart.xMode: ChartItem.Time
                chart.showValues: showValues.checked
                chart.minimumSpan: 2
                chart.emptyText: qsTr("The history of the attribute appears when its value changes.")
            }

            ColumnLayout {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                spacing: 4

                Label {
                    text: qsTr("Flags")
                    font.bold: true
                    color: Theme.strongText
                    visible: !page.nvme
                }
                Label {
                    Layout.fillWidth: true
                    visible: !page.nvme
                    text: page.model ? (page.model.count, page.model.flagsText(page.selectedRow)) : ""
                    wrapMode: Text.Wrap
                }
                CheckBox {
                    id: showValues
                    text: qsTr("Show values")
                    checked: true
                }
                Label {
                    text: qsTr("Attribute graph")
                    visible: !page.nvme
                }
                ComboBox {
                    id: graphMode
                    Layout.fillWidth: true
                    visible: !page.nvme
                    model: [qsTr("Display data field"), qsTr("Display value")]
                }
                CheckBox {
                    text: qsTr("Hexadecimal data")
                    checked: AppSettings.hexData
                    onToggled: AppSettings.hexData = checked
                }
                Item {
                    Layout.fillHeight: true
                }
                Label {
                    Layout.fillWidth: true
                    visible: !page.nvme
                    text: qsTr("The offset is subtracted from the data before it counts against the health. Disabled attributes don't affect the health and aren't logged.")
                    wrapMode: Text.Wrap
                    color: Theme.dimText
                    font.pixelSize: 11
                }
            }
        }
    }
}
