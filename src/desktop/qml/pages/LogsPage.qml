import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

ColumnLayout {
    spacing: 12
    RowLayout {
        Layout.fillWidth: true
        Label { text: "Application log"; color: Palette.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold; Layout.fillWidth: true }
        Button { text: "Refresh"; onClicked: dashboard.refresh(); flat: true; contentItem: Text { text: parent.text; color: Palette.accent; font.pixelSize: 11 } }
    }
    Label { text: "Entries are local to this application session. No packet content or external telemetry is collected."; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; Layout.fillWidth: true }
    LogViewer { Layout.fillWidth: true; Layout.fillHeight: true; entries: dashboard.logs }
}
