import QtQuick
import QtQuick.Controls

// The colored text description of a disk or of its temperature.
Rectangle {
    id: panel

    property var lines: []
    property string recommendation
    property int level: 0

    implicitHeight: Math.max(column.implicitHeight + 12, 84)
    color: level >= 2 ? Theme.criticalPanel : level === 1 ? Theme.warningPanel : Theme.goodPanel
    border.color: level >= 2 ? Theme.criticalPanelBorder : level === 1 ? Theme.warningPanelBorder : Theme.goodPanelBorder

    Column {
        id: column
        x: 6
        y: 6
        width: panel.width - 12

        Repeater {
            model: panel.lines
            Label {
                required property string modelData
                width: column.width
                text: modelData
                wrapMode: Text.Wrap
                color: Theme.panelText
            }
        }
        Item {
            width: 1
            height: panel.recommendation.length > 0 ? 12 : 0
        }
        Label {
            width: column.width
            visible: text.length > 0
            text: panel.recommendation
            font.bold: true
            wrapMode: Text.Wrap
            color: Theme.panelText
        }
    }
}
