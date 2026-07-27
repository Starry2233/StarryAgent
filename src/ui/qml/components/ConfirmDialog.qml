import QtQuick
import QtQuick.Window
import QtQuick.Controls

// ConfirmDialog — a desktop modal warning window. ApplicationModal blocks
// every window in the app until the user dismisses it (the "must close before
// continuing" binding the spec calls for). Used for the delete-conversation
// warning. Mobile gets an in-page overlay variant instead (see Sidebar).
Window {
    id: dlg
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    color: theme.paper
    title: qsTr("确认")

    property string message: ""
    property string confirmLabel: qsTr("删除")
    property string cancelLabel: qsTr("取消")
    signal confirmed()
    signal cancelled()

    width: 380
    height: 170
    minimumWidth: 320
    minimumHeight: 150

    function open(msg) {
        if (msg !== undefined) dlg.message = msg
        dlg.visible = true
    }

    Item {
        anchors.fill: parent

        Text {
            id: icon
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 20
            anchors.topMargin: 20
            text: "\u26a0"   // ⚠
            color: theme.clay
            font.family: theme.fontBody
            font.pixelSize: 22
        }
        Text {
            anchors.left: icon.right
            anchors.right: parent.right
            anchors.top: icon.top
            anchors.leftMargin: 12
            anchors.rightMargin: 20
            text: dlg.message
            color: theme.ink
            font.family: theme.fontBody
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }

        Row {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 20
            spacing: 12

            Button {
                text: dlg.cancelLabel
                contentItem: Text { text: parent.text; color: theme.ink; font.family: theme.fontBody; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? Qt.rgba(0,0,0,0.06) : "transparent"; border.color: theme.line; border.width: 1; radius: theme.rPill; implicitWidth: 72; implicitHeight: 34 }
                onClicked: { dlg.visible = false; dlg.cancelled() }
            }
            Button {
                text: dlg.confirmLabel
                contentItem: Text { text: parent.text; color: "white"; font.family: theme.fontBody; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? theme.clayDeep : theme.clay; radius: theme.rPill; implicitWidth: 72; implicitHeight: 34 }
                onClicked: { dlg.visible = false; dlg.confirmed() }
            }
        }
    }
}
