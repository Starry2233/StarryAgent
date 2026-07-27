import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// ModePicker — the hero moment (CLAUDE.md: "the one thing users remember").
// Three large cards: Agent / Coding / Pal. Staggered entrance, warm palette,
// no bouncy springs. Picking a mode creates the conversation.
Item {
    id: root

    signal picked(string modeId)

    // soft scrim over the chat area
    Rectangle {
        anchors.fill: parent
        color: theme.pageOverlay
        opacity: theme.hasWallpaper ? 1 : 0.96
    }

    // faint clay glow at the top for depth
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.5
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: theme.accent(0.08) }
            GradientStop { position: 1.0; color: theme.accent(0.00) }
        }
    }

    // Signature element: physics-based interactive star field
    // Stars respond to mouse movement with repulsion force and momentum,
    // creating a playful and memorable interaction that embodies "Starry"
    MouseArea {
        id: starFieldMouse
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true
        acceptedButtons: Qt.NoButton
    }

    Repeater {
        model: 18
        delegate: Item {
            id: starContainer
            width: 1
            height: 1

            // Physics state
            property real homeX: Math.random() * root.width
            property real homeY: root.height * 0.3 + Math.random() * root.height * 0.6
            property real physX: homeX
            property real physY: homeY
            property real velX: 0
            property real velY: (Math.random() - 0.5) * 0.3
            property real mass: 0.8 + Math.random() * 0.4
            property bool isActive: false

            x: physX
            y: physY

            Text {
                id: star
                anchors.centerIn: parent
                text: ["\u2726\uFE0E", "\u2727\uFE0E", "\u2731\uFE0E", "\u2732\uFE0E", "\u2733\uFE0E"][index % 5]
                color: theme.accent(0.15 + Math.random() * 0.12)
                font.family: theme.fontDisplay
                font.pixelSize: 12 + Math.random() * 16
                opacity: 0
                property real targetOpacity: 0.4 + Math.random() * 0.35

                Component.onCompleted: {
                    fadeInDelay.start()
                }

                Timer {
                    id: fadeInDelay
                    interval: Math.random() * 1800
                    repeat: false
                    onTriggered: {
                        star.opacity = star.targetOpacity
                        starContainer.isActive = true
                    }
                }

                Behavior on opacity {
                    NumberAnimation { duration: 1400; easing.type: Easing.OutCubic }
                }
            }

            // Natural drift animation (slow upward float + fade cycle)
            property real naturalDriftY: 0
            property real driftCycleProgress: 0
            readonly property real driftAmount: -120 - Math.random() * 80
            readonly property real driftDuration: 8000 + Math.random() * 6000

            Timer {
                id: driftCycleTimer
                interval: 50
                running: starContainer.isActive
                repeat: true
                onTriggered: {
                    starContainer.driftCycleProgress += 50 / starContainer.driftDuration
                    if (starContainer.driftCycleProgress >= 1.0) {
                        // Reset cycle
                        starContainer.driftCycleProgress = 0
                        starContainer.homeY = root.height * 0.3 + Math.random() * root.height * 0.6
                        star.opacity = star.targetOpacity
                    } else if (starContainer.driftCycleProgress > 0.85) {
                        // Fade out phase
                        const fadeProgress = (starContainer.driftCycleProgress - 0.85) / 0.15
                        star.opacity = star.targetOpacity * (1 - fadeProgress)
                    }

                    // Smooth easing for drift (InOutQuad)
                    let t = starContainer.driftCycleProgress
                    t = t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t
                    starContainer.naturalDriftY = t * starContainer.driftAmount
                }
            }

            // Physics update loop (60 fps)
            Timer {
                interval: 16
                running: starContainer.isActive
                repeat: true
                onTriggered: {
                    const mouseX = starFieldMouse.mouseX
                    const mouseY = starFieldMouse.mouseY
                    const dx = starContainer.physX - mouseX
                    const dy = starContainer.physY - mouseY
                    const dist = Math.sqrt(dx * dx + dy * dy)

                    // Repulsion force when mouse is nearby
                    const repelRadius = 150
                    if (dist < repelRadius && dist > 0.1) {
                        const force = (1 - dist / repelRadius) * 18.0 / starContainer.mass
                        starContainer.velX += (dx / dist) * force
                        starContainer.velY += (dy / dist) * force
                    }

                    // Gentle drift back to home position (with natural drift offset)
                    const homeForce = 0.02
                    const targetY = starContainer.homeY + starContainer.naturalDriftY
                    starContainer.velX += (starContainer.homeX - starContainer.physX) * homeForce
                    starContainer.velY += (targetY - starContainer.physY) * homeForce

                    // Damping
                    starContainer.velX *= 0.92
                    starContainer.velY *= 0.92

                    // Update position
                    starContainer.physX += starContainer.velX
                    starContainer.physY += starContainer.velY

                    // Boundary soft bounce
                    if (starContainer.physX < 0) {
                        starContainer.physX = 0
                        starContainer.velX *= -0.3
                    }
                    if (starContainer.physX > root.width) {
                        starContainer.physX = root.width
                        starContainer.velX *= -0.3
                    }
                    if (starContainer.physY < 0) {
                        starContainer.physY = 0
                        starContainer.velY *= -0.3
                    }
                    if (starContainer.physY > root.height) {
                        starContainer.physY = root.height
                        starContainer.velY *= -0.3
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: theme.sp5
        width: Math.min(parent.width - theme.sp6 * 2, 720)

        // entrance: fade + rise
        Component.onCompleted: {
            hero.opacity = 1
            hero.enterOffset = 0
        }

        Column {
            id: hero
            property real enterOffset: -20
            width: parent.width
            spacing: theme.sp3
            opacity: 0
            transform: Translate { y: hero.enterOffset }

            Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
            Behavior on enterOffset { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "\u2726\uFE0E"
                color: theme.clay
                font.family: theme.fontDisplay
                font.pixelSize: 40
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("How can I help?")
                color: theme.ink
                font.family: theme.fontDisplay
                font.pixelSize: 32
                font.weight: Font.Medium
                font.letterSpacing: -0.5
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Pick a mode to start a new conversation")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 13
                font.letterSpacing: 1
            }
        }

        // three mode cards — stack vertically on mobile, row on desktop
        Column {
            id: cardsCol
            width: parent.width
            spacing: theme.sp3

            // Desktop: 3 cards in a Row.
            Row {
                id: cards
                visible: win.width >= 600
                width: parent.width
                spacing: theme.sp4
                height: 200

                Repeater {
                    model: [
                        { id: "agent",  label: qsTr("Agent"),  mark: "\u2726\uFE0E", desc: qsTr("Delegate tasks. I'll search, analyze, and coordinate tools to get things done.") },
                        { id: "coding", label: qsTr("Coding"), mark: "</>",    desc: qsTr("Build together. I read your code, make changes, and run what's needed.") },
                        { id: "pal",    label: qsTr("Pal"),    mark: "\u2665", desc: qsTr("Just talk. I'm here to listen, discuss, and think through ideas with you.") }
                    ]
                    delegate: Rectangle {
                        id: desktopCard
                        property real enterOffset: 16
                        width: (cards.width - cards.spacing * 2) / 3
                        height: cards.height
                        radius: theme.rLg
                        color: cardMa.containsMouse ? theme.accent(0.06) : theme.surface
                        border.color: cardMa.containsMouse ? theme.clay : theme.line
                        border.width: 1
                        transform: Translate { y: desktopCard.enterOffset }
                        Behavior on color { ColorAnimation { duration: 200 } }
                        Behavior on border.color { ColorAnimation { duration: 200 } }

                        // staggered entrance
                        opacity: 0
                        Component.onCompleted: {
                            const d = 120 + index * 90
                            enterDelay.start()
                        }
                        Timer {
                            id: enterDelay
                            interval: 120 + index * 90
                            repeat: false
                            onTriggered: {
                                parent.opacity = 1
                                parent.enterOffset = 0
                            }
                        }
                        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                        Behavior on enterOffset { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                        Column {
                            anchors.fill: parent
                            anchors.margins: theme.sp4
                            spacing: theme.sp2

                            Text {
                                text: modelData.mark
                                color: theme.clay
                                font.family: theme.fontMono
                                font.pixelSize: 24
                            }
                            Text {
                                text: modelData.label
                                color: theme.ink
                                font.family: theme.fontDisplay
                                font.pixelSize: 22
                                font.weight: Font.Medium
                            }
                            Text {
                                width: parent.width
                                text: modelData.desc
                                color: theme.inkSoft
                                font.family: theme.fontBody
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }
                            Text {
                                text: "\u2192"
                                color: theme.clay
                                font.family: theme.fontBody
                                font.pixelSize: 16
                            }
                        }

                        MouseArea {
                            id: cardMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.picked(modelData.id)
                        }
                    }
                }
            }

            // Mobile: 3 cards stacked vertically, smaller.
            Repeater {
                id: mobileCards
                model: [
                    { id: "agent",  label: qsTr("Agent"),  mark: "\u2726\uFE0E", desc: qsTr("Delegate tasks. I'll search, analyze, and coordinate tools to get things done.") },
                    { id: "coding", label: qsTr("Coding"), mark: "</>",    desc: qsTr("Build together. I read your code, make changes, and run what's needed.") },
                    { id: "pal",    label: qsTr("Pal"),    mark: "\u2665", desc: qsTr("Just talk. I'm here to listen, discuss, and think through ideas with you.") }
                ]
                delegate: Rectangle {
                    id: mobileCard
                    property real enterOffset: 12
                    visible: win.width < 600
                    width: parent.width
                    height: 120
                    radius: theme.rMd
                    color: mCardMa.containsMouse ? theme.accent(0.06) : theme.surface
                    border.color: mCardMa.containsMouse ? theme.clay : theme.line
                    border.width: 1
                    transform: Translate { y: mobileCard.enterOffset }
                    Behavior on color { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    opacity: 0
                    Component.onCompleted: {
                        mEnterDelay.start()
                    }
                    Timer {
                        id: mEnterDelay
                        interval: 120 + index * 80
                        repeat: false
                        onTriggered: {
                            parent.opacity = 1
                            parent.enterOffset = 0
                        }
                    }
                    Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                    Behavior on enterOffset { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }

                    Row {
                        anchors.fill: parent
                        anchors.margins: theme.sp3
                        spacing: theme.sp3
                        Column {
                            width: parent.width - markCol.width - theme.sp3
                            spacing: theme.sp1
                            Text {
                                text: modelData.label
                                color: theme.ink
                                font.family: theme.fontDisplay
                                font.pixelSize: 18
                                font.weight: Font.Medium
                            }
                            Text {
                                width: parent.width
                                text: modelData.desc
                                color: theme.inkSoft
                                font.family: theme.fontBody
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }
                        Column {
                            id: markCol
                            spacing: theme.sp1
                            Text {
                                text: modelData.mark
                                color: theme.clay
                                font.family: theme.fontMono
                                font.pixelSize: 20
                            }
                            Text {
                                text: "\u2192"
                                color: theme.clay
                                font.family: theme.fontBody
                                font.pixelSize: 14
                            }
                        }
                    }

                    MouseArea {
                        id: mCardMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.picked(modelData.id)
                    }
                }
            }
        }
    }
}
