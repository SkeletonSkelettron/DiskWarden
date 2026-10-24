import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ThemedWindow {
    id: win

    property var test
    readonly property var disk: test?.disk ?? null
    readonly property bool busy: test !== null && (test.state === SurfaceTest.Running || test.state === SurfaceTest.Opening)
    property string blockText

    function clock(seconds) {
        const pad = n => (n < 10 ? "0" : "") + n
        return pad(Math.floor(seconds / 3600)) + ":" + pad(Math.floor(seconds / 60) % 60) + ":" + pad(seconds % 60)
    }

    width: 960
    height: 700
    minimumWidth: 780
    minimumHeight: 540
    title: qsTr("Surface Test - HDGushagi")

    component Box: GroupBox {
        Layout.fillWidth: true
        padding: 6
        topPadding: 22
    }

    component Pair: RowLayout {
        property alias name: nameLabel.text
        property alias value: valueLabel.text
        Layout.fillWidth: true
        Label {
            id: nameLabel
            Layout.fillWidth: true
        }
        Label {
            id: valueLabel
            font.bold: true
            color: Theme.strongText
        }
    }

    component Legend: RowLayout {
        property alias name: legendName.text
        property alias value: legendValue.text
        property color swatch
        Layout.fillWidth: true
        Rectangle {
            width: 10
            height: 10
            color: parent.swatch
            border.color: Theme.border
        }
        Label {
            id: legendName
            Layout.fillWidth: true
        }
        Label {
            id: legendValue
            font.bold: true
            color: Theme.strongText
        }
    }

    header: Rectangle {
        height: 44
        color: Theme.window

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Label { text: qsTr("Disk:") }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.bold: true
                color: Theme.strongText
                text: win.disk ? "[%1]  %2  %3  %4  [%5]".arg(win.disk.index).arg(win.disk.sizeText).arg(win.disk.device)
                                                           .arg(win.disk.model).arg(win.disk.serial) : ""
            }
            Rectangle {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 24
                readonly property int state: win.test?.state ?? 0
                color: state === SurfaceTest.Failed ? Theme.criticalPanel
                     : state === SurfaceTest.Stopped || state === SurfaceTest.Opening ? Theme.warningPanel
                     : state === SurfaceTest.Idle ? Theme.card
                     : (win.test?.badBlocks ?? 0) > 0 ? Theme.criticalPanel : Theme.goodPanel
                border.color: Theme.border
                Label {
                    anchors.centerIn: parent
                    text: win.test?.stateText ?? ""
                    font.bold: true
                    color: Theme.panelText
                }
            }
            Button {
                text: win.busy ? qsTr("Stop Test") : qsTr("Start Test")
                enabled: win.test !== null && win.test.state !== SurfaceTest.Opening
                onClicked: win.busy ? win.test.stop() : win.test.start()
            }
            Button {
                text: qsTr("Close")
                onClicked: win.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label { text: qsTr("Test type:") }
            ComboBox {
                Layout.preferredWidth: 230
                enabled: !win.busy
                model: [qsTr("Read test"), qsTr("Quick read test")]
                currentIndex: win.test?.mode ?? 0
                onActivated: index => win.test.mode = index
            }
            Label {
                text: qsTr("Data impact:")
                leftPadding: 12
            }
            Label {
                text: qsTr("Non-destructive - safe to data")
                font.bold: true
                color: Theme.dark ? Theme.good : "#108010"
            }
            Item { Layout.fillWidth: true }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.dimText
            text: ((win.test?.mode ?? 0) === 0
                   ? qsTr("Reads every sector to verify that the whole surface is accessible and readable, and measures how long each block takes: slower blocks get darker colors, unreadable ones are red.")
                   : qsTr("Reads the first 8 MB of every block: a quick overview of the surface that finds larger damaged areas in minutes."))
                  + " " + qsTr("Nothing is written to the disk. It is opened read-only through udisks2, which asks for the administrator password.")
        }

        TabBar {
            id: tabs
            background: null
            TabButtonHds {
                text: qsTr("Disk surface map")
                iconSource: "icons/surface.svg"
            }
            TabButtonHds {
                text: qsTr("Temperature and transfer speed")
                iconSource: "icons/performance.svg"
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            RowLayout {
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.panel
                    border.color: Theme.border

                    SurfaceMapItem {
                        anchors.fill: parent
                        anchors.margins: 4
                        test: win.test
                        revision: win.test?.mapRevision ?? 0
                        backgroundColor: Theme.mapBackground
                        gapColor: Theme.mapGap
                        untestedColor: Theme.mapUntested
                        fastColor: Theme.mapFast
                        slowColor: Theme.mapSlow
                        damagedColor: Theme.mapDamaged
                        badColor: Theme.mapBad
                        activeColor: Theme.mapActive
                        onBlockClicked: index => win.blockText = win.test.blockDescription(index)
                        onBlockHovered: index => {
                            if (index >= 0 && win.test.blockCount > 0)
                                win.blockText = win.test.blockDescription(index)
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 200
                    Layout.maximumWidth: 200
                    Layout.fillHeight: true
                    spacing: 6

                    Box {
                        title: qsTr("Information")
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            Label { text: qsTr("Total sectors") }
                            Label {
                                text: Number(win.test?.totalSectors ?? 0).toLocaleString(Qt.locale(), 'f', 0)
                                font.bold: true
                                color: Theme.strongText
                            }
                            Label { text: qsTr("Block =") }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                font.bold: true
                                color: Theme.strongText
                                text: (win.test?.blockSize ?? 0) > 0
                                      ? qsTr("%1 sectors (%2 MB)")
                                            .arg(Number(win.test.blockSize / win.test.sectorSize).toLocaleString(Qt.locale(), 'f', 0))
                                            .arg(Number(win.test.blockSize / 1048576).toLocaleString(Qt.locale(), 'f', 2))
                                      : "-"
                            }
                        }
                    }
                    Box {
                        title: qsTr("Status")
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            Legend {
                                swatch: Theme.mapFast
                                name: qsTr("Good:")
                                value: win.test?.goodBlocks ?? 0
                            }
                            Legend {
                                swatch: Theme.mapDamaged
                                name: qsTr("Very slow:")
                                value: win.test?.damagedBlocks ?? 0
                            }
                            Legend {
                                swatch: Theme.mapBad
                                name: qsTr("Bad:")
                                value: win.test?.badBlocks ?? 0
                            }
                        }
                    }
                    Box {
                        title: qsTr("Progress")
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            Pair {
                                name: qsTr("Test:")
                                value: Number((win.test?.progress ?? 0) * 100).toLocaleString(Qt.locale(), 'f', 2) + " %"
                            }
                            Pair {
                                name: qsTr("Time:")
                                value: win.clock(win.test?.elapsed ?? 0)
                            }
                            Pair {
                                name: qsTr("Est.:")
                                value: win.busy ? win.clock(win.test.remaining) : "-"
                            }
                            Pair {
                                name: qsTr("Speed:")
                                value: Number((win.test?.currentSpeed ?? 0) / 1048576).toLocaleString(Qt.locale(), 'f', 2) + " MB/s"
                            }
                            Pair {
                                name: qsTr("Average:")
                                value: Number((win.test?.averageSpeed ?? 0) / 1048576).toLocaleString(Qt.locale(), 'f', 2) + " MB/s"
                            }
                        }
                    }
                    Box {
                        title: qsTr("Sectors")
                        Pair {
                            anchors.fill: parent
                            name: qsTr("Unreadable:")
                            value: Number(win.test?.badSectors ?? 0).toLocaleString(Qt.locale(), 'f', 0)
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.dimText
                        text: qsTr("Point at or click a block of the map to see its sectors and speed.")
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            ColumnLayout {
                spacing: 6

                ChartPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    chart.points: win.test?.temperatureSeries ?? []
                    chart.xMode: ChartItem.Time
                    chart.yTitle: qsTr("Temperature (°C)")
                    chart.xTitle: qsTr("Time")
                    chart.minimumSpan: 20
                    chart.showPoints: false
                    chart.emptyText: qsTr("The temperature is recorded while the test runs.")
                }
                ChartPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    chart.points: win.test?.speedSeries ?? []
                    chart.xMode: ChartItem.Number
                    chart.yTitle: qsTr("Disk Performance (MB/s)")
                    chart.xTitle: qsTr("Position (GByte)")
                    chart.yMinimum: 0
                    chart.showPoints: (win.test?.blockCount ?? 0) <= 2000
                    chart.emptyText: qsTr("The transfer speed of every block is shown here during the test.")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 86
            color: Theme.panel
            border.color: Theme.border

            ListView {
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: win.test?.log ?? []
                ScrollBar.vertical: ScrollBar {}
                delegate: Label {
                    required property var modelData
                    width: ListView.view.width
                    text: modelData.time + "   " + modelData.text
                    font.bold: true
                    elide: Text.ElideRight
                    color: modelData.level >= 2 ? Theme.critical
                         : modelData.level === 1 ? (Theme.dark ? Theme.warning : "#9a6a00")
                         : Theme.dark ? Theme.link : "#1a2a8a"
                }
            }
        }
    }

    footer: Rectangle {
        height: 24
        color: Theme.window
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.border
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Label {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: win.blockText
            }
            Label {
                text: win.disk
                      ? qsTr("Temperature: %1, Health: %2")
                            .arg(win.disk.hasTemperature ? Theme.temperatureText(win.disk.temperature) : "?")
                            .arg(win.disk.health >= 0 ? win.disk.health + " %" : "?")
                      : ""
            }
        }
    }
}
