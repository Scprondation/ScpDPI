import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Rectangle {
    id: root
    property string label: "Metric"
    property string value: "—"
    property string detail: ""
    property string glyph: "•"
    property color tint: Palette.accent
    radius: Palette.radiusCard
    color: Palette.card
    border.width: 1; border.color: Palette.border
    implicitHeight: 112
    RowLayout {
        anchors.fill: parent; anchors.margins: 17; spacing: 14
        Rectangle {
            width: 38; height: 38; radius: 12; color: Qt.alpha(root.tint, 0.13)
            Label { anchors.centerIn: parent; text: root.glyph; color: root.tint; font.pixelSize: 18; font.weight: Font.Medium }
        }
        ColumnLayout {
            spacing: 4
            Label { text: root.label; color: Palette.textSecondary; font.pixelSize: 11 }
            Label { text: root.value; color: Palette.textPrimary; font.pixelSize: 24; font.weight: Font.DemiBold }
            Label { text: root.detail; color: Palette.textSecondary; font.pixelSize: 10; elide: Text.ElideRight; Layout.fillWidth: true }
        }
    }
}
