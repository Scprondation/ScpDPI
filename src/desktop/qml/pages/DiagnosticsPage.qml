import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Flickable {
    id: root; clip: true; contentWidth: width; contentHeight: content.implicitHeight
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    Column {
        id: content; width: root.width; spacing: 12
        RowLayout {
            width: parent.width
            ColumnLayout {
                Layout.fillWidth: true; spacing: 3
                Label { text: "Diagnostic findings"; color: Palette.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                Label { text: "ScpDPI reports potential issues; DPI packet processing is controlled separately."; color: Palette.textSecondary; font.pixelSize: 11 }
            }
            Button { text: "Run scan"; onClicked: dashboard.refresh(); flat: true; contentItem: Text { text: parent.text; color: Palette.accent; font.pixelSize: 11 } }
        }
        Repeater {
            model: dashboard.diagnostics
            delegate: Rectangle {
                required property var modelData
                width: content.width; implicitHeight: row.implicitHeight + 26; radius: 12; color: Palette.card; border.width: 1; border.color: Palette.border
                RowLayout {
                    id: row; anchors.fill: parent; anchors.margins: 14; spacing: 11
                    Rectangle { width: 7; height: 7; radius: 4; color: modelData.severity === "error" ? Palette.error : modelData.severity === "warning" ? Palette.warning : Palette.accent; Layout.alignment: Qt.AlignTop }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Label { text: modelData.code.replaceAll("_", " "); color: Palette.textPrimary; font.pixelSize: 12; font.weight: Font.Medium }
                        Label { text: modelData.message; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
                    }
                    Label { text: modelData.severity.toUpperCase(); color: modelData.severity === "error" ? Palette.error : modelData.severity === "warning" ? Palette.warning : Palette.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold; Layout.alignment: Qt.AlignTop }
                }
            }
        }
        Rectangle {
            visible: dashboard.diagnostics.length === 0; width: parent.width; height: 104; radius: 12; color: Palette.card; border.width: 1; border.color: Palette.border
            Label { anchors.centerIn: parent; text: "No diagnostic findings from the latest scan."; color: Palette.textSecondary; font.pixelSize: 12 }
        }
    }
}
