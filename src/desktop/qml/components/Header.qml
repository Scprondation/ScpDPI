import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

RowLayout {
    id: root
    property string title: "Overview"
    property string subtitle: "Network visibility"
    spacing: 18
    Label {
        text: root.title
        color: Palette.textPrimary
        font.pixelSize: 25
        font.weight: Font.DemiBold
    }
    Item { Layout.fillWidth: true }
    Rectangle {
        Layout.preferredWidth: 152; Layout.preferredHeight: 34; radius: 17; color: Palette.surface
        Row {
            anchors.centerIn: parent; spacing: 7
            Rectangle { width: 6; height: 6; radius: 3; color: Palette.success; anchors.verticalCenter: parent.verticalCenter }
            Label { text: "LOCAL · READ ONLY"; color: Palette.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
