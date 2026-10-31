import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Rectangle {
    id: root
    property string title: "Status"
    property string value: "Unknown"
    property string detail: ""
    property string tone: "neutral"
    readonly property color tint: tone === "success" ? Palette.success : tone === "warning" ? Palette.warning : tone === "error" ? Palette.error : Palette.accent
    radius: Palette.radiusCard; color: Palette.card; border.width: 1; border.color: Palette.border
    implicitHeight: 142
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: root.title; color: Palette.textSecondary; font.pixelSize: 12; Layout.fillWidth: true }
            StatusIndicator { text: root.tone === "success" ? "Healthy" : root.tone === "error" ? "Error" : root.tone === "warning" ? "Review" : "Info"; tone: root.tone }
        }
        Label { text: root.value; color: Palette.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight; wrapMode: Text.Wrap }
        Label { text: root.detail; color: Palette.textSecondary; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
    }
}
