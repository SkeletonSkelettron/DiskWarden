import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ThemedWindow {
    id: window

    readonly property var disk: DiskManager.currentDisk
    property int currentTab: AppSettings.currentTab
    property var surfaceWindows: ({})

    function openSurfaceTest(target) {
        if (!target)
            return
        let surface = surfaceWindows[target.key]
        if (!surface) {
            surface = surfaceComponent.createObject(window, { test: DiskManager.surfaceTest(target) })
            surfaceWindows[target.key] = surface
        }
        surface.show()
        surface.raise()
        surface.requestActivate()
    }

    // Used by --screenshot.
    function openConfiguration(page) {
        configurationDialog.openPage(page)
    }

    function closeConfiguration() {
        configurationDialog.close()
    }

    function openSurfaceTestOfCurrent() {
        openSurfaceTest(disk)
    }

    function startSelfTest(type) {
        if (disk)
            selfTestDialog.start(disk, type)
    }

    function statusTime(date) {
        if (!date || isNaN(date.getTime()))
            return qsTr("detecting...")
        return date.toLocaleDateString(Qt.locale(), Locale.ShortFormat) + " "
            + date.toLocaleDateString(Qt.locale(), "dddd") + " "
            + date.toLocaleTimeString(Qt.locale(), "HH:mm:ss")
    }

    width: 1120
    height: 740
    minimumWidth: 900
    minimumHeight: 580
    visible: false
    title: disk ? qsTr("Disk: %1, %2 [%3]  -  DiskWarden %4").arg(disk.index).arg(disk.model).arg(disk.serial).arg(DiskManager.version)
                : "DiskWarden " + DiskManager.version

    onCurrentTabChanged: AppSettings.currentTab = currentTab

    onClosing: close => {
        if (AppSettings.closeToTray && DiskManager.trayAvailable) {
            close.accepted = false
            window.hide()
        } else {
            Qt.quit()
        }
    }

    Connections {
        target: DiskManager
        function onConfigurationRequested() {
            configurationDialog.openPage(0)
        }
    }

    Component {
        id: surfaceComponent
        SurfaceTestWindow {}
    }

    SelfTestDialog {
        id: selfTestDialog
    }

    ConfigurationDialog {
        id: configurationDialog
    }

    AboutDialog {
        id: aboutDialog
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&Hide to Tray")
                enabled: DiskManager.trayAvailable
                onTriggered: window.hide()
            }
            MenuSeparator {}
            Action {
                text: qsTr("E&xit")
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&Disk")
            Action {
                text: qsTr("&Refresh")
                shortcut: "F5"
                onTriggered: DiskManager.refresh()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Surface Test...")
                enabled: window.disk !== null
                onTriggered: window.openSurfaceTest(window.disk)
            }
            Action {
                text: qsTr("S&hort Self-Test")
                enabled: window.disk?.selfTestSupported ?? false
                onTriggered: window.startSelfTest("short")
            }
            Action {
                text: qsTr("&Extended Self-Test")
                enabled: window.disk?.selfTestSupported ?? false
                onTriggered: window.startSelfTest("extended")
            }
            Action {
                text: qsTr("&Conveyance Self-Test")
                enabled: window.disk?.conveyanceSupported ?? false
                onTriggered: window.startSelfTest("conveyance")
            }
            Action {
                text: qsTr("&Abort Self-Test")
                enabled: window.disk?.selfTestRunning ?? false
                onTriggered: window.disk.abortSelfTest()
            }
        }
        Menu {
            title: qsTr("&View")
            Repeater {
                model: [qsTr("&Overview"), qsTr("&Temperature"), qsTr("&S.M.A.R.T."), qsTr("&Information"),
                        qsTr("&Log"), qsTr("Disk &Performance"), qsTr("&Alerts")]
                MenuItem {
                    required property string modelData
                    required property int index
                    text: modelData
                    checkable: true
                    checked: window.currentTab === index
                    onTriggered: window.currentTab = index
                }
            }
            MenuSeparator {}
            Menu {
                title: qsTr("T&heme")
                Repeater {
                    model: [qsTr("Follow the Desktop"), qsTr("Light"), qsTr("Dark (Palenight)")]
                    MenuItem {
                        required property string modelData
                        required property int index
                        text: modelData
                        checkable: true
                        checked: AppSettings.theme === index
                        onTriggered: AppSettings.theme = index
                    }
                }
            }
            MenuItem {
                text: qsTr("Hexadecimal S.M.A.R.T. Data")
                checkable: true
                checked: AppSettings.hexData
                onTriggered: AppSettings.hexData = checked
            }
        }
        Menu {
            title: qsTr("&Configuration")
            Action {
                text: qsTr("&Configuration...")
                shortcut: "F2"
                onTriggered: configurationDialog.openPage(0)
            }
            MenuSeparator {}
            MenuItem {
                text: qsTr("Minimize to Tray on Close")
                enabled: DiskManager.trayAvailable
                checkable: true
                checked: AppSettings.closeToTray
                onTriggered: AppSettings.closeToTray = checked
            }
            MenuItem {
                text: qsTr("Start When I Log In")
                checkable: true
                checked: AppSettings.autostart
                onTriggered: AppSettings.autostart = checked
            }
        }
        Menu {
            title: qsTr("&Help")
            Action {
                text: qsTr("&About DiskWarden")
                onTriggered: aboutDialog.open()
            }
        }
    }

    header: Rectangle {
        height: 38
        color: Theme.window

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }

        Row {
            x: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            ToolIconButton {
                iconSource: "icons/refresh.svg"
                tip: qsTr("Refresh (F5)")
                enabled: !DiskManager.refreshing
                onClicked: DiskManager.refresh()
            }
            ToolSeparator {}
            ToolIconButton {
                iconSource: "icons/surface.svg"
                tip: qsTr("Surface test")
                enabled: window.disk !== null
                onClicked: window.openSurfaceTest(window.disk)
            }
            ToolIconButton {
                iconSource: "icons/selftest-short.svg"
                tip: qsTr("Short self-test")
                enabled: window.disk?.selfTestSupported ?? false
                onClicked: window.startSelfTest("short")
            }
            ToolIconButton {
                iconSource: "icons/selftest-extended.svg"
                tip: qsTr("Extended self-test")
                enabled: window.disk?.selfTestSupported ?? false
                onClicked: window.startSelfTest("extended")
            }
            ToolSeparator {}
            ToolIconButton {
                iconSource: "icons/configure.svg"
                tip: qsTr("Configuration (F2)")
                onClicked: configurationDialog.openPage(0)
            }
            ToolIconButton {
                iconSource: "icons/alerts.svg"
                tip: qsTr("Alerts")
                onClicked: configurationDialog.openPage(3)
            }
            ToolSeparator {}
            ToolIconButton {
                iconSource: "icons/about.svg"
                tip: qsTr("About DiskWarden")
                onClicked: aboutDialog.open()
            }
        }

        BusyIndicator {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            running: DiskManager.refreshing
            visible: running
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
        Label {
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Status last updated: %1").arg(window.statusTime(DiskManager.lastUpdated))
        }
        Label {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: window.disk?.selfTestRunning ?? false
            text: qsTr("Self-test running: %1 %").arg(window.disk?.selfTestProgress ?? 0)
            color: Theme.link

            TapHandler {
                onTapped: selfTestDialog.start(window.disk, window.disk.selfTestType)
            }
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 4
        orientation: Qt.Horizontal

        handle: Rectangle {
            implicitWidth: 5
            color: Theme.window
        }

        Rectangle {
            SplitView.preferredWidth: 320
            SplitView.minimumWidth: 250
            color: Theme.panel
            border.color: Theme.border

            ScrollView {
                id: diskScroll
                anchors.fill: parent
                anchors.margins: 1
                contentWidth: availableWidth
                clip: true

                Column {
                    width: diskScroll.availableWidth
                    spacing: 2

                    Repeater {
                        model: DiskManager.disks
                        DiskCard {
                            width: parent.width
                        }
                    }

                    Item {
                        width: parent.width
                        height: 8
                    }

                    Repeater {
                        model: DiskManager.volumes
                        VolumeCard {
                            width: parent.width
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: DiskManager.disks.length === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.dimText
                text: DiskManager.available ? qsTr("Looking for disks...") : DiskManager.errorText
            }
        }

        ColumnLayout {
            spacing: 0

            TabBar {
                id: tabs
                Layout.fillWidth: true
                background: null
                currentIndex: window.currentTab
                onCurrentIndexChanged: window.currentTab = currentIndex

                TabButtonHds {
                    text: qsTr("Overview")
                    iconSource: "icons/overview.svg"
                }
                TabButtonHds {
                    text: qsTr("Temperature")
                    iconSource: "icons/temperature.svg"
                }
                TabButtonHds {
                    text: qsTr("S.M.A.R.T.")
                    iconSource: "icons/smart.svg"
                }
                TabButtonHds {
                    text: qsTr("Information")
                    iconSource: "icons/information.svg"
                }
                TabButtonHds {
                    text: qsTr("Log")
                    iconSource: "icons/log.svg"
                }
                TabButtonHds {
                    text: qsTr("Disk Performance")
                    iconSource: "icons/performance.svg"
                }
                TabButtonHds {
                    text: qsTr("Alerts")
                    iconSource: "icons/alerts.svg"
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: -1
                color: Theme.panel
                border.color: Theme.border

                StackLayout {
                    anchors.fill: parent
                    anchors.margins: 1
                    currentIndex: tabs.currentIndex
                    enabled: window.disk !== null

                    OverviewPage {
                        disk: window.disk
                    }
                    TemperaturePage {
                        disk: window.disk
                        onThresholdsRequested: configurationDialog.openPage(2)
                    }
                    SmartPage {
                        disk: window.disk
                    }
                    InformationPage {
                        disk: window.disk
                    }
                    LogPage {
                        disk: window.disk
                    }
                    PerformancePage {
                        disk: window.disk
                    }
                    AlertsPage {
                        disk: window.disk
                        onConfigureRequested: configurationDialog.openPage(3)
                    }
                }
            }
        }
    }
}
