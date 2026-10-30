import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property var disk
    readonly property var stats: disk?.temperatureStats ?? ({})
    readonly property int warningLevel: disk?.temperatureWarning ?? 50
    readonly property int criticalLevel: disk?.temperatureCritical ?? 60

    signal thresholdsRequested()

    function level(t) {
        return t >= criticalLevel ? 2 : t >= warningLevel ? 1 : 0
    }

    component TemperatureRow: RowLayout {
        id: row
        property string label
        property real value: 0
        property bool known: false
        property int decimals: 0
        property string note
        spacing: 8
        Layout.fillWidth: true

        StatusIcon {
            opacity: row.known ? 1 : 0
            level: page.level(row.value)
        }
        Label {
            text: row.label
            Layout.preferredWidth: 260
        }
        GradientBar {
            Layout.preferredWidth: 160
            mode: 2
            value: row.known ? row.value / 80 : 0
            warningAt: page.warningLevel / 80
            criticalAt: page.criticalLevel / 80
            text: row.known ? Theme.temperatureText(row.value, row.decimals) : "?"
        }
        Label {
            text: row.note
            color: Theme.dimText
            Layout.fillWidth: true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            TemperatureRow {
                label: qsTr("Current temperature:")
                known: page.disk?.hasTemperature ?? false
                value: page.disk?.temperature ?? 0
            }
            Label {
                text: "<a href=\"#\">" + qsTr("Set custom temperature thresholds") + "</a>"
                linkColor: Theme.link
                Layout.alignment: Qt.AlignTop
                onLinkActivated: page.thresholdsRequested()
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
        TemperatureRow {
            label: qsTr("Average temperature (today):")
            known: page.stats.hasToday ?? false
            value: page.stats.average ?? 0
            decimals: 2
        }
        TemperatureRow {
            label: qsTr("Maximum temperature (today):")
            known: page.stats.hasToday ?? false
            value: page.stats.maximumToday ?? 0
        }
        TemperatureRow {
            label: qsTr("Maximum temperature (ever measured):")
            known: page.stats.hasExtremes ?? false
            value: page.stats.maximumEver ?? 0
            note: page.stats.maximumEverTime ?? ""
        }
        TemperatureRow {
            label: qsTr("Minimum temperature (ever measured):")
            known: page.stats.hasExtremes ?? false
            value: page.stats.minimumEver ?? 0
            note: page.stats.minimumEverTime ?? ""
        }

        TextPanel {
            Layout.fillWidth: true
            level: page.disk?.hasTemperature ? page.disk.temperatureLevel : 1
            lines: {
                const result = []
                if (!page.disk || !page.disk.hasTemperature) {
                    result.push(page.disk?.standby ? qsTr("The disk is in standby, its temperature is read when it wakes up.")
                                                   : qsTr("The temperature of the disk can't be read."))
                } else if (page.disk.temperatureLevel >= 2) {
                    result.push(qsTr("The temperature is too high! It shortens the lifetime of the disk and can lead to data loss. Improve the cooling immediately."))
                } else if (page.disk.temperatureLevel === 1) {
                    result.push(qsTr("The temperature is higher than recommended. Check the cooling and the airflow around the disk."))
                } else {
                    result.push(qsTr("Temperature status is good."))
                }
                result.push(qsTr("Warning level: %1 °C, critical level: %2 °C for this type of disk.").arg(page.warningLevel).arg(page.criticalLevel))
                if ((page.stats.hotDays ?? 0) > 0)
                    result.push(qsTr("The average temperature was above the warning level on %n day(s), which shortens the estimated lifetime.", "", page.stats.hotDays))
                return result
            }
        }

        ChartPanel {
            id: chart
            Layout.fillWidth: true
            Layout.fillHeight: true
            tabs: [qsTr("Current (°C)"), qsTr("Daily Average (°C)"), qsTr("Daily Maximum (°C)"), qsTr("Daily Minimum (°C)")]
            chart.points: page.disk ? (page.disk.temperatureStats, page.disk.temperatureSeries(chart.currentIndex)) : []
            chart.xMode: chart.currentIndex === 0 ? ChartItem.Time : ChartItem.Date
            chart.showValues: true
            chart.minimumSpan: 20
            chart.valueDecimals: chart.currentIndex === 1 ? 1 : 0
            chart.emptyText: qsTr("The temperature is recorded at every detection.")
        }
    }
}
