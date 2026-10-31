import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

ColumnLayout {
    spacing: 16
    Rectangle {
        Layout.fillWidth: true; Layout.preferredHeight: 180; radius: 18; color: Palette.card; border.width: 1; border.color: Palette.border
        Column {
            anchors.centerIn: parent; spacing: 8
            Rectangle { width: 46; height: 46; radius: 14; color: Palette.accent; anchors.horizontalCenter: parent.horizontalCenter; Label { anchors.centerIn: parent; text: "S"; color: Palette.background; font.pixelSize: 24; font.weight: Font.Black } }
            Label { text: "ScpDPI"; color: Palette.textPrimary; font.pixelSize: 19; font.weight: Font.DemiBold; anchors.horizontalCenter: parent.horizontalCenter }
            Label { text: "Version 0.1.0 · Network insight"; color: Palette.textSecondary; font.pixelSize: 11; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
    Label { text: "ScpDPI reads interface and route metadata through native operating system APIs. It is designed to work alongside existing VPN clients without changing network behavior."; color: Palette.textSecondary; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
    Label { text: "The 0.1.0 release does not capture packets, alter DNS or routes, adjust MTU, or identify proxy protocols from encrypted traffic."; color: Palette.textSecondary; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
    Item { Layout.fillHeight: true }
}
