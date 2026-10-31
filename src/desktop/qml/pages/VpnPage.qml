import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Flickable {
    id: root; clip: true; contentWidth: width; contentHeight: content.implicitHeight
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    Column {
        id: content; width: root.width; spacing: 16
        Rectangle {
            width: parent.width; height: 106; radius: Palette.radiusCard; color: "#272235"; border.width: 1; border.color: "#44385F"
            RowLayout {
                anchors.fill: parent; anchors.margins: 18; spacing: 14
                Rectangle { width: 38; height: 38; radius: 12; color: "#3B3154"; Label { anchors.centerIn: parent; text: "◇"; color: Palette.accentHover; font.pixelSize: 19 } }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Label { text: "VPN and tunnel signals"; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Label { text: "Detection combines interface metadata with routes. It does not identify the VPN protocol."; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
                Label { text: dashboard.vpnCount.toString(); color: Palette.accentHover; font.pixelSize: 25; font.weight: Font.DemiBold }
            }
        }
        Repeater {
            model: dashboard.vpnInterfaces
            delegate: Rectangle {
                required property var modelData
                width: content.width; implicitHeight: evidence.implicitHeight + 32; radius: 14; color: Palette.card; border.width: 1; border.color: Palette.border
                ColumnLayout {
                    id: evidence; anchors.fill: parent; anchors.margins: 16; spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: modelData.name; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.Medium; Layout.fillWidth: true }
                        StatusIndicator { text: modelData.confidence; tone: modelData.confidence === "Confirmed" ? "success" : modelData.confidence === "Probable" ? "warning" : "neutral" }
                    }
                    Label { text: modelData.evidence; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
            }
        }
        Rectangle {
            visible: dashboard.vpnInterfaces.length === 0
            width: parent.width; height: 120; radius: 14; color: Palette.card; border.width: 1; border.color: Palette.border
            Column {
                anchors.centerIn: parent; spacing: 7
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "No VPN-like interface detected"; color: Palette.textPrimary; font.pixelSize: 13; font.weight: Font.Medium }
                Label { anchors.horizontalCenter: parent.horizontalCenter; text: "This does not rule out proxy-based VPN traffic."; color: Palette.textSecondary; font.pixelSize: 11 }
            }
        }
        Rectangle {
            width: parent.width; height: 120; radius: 14; color: Palette.surface; border.width: 1; border.color: Palette.border
            Column {
                anchors.fill: parent; anchors.margins: 15; spacing: 7
                Label { text: "Integrations"; color: Palette.textPrimary; font.pixelSize: 13; font.weight: Font.DemiBold }
                Label { text: "Xray, WireGuard, OpenVPN and Hysteria process integrations are Not Implemented in 0.1.0."; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; width: parent.width }
            }
        }
    }
}
