import QtQuick

// ToggleRow — a single settings row with a title, an optional description,
// and a MiuixSwitch on the right. Emits `toggled(checked)` when the user
// flips it.
Item {
    id: root

    property string title: ""
    property string description: ""
    property bool checked: false
    property bool enabled: true

    signal toggled(bool checked)

    width: parent ? parent.width : 0
    height: Math.max(40, innerCol.implicitHeight)

    Column {
        id: innerCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 60   // leave room for the switch
        spacing: 2

        Text {
            text: root.title
            color: root.enabled ? theme.ink : theme.inkSoft
            font.family: theme.fontBody
            font.pixelSize: 13
        }
        Text {
            text: root.description
            color: theme.inkSoft
            opacity: root.enabled ? 1 : 0.55
            font.family: theme.fontBody
            font.pixelSize: 11
            width: parent.width
            wrapMode: Text.Wrap
            visible: root.description.length > 0
        }
    }

    MiuixSwitch {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        checked: root.checked
        enabled: root.enabled
        onToggled: root.toggled(checked)
    }
}
