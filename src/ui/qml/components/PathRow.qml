import QtQuick

// PathRow — a small label + a monospaced, wrapped path line. Used in the
// Storage section of SettingsView to show workspace/tools.jsonc/etc.
Item {
    id: root

    property string label: ""
    property string path: ""

    width: parent ? parent.width : 0
    height: col.implicitHeight

    Column {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 1

        Text {
            text: root.label
            color: theme.inkSoft
            font.family: theme.fontBody
            font.pixelSize: 10
        }
        Text {
            width: parent.width
            text: root.path
            color: theme.ink
            font.family: theme.fontMono
            font.pixelSize: 11
            wrapMode: Text.Wrap
            elide: Text.ElideMiddle
        }
    }
}
