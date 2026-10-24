pragma Singleton
import QtQuick

// Colors of the light theme, which follows the classic Windows look, and
// of the dark theme after the Material Palenight palette.
QtObject {
    readonly property bool dark: AppSettings.theme === 2
        || (AppSettings.theme === 0 && Application.styleHints.colorScheme === Qt.ColorScheme.Dark)

    // Menu, tool and status bars, tab strips.
    readonly property color window: dark ? "#1f2233" : "#f0f0f0"
    // Pages and lists.
    readonly property color panel: dark ? "#292d3e" : "#ffffff"
    readonly property color input: dark ? "#232635" : "#ffffff"
    readonly property color border: dark ? "#3a3f58" : "#a0a0a0"
    readonly property color lightBorder: dark ? "#33374d" : "#d4d4d4"
    readonly property color text: dark ? "#a6accd" : "#101010"
    readonly property color strongText: dark ? "#e2e5f5" : "#000000"
    readonly property color dimText: dark ? "#676e95" : "#6e6e6e"
    readonly property color link: dark ? "#82aaff" : "#0050c8"
    readonly property color accent: dark ? "#c792ea" : "#0078d7"

    readonly property color card: dark ? "#2f3347" : "#ececec"
    readonly property color cardBorder: dark ? "#3c4160" : "#c6c6c6"
    readonly property color selectedCard: dark ? "#444267" : "#cce8ff"
    readonly property color selectedCardBorder: dark ? "#82aaff" : "#7eb4ea"
    readonly property color rowSelection: dark ? "#444267" : "#3399ff"
    readonly property color rowSelectionText: "#ffffff"
    readonly property color rowHover: dark ? "#34324a" : "#e5f1fb"
    readonly property color headerTop: dark ? "#303348" : "#ffffff"
    readonly property color headerBottom: dark ? "#292c3f" : "#e9e9e9"
    readonly property color tabNormal: dark ? "#1f2233" : "#f0f0f0"
    readonly property color tabHover: dark ? "#34324a" : "#e5f1fb"
    readonly property color titleBandLeft: dark ? "#444267" : "#0a246a"
    readonly property color titleBandRight: dark ? "#292d3e" : "#a6caf0"

    // Status colors.
    readonly property color good: dark ? "#c3e88d" : "#66d066"
    readonly property color warning: dark ? "#ffcb6b" : "#f0d040"
    readonly property color critical: dark ? "#f07178" : "#f08080"

    // Bars.
    readonly property color barEmpty: dark ? "#1b1e2b" : "#f8f8f8"
    readonly property color barBorder: dark ? "#4e5579" : "#8c8c8c"
    readonly property color barText: dark ? "#1b1e2b" : "#101010"
    readonly property real barGloss: dark ? 0.12 : 0.3
    readonly property color healthLow: dark ? "#f07178" : "#ff3030"
    readonly property color healthMiddle: dark ? "#ffcb6b" : "#ffe83a"
    readonly property color healthHigh: dark ? "#c3e88d" : "#30d030"
    readonly property color temperature1: dark ? "#c3e88d" : "#30c830"
    readonly property color temperature2: dark ? "#d9e88a" : "#a8e040"
    readonly property color temperature3: dark ? "#ffcb6b" : "#ffd83a"
    readonly property color temperature4: dark ? "#f78c6c" : "#ff8a2a"
    readonly property color temperature5: dark ? "#f07178" : "#ff3030"
    readonly property color usedTop: dark ? "#82aaff" : "#4848ff"
    readonly property color usedBottom: dark ? "#5a7fd6" : "#1414b4"
    readonly property color freeTop: dark ? "#c792ea" : "#ff70ff"
    readonly property color freeBottom: dark ? "#9a62c4" : "#c800c8"

    // Description panels.
    readonly property color goodPanel: dark ? "#3a4646" : "#c4f3bc"
    readonly property color goodPanelBorder: dark ? "#7f9a5c" : "#80c878"
    readonly property color warningPanel: dark ? "#4a4542" : "#fff2b4"
    readonly property color warningPanelBorder: dark ? "#c9a35a" : "#d4b648"
    readonly property color criticalPanel: dark ? "#4d3948" : "#ffcaca"
    readonly property color criticalPanelBorder: dark ? "#c46a72" : "#d27474"
    readonly property color panelText: dark ? "#e2e5f5" : "#101010"

    // Charts.
    readonly property color chartBackgroundTop: dark ? "#262a3b" : "#fafafa"
    readonly property color chartBackgroundBottom: dark ? "#1b1e2b" : "#d2d2d2"
    readonly property color chartPlotTop: dark ? "#2b2f42" : "#ffffff"
    readonly property color chartPlotBottom: dark ? "#222536" : "#e4e4e4"
    readonly property color chartGrid: dark ? "#4a5070" : "#a0a0a0"
    readonly property color chartText: dark ? "#a6accd" : "#303030"
    readonly property color chartBorder: dark ? "#4e5579" : "#808080"
    readonly property color chartBox: dark ? "#1f2233" : "#ffffff"
    readonly property color chartBoxBorder: dark ? "#676e95" : "#606060"
    readonly property color chartBoxText: dark ? "#e2e5f5" : "#101010"
    readonly property color chartLine: dark ? "#82aaff" : "#0000c0"
    readonly property color chartAverage: dark ? "#f07178" : "#d02020"
    readonly property color chartActivity: dark ? "#89ddff" : "#00a8d0"

    // Surface map.
    readonly property color mapBackground: panel
    readonly property color mapGap: dark ? "#3a4052" : "#c8d8c8"
    readonly property color mapUntested: dark ? "#2f3347" : "#f4f4f4"
    readonly property color mapFast: dark ? "#c3e88d" : "#b4ecb0"
    readonly property color mapSlow: dark ? "#4c7a34" : "#1c6a1c"
    readonly property color mapDamaged: dark ? "#ffcb6b" : "#f2d35c"
    readonly property color mapBad: dark ? "#f07178" : "#e04646"
    readonly property color mapActive: dark ? "#82aaff" : "#3c78d8"

    // Activity lights.
    readonly property color readLight: dark ? "#89ddff" : "#4fc3f7"
    readonly property color writeLight: dark ? "#f78c6c" : "#ff9800"

    // Window palette for the Fusion controls.
    readonly property color paletteButton: dark ? "#303348" : "#e9e9e9"
    readonly property color paletteHighlight: dark ? "#5b5e99" : "#0078d7"
    readonly property color paletteLight: dark ? "#40455f" : "#ffffff"
    readonly property color paletteMidlight: dark ? "#363a52" : "#e3e3e3"
    readonly property color paletteMid: dark ? "#2b2f42" : "#a0a0a0"
    readonly property color paletteDark: dark ? "#1b1e2b" : "#808080"
    readonly property color paletteShadow: dark ? "#121420" : "#505050"
    readonly property color toolTip: dark ? "#202331" : "#ffffe1"

    function levelColor(level) {
        return level >= 2 ? critical : level === 1 ? warning : good
    }

    function temperatureText(value, decimals) {
        return Number(value).toFixed(decimals || 0) + " °C"
    }

    function kindIcon(kind) {
        return kind === 3 ? Qt.resolvedUrl("icons/nvme.svg")
             : kind === 2 ? Qt.resolvedUrl("icons/ssd.svg")
             : Qt.resolvedUrl("icons/hdd.svg")
    }
}
