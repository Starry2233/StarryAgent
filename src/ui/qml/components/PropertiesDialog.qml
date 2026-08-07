import QtQuick
import QtQuick.Controls
import StarryAgent 1.0

// PropertiesDialog — a desktop modal window showing a conversation's
// metadata (id / title / mode / created / updated). Read-only. Mobile gets
// an in-page overlay variant (see Sidebar).
StarryWindow {
    id: dlg
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    color: theme.paper
    title: qsTr("Properties")

    property var conv: null

    width: 420
    height: contentCol.implicitHeight + 48   // auto-fit to content
    minimumWidth: 360

    function open(c) {
        dlg.conv = c
        dlg.visible = true
    }

    Column {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 12

        Text {
            text: qsTr("Conversation Properties")
            color: theme.ink
            font.family: theme.fontDisplay
            font.pixelSize: 16
            font.weight: Font.Medium
        }

        Repeater {
            model: [
                { label: qsTr("Title"), value: dlg.conv ? dlg.conv.title : "" },
                { label: qsTr("Mode"), value: dlg.conv ? dlg.conv.modeId : "" },
                { label: qsTr("ID"),   value: dlg.conv ? dlg.conv.id : "" },
                { label: qsTr("Created"), value: dlg.conv ? Qt.formatDateTime(dlg.conv.created, "yyyy-MM-dd HH:mm") : "" },
                { label: qsTr("Updated"), value: dlg.conv ? Qt.formatDateTime(dlg.conv.updated, "yyyy-MM-dd HH:mm") : "" }
            ]
            delegate: Column {
                width: parent.width
                spacing: 1
                Text {
                    text: modelData.label
                    color: theme.inkSoft
                    font.family: theme.fontBody
                    font.pixelSize: 10
                    font.letterSpacing: 1
                    font.capitalization: Font.AllUppercase
                }
                Text {
                    width: parent.width
                    text: modelData.value
                    color: theme.ink
                    font.family: theme.fontMono
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }

        Item { width: 1; height: 1 }   // spacer

        Button {
            text: qsTr("Close")
            anchors.right: parent.right
            contentItem: Text { text: parent.text; color: "white"; font.family: theme.fontBody; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle { color: parent.down ? theme.clayDeep : theme.clay; radius: theme.rPill; implicitWidth: 72; implicitHeight: 34 }
            onClicked: dlg.visible = false
        }
    }
}
