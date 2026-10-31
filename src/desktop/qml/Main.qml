import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ScpDPI

ApplicationWindow {
    id: window
    visible: true
    width: 1280
    height: 840
    minimumWidth: 980
    minimumHeight: 680
    title: "ScpDPI"
    color: Palette.background
    property string currentPage: "Dashboard"
    readonly property var pageTitles: ({
        Dashboard: ["Overview", "A live view of this device's network paths."],
        Network: ["Network", "Interfaces and routes reported by the operating system."],
        VPN: ["VPN", "Tunnel signals with confidence and supporting evidence."],
        Profiles: ["Profiles", "Choose how ScpDPI labels and inspects route classes."],
        Diagnostics: ["Diagnostics", "Potential routing and interface issues."],
        Logs: ["Logs", "Local activity from this application session."],
        Settings: ["Settings", "Local configuration and data handling."],
        About: ["About", "ScpDPI network insight, version 0.1.0."]
    })

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Sidebar {
            Layout.fillHeight: true
            currentPage: window.currentPage
            onPageSelected: page => window.currentPage = page
        }
        Rectangle { width: 1; Layout.fillHeight: true; color: Palette.border }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Header {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                Layout.leftMargin: 30
                Layout.rightMargin: 34
                title: window.pageTitles[window.currentPage][0]
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Palette.border }
            Loader {
                id: pageLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 28
                source: Qt.resolvedUrl("pages/" + window.currentPage + "Page.qml")
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 30
                Layout.rightMargin: 34
                Layout.bottomMargin: 12
                Label { text: window.pageTitles[window.currentPage][1]; color: Palette.textSecondary; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: dashboard.lastUpdate === "Waiting for first scan" ? "Last scan —" : "Updated " + dashboard.lastUpdate; color: Palette.textSecondary; font.pixelSize: 10 }
            }
        }
    }
}
