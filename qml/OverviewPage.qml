import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property var disk

    function levelOf(percent) {
        if (percent < 0)
            return -1
        return percent < AppSettings.healthCritical ? 2 : percent < AppSettings.healthWarning ? 1 : 0
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: 8
            rowSpacing: 8

            StatusIcon {
                level: page.levelOf(page.disk?.performance ?? -1)
            }
            Label {
                text: qsTr("Performance:")
                Layout.preferredWidth: 100
            }
            GradientBar {
                Layout.preferredWidth: 220
                value: (page.disk?.performance ?? 0) / 100
                text: (page.disk?.performance ?? -1) >= 0 ? page.disk.performance + " %" : qsTr("unknown")
            }
            Label {
                Layout.fillWidth: true
                text: page.disk?.performanceRating ?? ""
                font.bold: true
                color: Theme.strongText
            }
            HelpButton {
                text: qsTr("The performance is calculated from the performance related S.M.A.R.T. attributes (25 %), the data transfer errors of the last 30 days (25 %) and the speed the interface works at compared to the fastest it supports (50 %).")
            }

            StatusIcon {
                level: page.disk?.statusLevel ?? -1
            }
            Label {
                text: qsTr("Health:")
            }
            GradientBar {
                Layout.preferredWidth: 220
                value: (page.disk?.health ?? 0) / 100
                text: (page.disk?.health ?? -1) >= 0 ? page.disk.health + " %" : qsTr("unknown")
            }
            Label {
                Layout.fillWidth: true
                text: page.disk?.healthRating ?? ""
                font.bold: true
                color: Theme.strongText
            }
            HelpButton {
                text: qsTr("The health starts at 100 % and every problem found in the critical S.M.A.R.T. attributes lowers it: reallocated, pending and uncorrectable sectors, reallocation events, spin retries and seek errors. For SSDs the wear reported by the drive is taken into account as well.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextPanel {
                Layout.fillWidth: true
                lines: page.disk?.description ?? []
                recommendation: page.disk?.recommendation ?? ""
                level: page.disk?.descriptionLevel ?? 0
            }
            HelpButton {
                Layout.alignment: Qt.AlignTop
                text: qsTr("A short description of the status of the disk and what to do about it.")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            GridLayout {
                columns: 2
                columnSpacing: 12
                rowSpacing: 3

                Label {
                    text: qsTr("Power on time:")
                }
                Label {
                    text: page.disk?.powerOnText ?? ""
                    font.bold: true
                    color: Theme.strongText
                    Layout.preferredWidth: 160
                }
                Label {
                    text: qsTr("Estimated remaining lifetime:")
                }
                Label {
                    text: page.disk?.remainingText ?? ""
                    font.bold: true
                    color: Theme.strongText
                }
            }

            GridLayout {
                Layout.alignment: Qt.AlignTop
                columns: 2
                columnSpacing: 12
                rowSpacing: 3

                Label {
                    visible: (page.disk?.startStopCount ?? -1) >= 0
                    text: qsTr("Total start/stop count:")
                }
                Label {
                    visible: (page.disk?.startStopCount ?? -1) >= 0
                    text: Number(page.disk?.startStopCount ?? 0).toLocaleString(Qt.locale(), 'f', 0)
                    font.bold: true
                    color: Theme.strongText
                }
                Label {
                    visible: (page.disk?.lifetimeWritesText ?? "").length > 0
                    text: qsTr("Lifetime writes:")
                }
                Label {
                    visible: (page.disk?.lifetimeWritesText ?? "").length > 0
                    text: page.disk?.lifetimeWritesText ?? ""
                    font.bold: true
                    color: Theme.strongText
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Button {
                text: page.disk?.updating ? qsTr("Detecting...") : qsTr("Refresh")
                enabled: page.disk !== null && !page.disk.updating
                onClicked: page.disk.refresh()
            }
            HelpButton {
                text: qsTr("The remaining lifetime is an estimate. For SSDs it extrapolates the wear so far; for hard disks it starts from five years of continuous use and depends strongly on the health and on days with too high temperature.")
            }
        }

        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Click to add comment ...")
            text: page.disk?.comment ?? ""
            onEditingFinished: {
                if (page.disk)
                    page.disk.comment = text
            }
        }

        ChartPanel {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true
            tabs: [qsTr("Health (%)"), qsTr("Performance (%)")]
            chart.points: page.disk ? (page.disk.temperatureStats, page.disk.healthSeries(chart.currentIndex === 1)) : []
            chart.xMode: ChartItem.Date
            chart.showValues: true
            chart.minimumSpan: 10
            chart.emptyText: qsTr("The history appears after the first detection.")
        }
    }
}
