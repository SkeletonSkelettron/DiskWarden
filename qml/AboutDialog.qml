import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog

    title: qsTr("About DiskWarden")
    modal: true
    width: 460
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Close

    contentItem: RowLayout {
        spacing: 16

        Image {
            Layout.alignment: Qt.AlignTop
            source: "icons/diskwarden.svg"
            sourceSize: Qt.size(64, 64)
        }
        ColumnLayout {
            spacing: 6
            Label {
                text: "DiskWarden " + DiskManager.version
                font.pixelSize: 18
                font.bold: true
                color: Theme.strongText
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("Monitors the health, temperature and performance of hard disks and SSDs, warns about problems before data is lost, and tests the disk surface.")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.dimText
                text: qsTr("S.M.A.R.T. data comes from udisks2, so no administrator rights are needed for monitoring. Self-tests and surface tests ask for the administrator password through polkit.")
            }
        }
    }
}
