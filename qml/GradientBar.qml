import QtQuick
import QtQuick.Controls

// The bars of the main window. Modes: 0 health gradient (red to green),
// 1 solid color, 2 temperature bands (green, orange, red), 3 used (blue)
// and free (magenta) space.
Rectangle {
    id: bar

    property real value: 0
    property string text
    property int mode: 0
    property color fillColor: Theme.good
    property bool boldText: false
    // Where the orange and the red temperature bands start.
    property real warningAt: 0.625
    property real criticalAt: 0.75

    readonly property real clamped: Math.max(0, Math.min(1, value))

    // One band of a temperature bar.
    component Band: Rectangle {
        property real from
        property real to
        property color bandColor
        width: (bar.width - 2) * Math.max(0, Math.min(1, to) - Math.max(0, from))
        height: parent.height
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.lighter(bandColor, 1.2) }
            GradientStop { position: 1.0; color: bandColor }
        }
    }

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
        Row {
            visible: bar.mode === 2
            height: parent.height

            Band {
                from: 0
                to: bar.warningAt
                bandColor: Theme.temperatureColor(0)
            }
            Band {
                from: bar.warningAt
                to: bar.criticalAt
                bandColor: Theme.temperatureColor(1)
            }
            Band {
                from: bar.criticalAt
                to: 1
                bandColor: Theme.temperatureColor(2)
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
