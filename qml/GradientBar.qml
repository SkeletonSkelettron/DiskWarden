import QtQuick
import QtQuick.Controls

// The bars of the main window. Modes: 0 health gradient (red to green),
// 1 solid color, 2 temperature gradient, 3 used (blue) and free (magenta)
// space.
Rectangle {
    id: bar

    property real value: 0
    property string text
    property int mode: 0
    property color fillColor: Theme.good
    property bool boldText: false

    readonly property real clamped: Math.max(0, Math.min(1, value))

    implicitWidth: 150
    implicitHeight: 18
    color: Theme.barEmpty
    border.color: Theme.barBorder

    Item {
        id: fill
        x: 1
        y: 1
        width: (bar.width - 2) * (bar.mode === 3 ? 1 : bar.clamped)
        height: bar.height - 2
        clip: true

        Rectangle {
            visible: bar.mode === 0
            width: bar.width - 2
            height: parent.height
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.healthLow }
                GradientStop { position: 0.5; color: Theme.healthMiddle }
                GradientStop { position: 1.0; color: Theme.healthHigh }
            }
        }
        Rectangle {
            visible: bar.mode === 2
            width: bar.width - 2
            height: parent.height
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.temperature1 }
                GradientStop { position: 0.5; color: Theme.temperature2 }
                GradientStop { position: 0.68; color: Theme.temperature3 }
                GradientStop { position: 0.85; color: Theme.temperature4 }
                GradientStop { position: 1.0; color: Theme.temperature5 }
            }
        }
        Rectangle {
            visible: bar.mode === 1
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(bar.fillColor, 1.2) }
                GradientStop { position: 1.0; color: bar.fillColor }
            }
        }
        Row {
            visible: bar.mode === 3
            anchors.fill: parent

            Rectangle {
                width: parent.width * bar.clamped
                height: parent.height
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.usedTop }
                    GradientStop { position: 1.0; color: Theme.usedBottom }
                }
            }
            Rectangle {
                width: parent.width * (1 - bar.clamped)
                height: parent.height
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.freeTop }
                    GradientStop { position: 1.0; color: Theme.freeBottom }
                }
            }
        }
    }

    // Gloss over the upper half.
    Rectangle {
        x: 1
        y: 1
        width: fill.width
        height: (bar.height - 2) / 2
        color: "white"
        opacity: Theme.barGloss
    }

    // The text is drawn twice: dark over the fill, in the theme's text
    // color over the empty part.
    component BarText: Label {
        x: (bar.width - width) / 2 - parent.x
        y: (bar.height - height) / 2 - parent.y
        text: bar.text
        font.bold: bar.boldText
        font.pixelSize: 11
    }

    Item {
        x: fill.x
        width: fill.width
        height: bar.height
        clip: true
        BarText {
            color: bar.mode === 3 && !Theme.dark ? "white" : Theme.barText
        }
    }
    Item {
        x: fill.x + fill.width
        width: bar.width - x
        height: bar.height
        clip: true
        BarText {
            color: Theme.text
        }
    }
}
