import QtQuick

// FieldBg — the warm paper-textured TextField background used across
// SettingsView. A dedicated file (rather than an inline Component) because
// Control.background is an Item property, and `FieldBg {}` instantiates the
// Rectangle root directly.
Rectangle {
    color: theme.paper
    radius: theme.rSm
    border.color: theme.line
    border.width: 1
    implicitHeight: 36
}
