import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog

    readonly property var pages: [
        { title: qsTr("Preferences"), icon: "icons/configure.svg" },
        { title: qsTr("Detection"), icon: "icons/refresh.svg" },
        { title: qsTr("Thresholds"), icon: "icons/temperature.svg" },
        { title: qsTr("Alerts"), icon: "icons/alerts.svg" }
    ]

    function openPage(index) {
        list.currentIndex = index
        open()
    }

    title: qsTr("Configuration - HDGushagi")
    modal: true
    width: 700
    height: 470
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Ok

    component Section: ColumnLayout {
        spacing: 8
    }

    component Note: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: Theme.dimText
    }

    component Spin: SpinBox {
        editable: true
        Layout.preferredWidth: 110
    }

    contentItem: RowLayout {
        spacing: 10

        Rectangle {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: Theme.panel
            border.color: Theme.border

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 2
                model: dialog.pages
                currentIndex: 0
                delegate: Rectangle {
                    id: item
                    required property var modelData
                    required property int index
                    width: list.width
                    height: 26
                    color: ListView.isCurrentItem ? Theme.rowSelection : "transparent"
                    Row {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        Image {
                            width: 16
                            height: 16
                            sourceSize: Qt.size(32, 32)
                            source: item.modelData.icon
                        }
                        Label {
                            text: item.modelData.title
                            color: item.ListView.isCurrentItem ? Theme.rowSelectionText : Theme.text
                        }
                    }
                    TapHandler {
                        onTapped: list.currentIndex = item.index
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Rectangle {
                Layout.fillWidth: true
                height: 32
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: Theme.titleBandLeft }
                    GradientStop { position: 1; color: Theme.titleBandRight }
                }
                Row {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Image {
                        width: 20
                        height: 20
                        sourceSize: Qt.size(40, 40)
                        source: dialog.pages[list.currentIndex].icon
                    }
                    Label {
                        text: dialog.pages[list.currentIndex].title
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 16
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: list.currentIndex

                Section {
                    RowLayout {
                        Label { text: qsTr("Theme:") }
                        ComboBox {
                            Layout.preferredWidth: 220
                            model: [qsTr("Follow the desktop"), qsTr("Light"), qsTr("Dark (Palenight)")]
                            currentIndex: AppSettings.theme
                            onActivated: index => AppSettings.theme = index
                        }
                    }
                    CheckBox {
                        text: qsTr("Minimize to the system tray when the window is closed")
                        enabled: DiskManager.trayAvailable
                        checked: AppSettings.closeToTray
                        onToggled: AppSettings.closeToTray = checked
                    }
                    CheckBox {
                        text: qsTr("Start minimized to the system tray")
                        enabled: DiskManager.trayAvailable
                        checked: AppSettings.startMinimized
                        onToggled: AppSettings.startMinimized = checked
                    }
                    CheckBox {
                        text: qsTr("Start HDGushagi when I log in")
                        checked: AppSettings.autostart
                        onToggled: AppSettings.autostart = checked
                    }
                    CheckBox {
                        text: qsTr("Show S.M.A.R.T. data in hexadecimal")
                        checked: AppSettings.hexData
                        onToggled: AppSettings.hexData = checked
                    }
                    Item { Layout.fillHeight: true }
                }

                Section {
                    RowLayout {
                        Label { text: qsTr("Detection frequency:") }
                        Spin {
                            from: 1
                            to: 240
                            value: AppSettings.detectionMinutes
                            onValueModified: AppSettings.detectionMinutes = value
                        }
                        Label { text: qsTr("minutes") }
                    }
                    Note {
                        text: qsTr("At every detection the S.M.A.R.T. data of all disks is read, the health is evaluated and the temperature is recorded. NVMe temperatures are also followed continuously.")
                    }
                    CheckBox {
                        text: qsTr("Wake up sleeping hard disks to read their status")
                        checked: AppSettings.wakeSleepingDisks
                        onToggled: AppSettings.wakeSleepingDisks = checked
                    }
                    Note {
                        text: qsTr("Hard disks in standby are left alone by default, so monitoring doesn't keep them spinning. Their last known values are shown until they wake up.")
                    }
                    Item { Layout.fillHeight: true }
                }

                Section {
                    GridLayout {
                        columns: 3
                        columnSpacing: 12
                        rowSpacing: 6

                        Label { text: qsTr("Temperature") ; font.bold: true }
                        Label { text: qsTr("Warning (°C)") }
                        Label { text: qsTr("Critical (°C)") }

                        Label { text: qsTr("Hard disks") }
                        Spin {
                            from: 20; to: 100
                            value: AppSettings.hddTemperatureWarning
                            onValueModified: AppSettings.hddTemperatureWarning = value
                        }
                        Spin {
                            from: 20; to: 100
                            value: AppSettings.hddTemperatureCritical
                            onValueModified: AppSettings.hddTemperatureCritical = value
                        }

                        Label { text: qsTr("S-ATA SSDs") }
                        Spin {
                            from: 20; to: 110
                            value: AppSettings.ssdTemperatureWarning
                            onValueModified: AppSettings.ssdTemperatureWarning = value
                        }
                        Spin {
                            from: 20; to: 110
                            value: AppSettings.ssdTemperatureCritical
                            onValueModified: AppSettings.ssdTemperatureCritical = value
                        }

                        Label { text: qsTr("NVMe SSDs") }
                        Spin {
                            from: 20; to: 120
                            value: AppSettings.nvmeTemperatureWarning
                            onValueModified: AppSettings.nvmeTemperatureWarning = value
                        }
                        Spin {
                            from: 20; to: 120
                            value: AppSettings.nvmeTemperatureCritical
                            onValueModified: AppSettings.nvmeTemperatureCritical = value
                        }

                        Item { height: 8; Layout.columnSpan: 3 }

                        Label { text: qsTr("Health") ; font.bold: true }
                        Label { text: qsTr("Warning below (%)") }
                        Label { text: qsTr("Critical below (%)") }

                        Label { text: qsTr("All disks") }
                        Spin {
                            from: 1; to: 100
                            value: AppSettings.healthWarning
                            onValueModified: AppSettings.healthWarning = value
                        }
                        Spin {
                            from: 1; to: 100
                            value: AppSettings.healthCritical
                            onValueModified: AppSettings.healthCritical = value
                        }
                    }
                    Note {
                        text: qsTr("The thresholds color the status icons, the temperature bars and the tray icon, and decide when alerts are raised.")
                    }
                    Item { Layout.fillHeight: true }
                }

                Section {
                    Label { text: qsTr("Show a desktop notification and add an alert:") }
                    CheckBox {
                        text: qsTr("When a disk predicts its own failure")
                        checked: AppSettings.alertFailure
                        onToggled: AppSettings.alertFailure = checked
                    }
                    CheckBox {
                        text: qsTr("When the health of a disk drops below the thresholds")
                        checked: AppSettings.alertHealth
                        onToggled: AppSettings.alertHealth = checked
                    }
                    CheckBox {
                        text: qsTr("When the temperature is too high")
                        checked: AppSettings.alertTemperature
                        onToggled: AppSettings.alertTemperature = checked
                    }
                    CheckBox {
                        text: qsTr("When a new problem is added to the log")
                        checked: AppSettings.alertLogEntry
                        onToggled: AppSettings.alertLogEntry = checked
                    }
                    RowLayout {
                        CheckBox {
                            text: qsTr("When the free space of a volume is below")
                            checked: AppSettings.alertFreeSpace
                            onToggled: AppSettings.alertFreeSpace = checked
                        }
                        Spin {
                            from: 1; to: 50
                            value: AppSettings.freeSpacePercent
                            onValueModified: AppSettings.freeSpacePercent = value
                        }
                        Label { text: "%" }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
