import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components" as Components

ApplicationWindow {
    id: win
    visible: true
    width: 1180
    height: 760
    minimumWidth: 462
    minimumHeight: 600
    color: theme.hasWallpaper ? "transparent" : theme.paper
    title: qsTr("StarryAgent")

    function shouldHandleAndroidBackInQml() {
        if (Qt.platform.os !== "android")
            return true
        if (config.firstLaunch)
            return true
        return shell.drawerOpen
                || shell.destination === "settings"
                || (shell.destination === "picker" && frontendSessionStore.activeConversation)
    }

    function handleAndroidBack() {
        if (Qt.platform.os !== "android")
            return false
        if (config.firstLaunch)
            return true
        if (shell.drawerOpen) {
            shell.drawerOpen = false
            return true
        }
        if (shell.destination === "settings") {
            shell._navOverride = null
            return true
        }
        if (shell.destination === "picker" && frontendSessionStore.activeConversation) {
            shell._navOverride = null
            return true
        }
        return false
    }

    Shortcut {
        sequences: [StandardKey.Back]
        context: Qt.ApplicationShortcut
        enabled: win.shouldHandleAndroidBackInQml()
        onActivated: win.handleAndroidBack()
    }

    Theme { id: theme; dark: settings.theme === "dark" }
    readonly property bool androidChildSkillChooserVisible: Qt.platform.os === "android"
                                                     && skillInstallManager.hasPendingChildSelection

    Rectangle {
        anchors.fill: parent
        visible: androidChildSkillChooserVisible
        z: 1000
        color: Qt.rgba(0, 0, 0, 0.35)

        MouseArea {
            anchors.fill: parent
            onClicked: skillInstallManager.clearPendingChildSelection()
        }
    }

    Rectangle {
        visible: androidChildSkillChooserVisible
        z: 1001
        width: Math.min(win.width - theme.sp4 * 2, 560)
        height: Math.min(win.height - theme.sp4 * 2, childSkillChooserColumn.implicitHeight + theme.sp4 * 2)
        x: (win.width - width) / 2
        y: (win.height - height) / 2
        radius: theme.rLg
        color: theme.surface
        border.color: theme.line
        border.width: 1

        Column {
            id: childSkillChooserColumn
            width: parent.width - theme.sp4 * 2
            anchors.top: parent.top
            anchors.topMargin: theme.sp4
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: theme.sp3

            Text {
                width: parent.width
                text: qsTr("Parent skill installed")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
            }

            Text {
                width: parent.width
                text: skillInstallManager.pendingParentSkillName.length > 0
                      ? skillInstallManager.pendingParentSkillName
                      : skillInstallManager.pendingParentSkillId
                color: theme.ink
                font.family: theme.fontDisplay
                font.pixelSize: 18
                font.weight: Font.Medium
                wrapMode: Text.Wrap
            }

            Text {
                width: parent.width
                text: qsTr("Choose one child skill to add from this package.")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Rectangle {
                width: parent.width
                height: Math.min(360, childSkillList.contentHeight + theme.sp3 * 2)
                radius: theme.rLg
                color: theme.surfaceAlt
                border.color: theme.line
                border.width: 1

                ListView {
                    id: childSkillList
                    anchors.fill: parent
                    anchors.margins: theme.sp3
                    clip: true
                    spacing: theme.sp2
                    model: skillInstallManager.pendingChildSkills
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }
                    leftMargin: 1
                    rightMargin: 1
                    topMargin: 1
                    bottomMargin: 1

                    delegate: Rectangle {
                        required property var modelData
                        width: childSkillList.width
                        height: childSkillColumn.implicitHeight + theme.sp4
                        radius: theme.rMd
                        color: theme.surface
                        border.width: 1
                        border.color: theme.line

                        Column {
                            id: childSkillColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: theme.sp3
                            spacing: theme.sp2

                            Text {
                                width: parent.width
                                text: modelData.name && modelData.name.length > 0 ? modelData.name : modelData.skillId
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                            }

                            Text {
                                width: parent.width
                                text: modelData.skillId || ""
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                text: modelData.description && modelData.description.length > 0
                                      ? modelData.description
                                      : qsTr("No description")
                                color: theme.inkSoft
                                font.family: theme.fontBody
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }

                            ThemeButton {
                                width: 120
                                text: modelData.installed ? qsTr("Added") : qsTr("Add skill")
                                variant: modelData.installed ? "secondary" : "primary"
                                enabled: !modelData.installed
                                onClicked: skillInstallManager.completePendingChildInstall(modelData.relativePath || "")
                            }
                        }
                    }
                }
            }

            Row {
                width: parent.width
                layoutDirection: Qt.RightToLeft
                spacing: theme.sp2

                ThemeButton {
                    text: qsTr("Done")
                    onClicked: skillInstallManager.clearPendingChildSelection()
                }
            }
        }
    }

    Image {
        anchors.fill: parent
        visible: theme.wallpaperSource.length > 0 && theme.wallpaperMode !== "tile"
        source: theme.wallpaperSource
        fillMode: theme.wallpaperMode === "contain" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        opacity: theme.wallpaperOpacity
    }
    BorderImage {
        anchors.fill: parent
        visible: theme.wallpaperSource.length > 0 && theme.wallpaperMode === "tile"
        source: theme.wallpaperSource
        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat
        opacity: theme.wallpaperOpacity
    }
    // faint warm clay glow at the top for depth (no flat solid)
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.45
        visible: !theme.hasWallpaper
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Qt.rgba(0.76, 0.31, 0.16, 0.06) }
            GradientStop { position: 1.0; color: Qt.rgba(0.76, 0.31, 0.16, 0.00) }
        }
    }

    // first launch: pick the .starryagent directory.
    DirPromptView {
        id: dirPrompt
        anchors.fill: parent
        visible: config.firstLaunch
    }

    // main shell: sidebar + right pane (hidden on first launch)
    Item {
        id: shell
        anchors.fill: parent
        visible: !config.firstLaunch

        // Narrow viewport → sidebar becomes a slide-in drawer (overlay).
        // Wide viewport → sidebar is inline (occupies layout space).
        property bool isNarrow: win.width < 600
        property bool drawerOpen: false

        // Right-pane navigation. `destination` is a pure binding; `_navOverride`
        // is the imperative layer set by user actions and cleared to fall back
        // to the active-conversation-derived state. This makes Settings a peer
        // of Chat in the right layout — never an overlay. The three views are
        // mutually exclusive: picker | chat | settings.
        property var _navOverride: null
        property string destination: _navOverride !== null ? _navOverride
                                           : (frontendSessionStore.activeConversation ? "chat" : "picker")

        // Whenever a conversation becomes active (sidebar row click or
        // new-conv pick), drop the override so the right pane follows it.
        // On narrow viewports, also close the drawer — the user picked one.
        Connections {
            target: frontendSessionStore
            function onActiveConversationChanged() {
                if (frontendSessionStore.activeConversation) {
                    shell._navOverride = null
                    if (shell.isNarrow) shell.drawerOpen = false
                }
            }
        }

        // --- Content area ---
        // Full width on narrow (sidebar overlays on top), offset by sidebar
        // width on wide (sidebar is inline).
        Item {
            id: contentArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: shell.isNarrow ? 0 : 280

            Loader {
                anchors.fill: parent
                active: shell.destination === "chat"
                sourceComponent: ChatView {
                    isDemo: demoMode
                }
            }

            Loader {
                anchors.fill: parent
                active: shell.destination === "picker"
                sourceComponent: ModePicker {
                    onPicked: (modeId) => {
                        frontendSessionStore.openNewConversation(modeId)
                        shell._navOverride = null   // active now set -> binding drives "chat"
                    }
                }
            }

            Loader {
                anchors.fill: parent
                active: shell.destination === "settings"
                sourceComponent: SettingsView {
                }
            }
        }

        // --- Hamburger button (narrow only) ---
        // Top-left floating button to toggle the drawer.
        Item {
            id: hamburger
            visible: shell.isNarrow
            width: 40; height: 40
            x: theme.sp2
            y: theme.sp2
            z: 100
            Rectangle {
                anchors.fill: parent
                radius: theme.rSm
                color: hamburgerMa.containsMouse ? Qt.rgba(0, 0, 0, 0.06) : "transparent"
                border.color: theme.line
                border.width: 1
            }
            // three horizontal lines
            Column {
                anchors.centerIn: parent
                spacing: 4
                Repeater {
                    model: 3
                    Rectangle { width: 18; height: 2; radius: 1; color: theme.ink }
                }
            }
            MouseArea {
                id: hamburgerMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: shell.drawerOpen = !shell.drawerOpen
            }
        }

        // --- Edge swipe area (narrow only, drawer closed) ---
        // Invisible 24px strip on the left edge. A rightward swipe opens the
        // drawer; a simple tap does nothing (avoids accidental opens).
        MouseArea {
            id: edgeSwipe
            visible: shell.isNarrow && !shell.drawerOpen
            width: 24
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            z: 50
            property real startX: 0
            property real startY: 0
            onPressed: (mouse) => {
                startX = mouse.x
                startY = mouse.y
            }
            onPositionChanged: (mouse) => {
                var dx = mouse.x - startX
                var dy = Math.abs(mouse.y - startY)
                if (dx > 20 && dx > dy * 2)
                    shell.drawerOpen = true
            }
        }

        // --- Scrim (narrow only, drawer open) ---
        // Dark overlay between content and drawer. Tap to close.
        Rectangle {
            id: scrim
            visible: shell.isNarrow && shell.drawerOpen
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.35)
            z: 150
            MouseArea { anchors.fill: parent; onClicked: shell.drawerOpen = false }
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }

        // --- Sidebar ---
        // Wide: inline at x=0, z=0 (content offset by 280px).
        // Narrow: overlay drawer, slides from -width to 0.
        Loader {
            width: 280
            height: parent.height
            x: shell.isNarrow ? (shell.drawerOpen ? 0 : -width) : 0
            y: 0
            z: shell.isNarrow ? 200 : 0
            active: true
            sourceComponent: Sidebar {
                width: 280
                height: shell.height
                settingsActive: shell.destination === "settings"
                onRequestNewConversation: shell._navOverride = "picker"
                onRequestSettings: shell._navOverride = "settings"
                onRequestOpenConversation: {
                    shell._navOverride = null
                    if (shell.isNarrow)
                        shell.drawerOpen = false
                }
                onRequestClose: shell.drawerOpen = false
            }
            Behavior on x {
                NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                enabled: shell.isNarrow
            }
        }
    }

}
