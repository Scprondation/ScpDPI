import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

Flickable {
    id: root; clip: true; contentWidth: width; contentHeight: content.implicitHeight
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    Column {
        id: content; width: root.width; spacing: 18
        RowLayout {
            width: parent.width
            ColumnLayout {
                Layout.fillWidth: true; spacing: 3
                Label { text: "Interfaces"; color: Palette.textPrimary; font.pixelSize: 15; font.weight: Font.DemiBold }
                Label { text: "Names, addresses, link state and MTU from native system APIs."; color: Palette.textSecondary; font.pixelSize: 11 }
            }
            Button { text: "Refresh"; onClicked: dashboard.refresh(); flat: true; contentItem: Text { text: parent.text; color: Palette.accent; font.pixelSize: 11 } }
        }
        GridLayout {
            width: parent.width; columns: width > 850 ? 2 : 1; columnSpacing: 12; rowSpacing: 12
            Repeater { model: dashboard.interfaces; delegate: InterfaceCard { required property var modelData; Layout.fillWidth: true; iface: modelData } }
            Label { visible: dashboard.interfaces.length === 0; text: "No interface data available."; color: Palette.textSecondary }
        }
        RouteTable { width: parent.width; model: dashboard.routes }
    }
}
