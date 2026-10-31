import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Rectangle {
    id: root
    property var iface: ({})
    radius: 14; color: Palette.card; border.width: 1; border.color: Palette.border
    implicitHeight: 132
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 15; spacing: 9
        RowLayout {
            Layout.fillWidth: true; spacing: 9
            Rectangle { width: 30; height: 30; radius: 10; color: "#302A43"; Label { anchors.centerIn: parent; text: root.iface.virtual ? "◇" : "⌘"; color: Palette.accent; font.pixelSize: 15 } }
            ColumnLayout {
                spacing: 1; Layout.fillWidth: true
                Label { text: root.iface.name || "Unknown interface"; color: Palette.textPrimary; font.pixelSize: 13; font.weight: Font.Medium; elide: Text.ElideRight; Layout.fillWidth: true }
                Label { text: root.iface.kind || "Type unavailable"; color: Palette.textSecondary; font.pixelSize: 10 }
            }
            StatusIndicator { text: root.iface.up ? "Up" : "Down"; tone: root.iface.up ? "success" : "neutral" }
        }
        Label { text: root.iface.addresses ? root.iface.addresses.map(function(a) { return a.address }).join("   ·   ") : "Address unavailable"; color: Palette.textSecondary; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Label { text: "MTU  " + (root.iface.mtu || "Unavailable"); color: Palette.textSecondary; font.pixelSize: 10; Layout.fillWidth: true }
            Label { text: root.iface.mac || "MAC unavailable"; color: Palette.textSecondary; font.pixelSize: 10; elide: Text.ElideRight }
        }
    }
}
