import QtQuick
import QtQuick.Controls
import ScpDPI

Row {
    id: root
    property string text: "Unknown"
    property string tone: "neutral"
    spacing: 9
    readonly property color statusColor: tone === "success" ? Palette.success : tone === "warning" ? Palette.warning : tone === "error" ? Palette.error : Palette.textSecondary
    Rectangle {
        width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter
        color: root.statusColor
    }
    Label {
        text: root.text
        color: root.statusColor
        font.pixelSize: 13
        font.weight: Font.Medium
        anchors.verticalCenter: parent.verticalCenter
        elide: Text.ElideRight
    }
}
