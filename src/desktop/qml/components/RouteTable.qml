import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Rectangle {
    id: root
    property var model: []
    property bool compact: false
    radius: Palette.radiusCard; color: Palette.card; border.width: 1; border.color: Palette.border
    implicitHeight: compact ? Math.min(330, 54 + model.length * 42) : 430
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Route table"; color: Palette.textPrimary; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.fillWidth: true }
            Label { text: root.model.length + " entries"; color: Palette.textSecondary; font.pixelSize: 10 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Palette.border }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Repeater {
                model: ["Destination", "Gateway", "Interface", "Metric"]
                delegate: Label {
                    required property string modelData
                    Layout.fillWidth: true; text: modelData; color: Palette.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold
                }
            }
        }
        ListView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: root.model; spacing: 2
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width; height: 36; radius: 7; color: modelData.default ? "#292439" : "transparent"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 8
                    Label { text: modelData.destination; color: modelData.default ? Palette.accentHover : Palette.textPrimary; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: modelData.gateway; color: Palette.textSecondary; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: modelData.interface || "Unknown"; color: Palette.textSecondary; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: modelData.metric; color: Palette.textSecondary; font.pixelSize: 10; Layout.fillWidth: true }
                }
            }
            Label {
                anchors.centerIn: parent; visible: root.model.length === 0
                text: "Route data is unavailable"; color: Palette.textSecondary; font.pixelSize: 12
            }
        }
    }
}
