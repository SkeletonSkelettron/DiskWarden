import QtQuick
import QtQuick.Controls

ToolButton {
    id: control

    property string iconSource
    property string tip

    implicitWidth: 32
    implicitHeight: 32
    display: AbstractButton.IconOnly
    icon.source: iconSource
    icon.width: 24
    icon.height: 24
    icon.color: "transparent"
    opacity: enabled ? 1 : 0.35

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.text: tip
    ToolTip.delay: 500
}
