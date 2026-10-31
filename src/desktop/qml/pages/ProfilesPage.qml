import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Flickable {
    id: root; clip: true; contentWidth: width; contentHeight: content.implicitHeight
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    Column {
        id: content; width: root.width; spacing: 14
        Label { text: "Active profile"; color: Palette.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
        Repeater {
            model: [
                {id: "default", name: "Default", description: "Observe known VPN and direct routes. Unknown routes always bypass."},
                {id: "vpn-safe", name: "VPN Safe", description: "Bypass direct and unknown routes. Observe routes with VPN evidence."},
                {id: "diagnostic", name: "Diagnostic", description: "Observe known route classes and surface additional diagnostics. Unknown routes bypass."},
                {id: "custom", name: "Custom", description: "Safe baseline only. User-defined rules are not available in 0.1.0."}
            ]
            delegate: Rectangle {
                required property var modelData
                width: content.width; height: 86; radius: 14
                color: dashboard.profile === modelData.id ? "#282438" : Palette.card
                border.width: 1; border.color: dashboard.profile === modelData.id ? "#594A7A" : Palette.border
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 17; anchors.rightMargin: 16; spacing: 14
                    Rectangle {
                        width: 16; height: 16; radius: 8; color: "transparent"; border.width: 1; border.color: dashboard.profile === modelData.id ? Palette.accent : Palette.textSecondary
                        Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 4; color: Palette.accent; visible: dashboard.profile === modelData.id }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Label { text: modelData.name; color: Palette.textPrimary; font.pixelSize: 13; font.weight: Font.Medium }
                        Label { text: modelData.description; color: Palette.textSecondary; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                    Button {
                        text: dashboard.profile === modelData.id ? "Selected" : "Use profile"
                        enabled: dashboard.profile !== modelData.id
                        onClicked: dashboard.setProfile(modelData.id)
                        background: Rectangle { radius: 8; color: parent.enabled ? "#302A43" : Palette.surface }
                        contentItem: Text { text: parent.text; color: parent.enabled ? Palette.accentHover : Palette.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 9; rightPadding: 9 }
                    }
                }
            }
        }
        Label { text: "Policies describe inspection decisions only. ScpDPI 0.1.0 does not intercept or alter packets, DNS, routes or MTU."; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; width: parent.width }
    }
}
