import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Rectangle {
    id: root
    property string currentPage: "Dashboard"
    signal pageSelected(string page)
    color: Palette.surface
    implicitWidth: 224
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 18; anchors.rightMargin: 14
        anchors.topMargin: 24; anchors.bottomMargin: 18
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            spacing: 11
            Rectangle {
                width: 38; height: 38; radius: 12
                color: Palette.accent
                Label { anchors.centerIn: parent; text: "S"; color: Palette.background; font.pixelSize: 20; font.weight: Font.Black }
            }
            Column {
                spacing: 2
                Label { text: "ScpDPI"; color: Palette.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
                Label { text: "NETWORK INSIGHT"; color: Palette.textSecondary; font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 1.2 }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.topMargin: 23; Layout.bottomMargin: 18; height: 1; color: Palette.border }
        Label { text: "WORKSPACE"; color: Palette.textSecondary; font.pixelSize: 10; font.weight: Font.DemiBold; font.letterSpacing: 1.1; Layout.bottomMargin: 10 }
        Repeater {
            model: [
                {name: "Dashboard", glyph: "◫"}, {name: "Network", glyph: "⌘"},
                {name: "VPN", glyph: "◇"}, {name: "Profiles", glyph: "◷"},
                {name: "Diagnostics", glyph: "⌁"}, {name: "Logs", glyph: "≡"}
            ]
            delegate: Button {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                Layout.bottomMargin: 3
                flat: true
                onClicked: root.pageSelected(modelData.name)
                background: Rectangle {
                    radius: Palette.radiusSmall
                    color: root.currentPage === modelData.name ? "#302A43" : parent.hovered ? Palette.card : "transparent"
                    Rectangle { width: 3; height: 20; radius: 2; color: Palette.accent; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; visible: root.currentPage === modelData.name }
                }
                contentItem: RowLayout {
                    spacing: 12
                    Label { text: modelData.glyph; color: root.currentPage === modelData.name ? Palette.accent : Palette.textSecondary; font.pixelSize: 17; Layout.leftMargin: 4; Layout.alignment: Qt.AlignVCenter }
                    Label { text: modelData.name; color: root.currentPage === modelData.name ? Palette.textPrimary : Palette.textSecondary; font.pixelSize: 13; font.weight: root.currentPage === modelData.name ? Font.Medium : Font.Normal; Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter }
                }
            }
        }
        Item { Layout.fillHeight: true }
        Rectangle { Layout.fillWidth: true; Layout.bottomMargin: 15; height: 1; color: Palette.border }
        Repeater {
            model: [{name: "Settings", glyph: "⚙"}, {name: "About", glyph: "ⓘ"}]
            delegate: Button {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: 39; flat: true
                onClicked: root.pageSelected(modelData.name)
                background: Rectangle { radius: Palette.radiusSmall; color: root.currentPage === modelData.name ? "#302A43" : parent.hovered ? Palette.card : "transparent" }
                contentItem: RowLayout {
                    spacing: 12
                    Label { text: modelData.glyph; color: root.currentPage === modelData.name ? Palette.accent : Palette.textSecondary; font.pixelSize: 15; Layout.leftMargin: 4 }
                    Label { text: modelData.name; color: root.currentPage === modelData.name ? Palette.textPrimary : Palette.textSecondary; font.pixelSize: 13; Layout.fillWidth: true }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.topMargin: 18; radius: 12; color: Palette.card; implicitHeight: 63
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                Rectangle { width: 8; height: 8; radius: 4; color: dashboard.coreStatus === "Running" ? Palette.success : Palette.warning }
                ColumnLayout {
                    spacing: 2
                    Label { text: "Core status"; color: Palette.textSecondary; font.pixelSize: 10 }
                    Label { text: dashboard.coreStatus; color: Palette.textPrimary; font.pixelSize: 12; font.weight: Font.Medium }
                }
                Item { Layout.fillWidth: true }
                Label { text: "0.1"; color: Palette.textSecondary; font.pixelSize: 10 }
            }
        }
    }
}
