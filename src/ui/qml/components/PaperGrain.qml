import QtQuick

// PaperGrain — subtle warm texture overlay for main surfaces.
// CLAUDE.md: "Subtle grain, layered surfaces, soft shadows, decorative hairlines.
// Keep it restrained and warm — paper-like, not glassy/AI."
ShaderEffect {
    id: root

    property real time: 0
    property real intensity: 0.15  // subtle but visible
    property bool dark: false

    anchors.fill: parent
    blending: true  // Enable alpha blending with layer beneath

    // Gentle time animation for organic feel (very slow, imperceptible motion)
    NumberAnimation on time {
        from: 0
        to: 100
        duration: 120000
        loops: Animation.Infinite
    }

    fragmentShader: "qrc:/ui/qml/components/grain.frag.qsb"

    // Always visible; ShaderEffect gracefully degrades if shaders unavailable
    visible: true
}
