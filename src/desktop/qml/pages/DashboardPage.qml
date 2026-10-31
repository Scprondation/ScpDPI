import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Flickable {
    id: root
    clip: true
    contentWidth: width
    contentHeight: content.implicitHeight
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    Column {
        id: content
        width: root.width
        spacing: 20
        RowLayout {
            width: parent.width
            Label { text: "Your network at a glance"; color: Palette.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
            ProfileSelector {}
        }
        GridLayout {
            width: parent.width
            columns: width > 900 ? 4 : 2
            columnSpacing: 12; rowSpacing: 12
            MetricCard { Layout.fillWidth: true; label: "Interfaces"; value: dashboard.interfaces.length.toString(); detail: "Detected by the operating system"; glyph: "⌘"; tint: Palette.accent }
            MetricCard { Layout.fillWidth: true; label: "Routes"; value: dashboard.routeCount.toString(); detail: "IPv4 and IPv6 table entries"; glyph: "↗"; tint: "#7DD3FC" }
            MetricCard { Layout.fillWidth: true; label: "VPN signals"; value: dashboard.vpnCount.toString(); detail: "Potential tunnel interfaces"; glyph: "◇"; tint: "#C4B5FD" }
            MetricCard { Layout.fillWidth: true; label: "Alerts"; value: dashboard.warningCount.toString(); detail: "Warnings and errors in diagnostics"; glyph: "!"; tint: dashboard.warningCount > 0 ? Palette.warning : Palette.success }
        }
        GridLayout {
            width: parent.width; columns: width > 850 ? 2 : 1; columnSpacing: 12; rowSpacing: 12
            StatusCard {
                Layout.fillWidth: true
                title: "Core"
                value: dashboard.coreStatus
                detail: dashboard.lastError.length > 0 ? dashboard.lastError : "Read-only network scan · updated " + dashboard.lastUpdate
                tone: dashboard.coreStatus === "Running" ? "success" : dashboard.coreStatus === "Error" ? "error" : "warning"
            }
            StatusCard {
                Layout.fillWidth: true
                title: "VPN detection"
                value: dashboard.vpnStatus
                detail: "Interface evidence is probabilistic; protocol identity is not inferred from a name alone."
                tone: dashboard.vpnCount > 0 ? "warning" : "neutral"
            }
        }
        RowLayout {
            width: parent.width
            Label { text: "Core control"; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Button {
                text: dashboard.coreEnabled ? "Pause scans" : "Resume scans"
                onClicked: dashboard.setCoreEnabled(!dashboard.coreEnabled)
                background: Rectangle { radius: 8; color: parent.hovered ? "#3A3150" : "#302A43"; border.width: 1; border.color: "#594A7A" }
                contentItem: Text { text: parent.text; color: Palette.accentHover; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; leftPadding: 12; rightPadding: 12 }
            }
        }
        RowLayout {
            width: parent.width
            Label { text: "Interfaces"; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Button { text: "Refresh"; onClicked: dashboard.refresh(); flat: true; contentItem: Text { text: parent.text; color: Palette.accent; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter } }
        }
        GridLayout {
            width: parent.width; columns: width > 900 ? 2 : 1; columnSpacing: 12; rowSpacing: 12
            Repeater {
                model: dashboard.interfaces
                delegate: InterfaceCard { required property var modelData; Layout.fillWidth: true; iface: modelData }
            }
            Label { visible: dashboard.interfaces.length === 0; text: dashboard.coreStatus === "Error" ? "Interface data could not be loaded." : "No interfaces were returned."; color: Palette.textSecondary; font.pixelSize: 12 }
        }
        GridLayout {
            width: parent.width; columns: width > 850 ? 2 : 1; columnSpacing: 12; rowSpacing: 12
            RouteTable { Layout.fillWidth: true; compact: true; model: dashboard.routes }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 220
                radius: Palette.radiusCard; color: Palette.card; border.width: 1; border.color: Palette.border
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 13
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Diagnostics"; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.fillWidth: true }
                        Label { text: dashboard.diagnostics.length + " items"; color: Palette.textSecondary; font.pixelSize: 10 }
                    }
                    Repeater {
                        model: dashboard.diagnostics.slice(0, 3)
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true; spacing: 9
                            Rectangle { width: 6; height: 6; radius: 3; color: modelData.severity === "error" ? Palette.error : modelData.severity === "warning" ? Palette.warning : Palette.accent }
                            Label { text: modelData.message; color: Palette.textSecondary; font.pixelSize: 10; wrapMode: Text.Wrap; Layout.fillWidth: true; maximumLineCount: 2; elide: Text.ElideRight }
                        }
                    }
                    Label { visible: dashboard.diagnostics.length === 0; text: "No diagnostic findings."; color: Palette.textSecondary; font.pixelSize: 11 }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
