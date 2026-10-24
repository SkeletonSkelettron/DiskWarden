import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Starts a self-test of the disk's firmware and follows its progress.
Dialog {
    id: dialog

    property var disk
    property string testType: "short"
    property int tick: 0
    readonly property bool running: disk?.selfTestRunning ?? false
    readonly property int progress: disk?.selfTestProgress ?? 0
    readonly property int elapsed: (tick, disk?.selfTestElapsed ?? 0)

    function start(target, type) {
        disk = target
        testType = type
        if (!disk.selfTestRunning)
            disk.startSelfTest(type)
        open()
    }

    function typeName(type) {
        return type === "extended" ? qsTr("Extended self-test")
             : type === "conveyance" ? qsTr("Conveyance self-test")
             : qsTr("Short self-test")
    }

    function clock(seconds) {
        const pad = n => (n < 10 ? "0" : "") + n
        return pad(Math.floor(seconds / 3600)) + ":" + pad(Math.floor(seconds / 60) % 60) + ":" + pad(seconds % 60)
    }

    function remainingText() {
        if (!running)
            return "-"
        if (progress <= 0) {
            return testType === "short" ? qsTr("about 2 minutes")
                 : testType === "conveyance" ? qsTr("about 5 minutes") : qsTr("calculating...")
        }
        return clock(Math.max(0, Math.round(elapsed * (100 - progress) / progress)))
    }

    title: qsTr("Disk Self-Test - Disk: %1").arg(disk?.index ?? "")
    modal: true
    width: 480
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape

    Timer {
        interval: 1000
        repeat: true
        running: dialog.visible
        onTriggered: dialog.tick++
    }

    contentItem: ColumnLayout {
        spacing: 10

        GridLayout {
            columns: 2
            columnSpacing: 16
            rowSpacing: 4

            Label { text: qsTr("Disk model:") }
            Label {
                text: dialog.disk?.model ?? ""
                font.bold: true
                color: Theme.strongText
            }
            Label { text: qsTr("Serial number:") }
            Label {
                text: dialog.disk?.serial ?? ""
                font.bold: true
                color: Theme.strongText
            }
            Label { text: qsTr("Test operation:") }
            Label {
                text: dialog.typeName(dialog.disk?.selfTestType || dialog.testType)
                font.bold: true
                color: Theme.strongText
            }
            Label { text: qsTr("Status:") }
            Label {
                Layout.fillWidth: true
                text: (dialog.disk?.selfTestError ?? "") || (dialog.disk?.selfTestStatus ?? "")
                font.bold: true
                wrapMode: Text.Wrap
                color: (dialog.disk?.selfTestError ?? "") ? Theme.critical : Theme.strongText
            }
        }

        GradientBar {
            Layout.fillWidth: true
            implicitHeight: 22
            value: dialog.progress / 100
            text: dialog.progress + " %"
        }

        GridLayout {
            columns: 2
            columnSpacing: 16
            rowSpacing: 4

            Label { text: qsTr("Elapsed time:") }
            Label {
                text: dialog.running ? dialog.clock(dialog.elapsed) : "-"
                font.bold: true
                color: Theme.strongText
            }
            Label { text: qsTr("Estimated time remaining:") }
            Label {
                text: dialog.remainingText()
                font.bold: true
                color: Theme.strongText
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.dimText
            text: qsTr("The test runs inside the disk, so the computer can be used meanwhile. Close this window with Background to let it continue: the result appears in the log of the disk. Starting a test needs the administrator password.")
        }
    }

    footer: DialogButtonBox {
        Button {
            text: dialog.running ? qsTr("Abort Test") : qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialog.running ? dialog.disk.abortSelfTest() : dialog.close()
        }
        Button {
            visible: dialog.running
            text: qsTr("Background")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
