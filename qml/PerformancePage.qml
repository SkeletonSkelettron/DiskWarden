import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: page

    property var disk
    readonly property real powerOnDays: Math.max(1, (disk?.powerOnHours ?? 0) / 24)

    function bytes(value) {
        if (value === undefined || value < 0)
            return qsTr("unknown")
        const units = ["bytes", "KB", "MB", "GB", "TB", "PB"]
        let unit = 0
        while (value >= 1024 && unit < units.length - 1) {
            value /= 1024
            ++unit
        }
        return Number(value).toLocaleString(Qt.locale(), 'f', unit === 0 ? 0 : 2) + " " + units[unit]
    }

    function rate(value) {
        const mb = value / (1024 * 1024)
        return Number(mb).toLocaleString(Qt.locale(), 'f', mb < 10 ? 2 : 1) + " MB/s"
    }

    component Light: Rectangle {
        property color on
        property bool active
        width: 12
        height: 12
        radius: 6
        color: active ? on : Theme.cardBorder
        border.color: Qt.darker(color, 1.5)
    }

    component Value: Label {
        font.bold: true
        color: Theme.strongText
        Layout.preferredWidth: 150
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        GridLayout {
            columns: 6
            columnSpacing: 8
            rowSpacing: 4

            Light {
                on: Theme.readLight
                active: (page.disk?.readRate ?? 0) > 0
            }
            Label { text: qsTr("Read speed:") }
            Value { text: page.rate(page.disk?.readRate ?? 0) }
            Light {
                on: Theme.writeLight
                active: (page.disk?.writeRate ?? 0) > 0
            }
            Label { text: qsTr("Write speed:") }
            Value { text: page.rate(page.disk?.writeRate ?? 0) }

            Item { width: 12 }
            Label { text: qsTr("Disk activity:") }
            GradientBar {
                Layout.preferredWidth: 150
                mode: 1
                fillColor: Theme.chartActivity
                value: (page.disk?.utilization ?? 0) / 100
                text: Math.round(page.disk?.utilization ?? 0) + " %"
            }
            Item { width: 12 }
            Label { text: qsTr("Estimated maximum transfer rate:") }
            Value { text: (page.disk?.estimatedMaxRate ?? 0) > 0 ? page.rate(page.disk.estimatedMaxRate) : qsTr("measuring...") }

            Item { width: 12 }
            Label { text: qsTr("Read in this session:") }
            Value { text: page.bytes(page.disk?.sessionRead ?? 0) }
            Item { width: 12 }
            Label { text: qsTr("Written in this session:") }
            Value { text: page.bytes(page.disk?.sessionWritten ?? 0) }

            Item { width: 12 }
            Label { text: qsTr("Total read (lifetime):") }
            Value { text: page.bytes(page.disk?.lifetimeRead ?? -1) }
            Item { width: 12 }
            Label { text: qsTr("Total written (lifetime):") }
            Value { text: page.bytes(page.disk?.lifetimeWritten ?? -1) }

            Item { width: 12 }
            Label { text: qsTr("Average read per power-on day:") }
            Value { text: (page.disk?.lifetimeRead ?? -1) >= 0 ? page.bytes(page.disk.lifetimeRead / page.powerOnDays) : qsTr("unknown") }
            Item { width: 12 }
            Label { text: qsTr("Average written per power-on day:") }
            Value { text: (page.disk?.lifetimeWritten ?? -1) >= 0 ? page.bytes(page.disk.lifetimeWritten / page.powerOnDays) : qsTr("unknown") }
        }

        ChartPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            tabs: [qsTr("Disk activity (%)")]
            chart.points: page.disk?.utilizationSeries ?? []
            chart.points2: page.disk?.utilizationAverage ?? []
            chart.lineColor: Theme.chartActivity
            chart.yMinimum: 0
            chart.yMaximum: 100
            chart.showPoints: false
            chart.xMode: ChartItem.Time
        }

        ChartPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            tabs: [qsTr("Transfer rate (MB/s)")]
            chart.points: page.disk?.rateSeries ?? []
            chart.points2: page.disk?.rateAverage ?? []
            chart.lineColor: Theme.chartActivity
            chart.minimumSpan: 10
            chart.showPoints: false
            chart.xMode: ChartItem.Time
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.dimText
            text: qsTr("The light blue line shows the current value, the red line the average of the last ten seconds.")
        }
    }
}
