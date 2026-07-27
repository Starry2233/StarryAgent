import QtQuick

// MiuixSwitch — a fat pill switch ported from compose-miuix-ui's Switch.kt.
// Geometry: 49×28 track, 20dp round thumb traveling x=4 ↔ x=25 (21dp travel).
// Animation: thumb scale puffs to 1.127× on press/hover (spring, dampingRatio
// 0.6) and springs back on release; horizontal travel uses spring(dampingRatio
// 0.7); track color crossfades. Click-only (no drag) so it doesn't fight the
// scrolling Flickable it lives in.
//
// Pure black+white palette per the design brief — ON: black track + white
// thumb; OFF: neutral gray track + white thumb. Deliberately theme-independent
// so it reads as a crisp physical switch against the warm paper surface.
Item {
    id: root

    property bool checked: false
    signal toggled(bool checked)

    width: 49
    height: 28

    readonly property color _trackOn: theme.dark ? "#E8E1D0" : "#1A1A1A"
    readonly property color _trackOff: theme.dark ? "#3B342C" : "#D4D4D4"
    readonly property color _thumb: theme.dark ? "#1A1714" : "#FFFFFF"

    Rectangle {
        id: track
        anchors.fill: parent
        radius: parent.height / 2
        color: root.checked ? root._trackOn : root._trackOff
        border.color: Qt.rgba(0, 0, 0, 0.08)
        border.width: 1
        Behavior on color {
            ColorAnimation { duration: 220; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: thumb
        y: (parent.height - height) / 2
        width: 20
        height: 20
        radius: 10
        color: root._thumb
        border.color: Qt.rgba(0, 0, 0, 0.10)
        border.width: 1
        x: root.checked ? (parent.width - width - 4) : 4
        // MIUI puff: 1.127× while pressed or hovered, else 1.0
        scale: (ma.pressed || ma.containsMouse) ? 1.127 : 1.0

        Behavior on x {
            SpringAnimation {
                spring: 3.6
                damping: 0.35
                mass: 0.85
            }
        }
        Behavior on scale {
            SpringAnimation {
                spring: 5.0
                damping: 0.30
            }
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
