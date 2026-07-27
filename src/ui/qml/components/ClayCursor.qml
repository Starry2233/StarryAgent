import QtQuick

// ClayCursor — a blinking clay-colored text cursor (2px). Used as a
// TextField/TextArea `cursorDelegate`. Blinks on a 530ms timer while the
// owning field is focused (parent.cursorVisible true); hides on blur.
Rectangle {
    id: cursor
    color: theme.clay
    width: 2
    property bool _blinkOn: true
    property bool _focused: cursor.parent ? cursor.parent.cursorVisible : false
    visible: cursor._focused && cursor._blinkOn
    Timer {
        interval: 530
        repeat: true
        running: cursor._focused
        onTriggered: cursor._blinkOn = !cursor._blinkOn
        onRunningChanged: cursor._blinkOn = true   // show immediately on focus
    }
}
