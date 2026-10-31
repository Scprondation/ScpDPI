import QtQuick
import QtQuick.Controls
import ScpDPI

ComboBox {
    id: root
    model: ["default", "vpn-safe", "diagnostic", "custom"]
    currentIndex: Math.max(0, model.indexOf(dashboard.profile))
    implicitWidth: 170
    onActivated: dashboard.setProfile(currentText)
    Connections {
        target: dashboard
        function onChanged() { root.currentIndex = root.model.indexOf(dashboard.profile) }
    }
    contentItem: Text {
        leftPadding: 12; rightPadding: 30; text: root.displayText
        color: Palette.textPrimary; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12; elide: Text.ElideRight
    }
    background: Rectangle { radius: 9; color: Palette.cardRaised; border.color: root.activeFocus ? Palette.accent : Palette.border; border.width: 1 }
    indicator: Label { text: "⌄"; color: Palette.textSecondary; anchors.right: parent.right; anchors.rightMargin: 11; anchors.verticalCenter: parent.verticalCenter }
    delegate: ItemDelegate {
        width: root.width; text: modelData; highlighted: root.highlightedIndex === index
        contentItem: Text { text: modelData; color: Palette.textPrimary; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: parent.highlighted ? Palette.cardRaised : Palette.surface }
    }
    popup: Popup {
        y: root.height + 5; width: root.width; padding: 5
        background: Rectangle { radius: 10; color: Palette.surface; border.color: Palette.border }
    }
}
