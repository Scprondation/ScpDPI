import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Rectangle {
    id: root
    property var entries: []
    radius: Palette.radiusCard; color: Palette.card; border.width: 1; border.color: Palette.border
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 8
        Label { text: "Recent activity"; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold }
        Rectangle { Layout.fillWidth: true; height: 1; color: Palette.border }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: root.entries; spacing: 2
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Label { required property string modelData; width: ListView.view.width; text: modelData; color: Palette.textSecondary; font.pixelSize: 11; wrapMode: Text.Wrap; padding: 5 }
            Label { anchors.centerIn: parent; visible: root.entries.length === 0; text: "No log entries"; color: Palette.textSecondary }
        }
    }
}
