import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The log or the alerts of a disk.
Item {
    id: page

    property var disk
    property bool alerts: false
    readonly property var model: alerts ? page.disk?.alertModel ?? null : page.disk?.logModel ?? null

    signal configureRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.panel
            border.color: Theme.border

            Row {
                id: header
                x: 1
                y: 1
                Repeater {
                    model: [[qsTr("Date"), 170], [page.alerts ? qsTr("Alert") : qsTr("Event"), Math.max(200, page.width - 16 - 172)]]
                    Rectangle {
                        required property var modelData
                        width: modelData[1]
                        height: 22
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
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            verticalAlignment: Text.AlignVCenter
                            text: parent.modelData[0]
                        }
                    }
                }
            }

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 1
                anchors.topMargin: header.height + 1
                clip: true
                model: page.model
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: row
                    required property var model
                    width: list.width
                    height: Math.max(22, text.implicitHeight + 6)

                    Row {
                        x: 6
                        spacing: 6
                        height: parent.height
                        StatusIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            level: row.model.level === 0 ? 3 : row.model.level
                        }
                        Label {
                            width: 142
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.model.time
                        }
                        Label {
                            id: text
                            width: row.width - 190
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.model.text
                            wrapMode: Text.Wrap
                            color: row.model.level >= 2 ? Theme.critical : Theme.text
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: page.alerts ? qsTr("No alerts have been raised for this disk.") : qsTr("No events have been logged for this disk yet.")
                    color: Theme.dimText
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.dimText
                text: page.alerts
                      ? qsTr("Alerts are shown as desktop notifications when the disk fails, its health or temperature crosses a threshold, a new problem is logged or a volume runs out of space.")
                      : qsTr("Changes of the critical S.M.A.R.T. attributes, health changes and the results of the disk tests are logged here.")
            }
            Button {
                visible: page.alerts
                text: qsTr("Configure alerts...")
                onClicked: page.configureRequested()
            }
            Button {
                text: page.alerts ? qsTr("Clear alerts") : qsTr("Clear log")
                enabled: (page.model?.count ?? 0) > 0
                onClicked: page.alerts ? page.disk.clearAlerts() : page.disk.clearLog()
            }
        }
    }
}
