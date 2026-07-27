import QtQuick
import QtQuick.Controls

Button {
    id: root

    property string variant: "secondary"
    property bool compact: false
    property bool danger: false

    implicitWidth: Math.max(compact ? 34 : 76, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: compact ? 28 : 36
    leftPadding: compact ? theme.sp2 : theme.sp4
    rightPadding: compact ? theme.sp2 : theme.sp4
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true

    contentItem: Text {
        text: root.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: {
            if (!root.enabled)
                return theme.inkSoft
            if (root.variant === "primary")
                return "white"
            if (root.danger)
                return theme.clayDeep
            return theme.ink
        }
        opacity: root.enabled ? 1 : 0.52
        font.family: theme.fontBody
        font.pixelSize: root.compact ? 15 : 13
        font.weight: Font.Medium
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: theme.rPill
        color: {
            if (!root.enabled)
                return theme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.025)
            if (root.variant === "primary")
                return root.down ? theme.clayDeep : theme.clay
            if (root.danger)
                return root.down || root.hovered
                       ? Qt.rgba(theme.clay.r, theme.clay.g, theme.clay.b, theme.dark ? 0.18 : 0.12)
                       : "transparent"
            if (root.down || root.hovered)
                return theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.045)
            return theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(255, 255, 255, 0.34)
        }
        border.width: root.variant === "primary" ? 0 : 1
        border.color: {
            if (!root.enabled)
                return theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
            if (root.danger)
                return Qt.rgba(theme.clay.r, theme.clay.g, theme.clay.b, theme.dark ? 0.28 : 0.24)
            return root.hovered ? theme.clay : theme.line
        }

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }
}
