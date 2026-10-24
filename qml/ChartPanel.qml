import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One or more graph tabs above a chart.
ColumnLayout {
    id: panel

    property var tabs: []
    property alias currentIndex: bar.currentIndex
    property alias chart: chart

    spacing: 0

    TabBar {
        id: bar
        visible: panel.tabs.length > 0
        background: null

        Repeater {
            model: panel.tabs
            TabButtonHds {
                required property string modelData
                text: modelData
            }
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: Theme.window
        border.color: Theme.border

        ChartItem {
            id: chart
            anchors.fill: parent
            anchors.margins: 1
            emptyText: qsTr("No data has been collected yet.")
            lineColor: Theme.chartLine
            lineColor2: Theme.chartAverage
            backgroundTop: Theme.chartBackgroundTop
            backgroundBottom: Theme.chartBackgroundBottom
            plotTop: Theme.chartPlotTop
            plotBottom: Theme.chartPlotBottom
            gridColor: Theme.chartGrid
            textColor: Theme.chartText
            borderColor: Theme.chartBorder
            boxColor: Theme.chartBox
            boxBorderColor: Theme.chartBoxBorder
            boxTextColor: Theme.chartBoxText
        }
    }
}
