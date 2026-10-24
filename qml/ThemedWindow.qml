import QtQuick
import QtQuick.Controls

// A window whose controls follow the light or dark theme.
ApplicationWindow {
    color: Theme.window
    font.pixelSize: 12

    palette.window: Theme.window
    palette.windowText: Theme.text
    palette.base: Theme.input
    palette.alternateBase: Theme.panel
    palette.text: Theme.text
    palette.button: Theme.paletteButton
    palette.buttonText: Theme.text
    palette.brightText: Theme.strongText
    palette.highlight: Theme.paletteHighlight
    palette.highlightedText: "#ffffff"
    palette.light: Theme.paletteLight
    palette.midlight: Theme.paletteMidlight
    palette.mid: Theme.paletteMid
    palette.dark: Theme.paletteDark
    palette.shadow: Theme.paletteShadow
    palette.toolTipBase: Theme.toolTip
    palette.toolTipText: Theme.text
    palette.placeholderText: Theme.dimText
    palette.link: Theme.link
}
