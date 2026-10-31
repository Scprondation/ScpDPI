import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Flickable {
    id: root; clip: true; contentWidth: width; contentHeight: content.implicitHeight
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    Column {
        id: content; width: root.width; spacing: 14
        Label { text: "Configuration"; color: Palette.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
        Rectangle {
            width: parent.width; height: 65; radius: 12; color: Palette.card; border.width: 1; border.color: Palette.border
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                ColumnLayout { Layout.fillWidth: true; spacing: 3; Label { text: "Unknown route action"; color: Palette.textPrimary; font.pixelSize: 12 }; Label { text: "Required safe default for version 0.1.0"; color: Palette.textSecondary; font.pixelSize: 10 } }
                Label { text: "Bypass"; color: Palette.success; font.pixelSize: 12; font.weight: Font.Medium }
            }
        }
        Rectangle {
            width: parent.width; height: 65; radius: 12; color: Palette.card; border.width: 1; border.color: Palette.border
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                ColumnLayout { Layout.fillWidth: true; spacing: 3; Label { text: "DPI processing"; color: Palette.textPrimary; font.pixelSize: 12 }; Label { text: "Selected Windows connections are processed; DNS, routes and MTU stay unchanged"; color: Palette.textSecondary; font.pixelSize: 10 } }
                Label { text: "Windows"; color: Palette.accent; font.pixelSize: 12; font.weight: Font.Medium }
            }
        }
        Rectangle {
            width: parent.width; height: 65; radius: 12; color: Palette.card; border.width: 1; border.color: Palette.border
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16
                ColumnLayout { Layout.fillWidth: true; spacing: 3; Label { text: "Data handling"; color: Palette.textPrimary; font.pixelSize: 12 }; Label { text: "Diagnostics and logs stay on this device"; color: Palette.textSecondary; font.pixelSize: 10 } }
                Label { text: "Local only"; color: Palette.success; font.pixelSize: 12; font.weight: Font.Medium }
            }
        }
        Label { text: dashboard.lastError.length > 0 ? dashboard.lastError : "Configuration file is stored in the current user's application config directory."; color: dashboard.lastError.length > 0 ? Palette.error : Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; width: parent.width }
    }
}
