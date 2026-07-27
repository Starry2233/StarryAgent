import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Sidebar — Doubao-style. 280px on desktop, collapses below 600px viewport.
// "新对话" pinned at top; conversation list grouped by recency
// (今天 / 昨天 / 7天内 / 更早); settings pinned at the bottom.
Item {
    id: root

    property bool collapsed: false   // retained for API compat; no longer controls surface visibility
    property bool settingsActive: false  // driven by main's destination state
    // right-click target + inline-rename target id; mobile branch flag
    property var targetConv: null
    property string editingId: ""
    property string editingText: ""   // live text of the field being renamed
    property bool isMobile: Qt.platform.os === "android"
    // True while the conversation list is moving (drag or flick deceleration).
    // Used to suppress hover highlights on cards under the cursor during scroll.
    property bool scrolling: list.moving

    signal requestNewConversation()
    signal requestSettings()
    signal requestOpenConversation()
    signal requestClose()   // drawer mode: ask the parent to close the drawer

    // --- Context menu (Rename / Delete / Properties) ---
    ThemedMenu {
        id: ctxMenu
        minMenuWidth: 168
        items: [qsTr("重命名"), qsTr("删除"), qsTr("属性")]
        onTriggered: function(index) {
            if (!root.targetConv)
                return
            if (index === 0)
                root.editingId = root.targetConv.id
            else if (index === 1)
                root.requestDelete(root.targetConv)
            else if (index === 2)
                root.requestProperties(root.targetConv)
        }
    }

    // --- Desktop dialogs (modal windows). Mobile uses the overlays below. ---
    ConfirmDialog {
        id: deleteDlg
        onConfirmed: if (root.targetConv) {
            const c = root.targetConv
            root.targetConv = null
            conversations.remove(c)
        }
    }
    PropertiesDialog {
        id: propsDlg
    }

    // --- Mobile overlays (scrim + card centered on the full screen) ---
    // Reparented to the window's contentItem so the scrim covers the whole
    // screen and the cards center on it — not just the 280px sidebar drawer.
    Rectangle {
        id: mScrim
        parent: Window.window ? Window.window.contentItem : root
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.35)
        visible: root.isMobile && (mConfirm.visible || mProps.visible)
        MouseArea { anchors.fill: parent; onClicked: {} }   // block backdrop taps
        z: 100
    }
    Rectangle {
        id: mConfirm
        property bool visible2: false
        parent: Window.window ? Window.window.contentItem : root
        anchors.centerIn: parent
        width: 320; height: 150
        radius: theme.rLg
        color: theme.surface
        border.color: theme.line; border.width: 1
        visible: root.isMobile && visible2
        z: 101
        Text {
            id: mConfirmMsg
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.margins: 20
            wrapMode: Text.Wrap
            color: theme.ink; font.family: theme.fontBody; font.pixelSize: 13
        }
        Row {
            anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 20
            spacing: 12
            Button {
                text: qsTr("取消")
                contentItem: Text { text: parent.text; color: theme.ink; font.family: theme.fontBody; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? Qt.rgba(0,0,0,0.06) : "transparent"; border.color: theme.line; border.width: 1; radius: theme.rPill; implicitWidth: 64; implicitHeight: 32 }
                onClicked: mConfirm.visible2 = false
            }
            Button {
                text: qsTr("删除")
                contentItem: Text { text: parent.text; color: "white"; font.family: theme.fontBody; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? theme.clayDeep : theme.clay; radius: theme.rPill; implicitWidth: 64; implicitHeight: 32 }
                onClicked: {
                    mConfirm.visible2 = false
                    if (root.targetConv) { const c = root.targetConv; root.targetConv = null; conversations.remove(c) }
                }
            }
        }
    }
    Rectangle {
        id: mProps
        property bool visible2: false
        parent: Window.window ? Window.window.contentItem : root
        anchors.centerIn: parent
        width: 340
        height: mPropsCol.implicitHeight + 40   // auto-fit to content
        radius: theme.rLg
        color: theme.surface
        border.color: theme.line; border.width: 1
        visible: root.isMobile && visible2
        z: 101
        Column {
            id: mPropsCol
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 20; spacing: 10
            Text { text: qsTr("对话属性"); color: theme.ink; font.family: theme.fontDisplay; font.pixelSize: 15; font.weight: Font.Medium }
            Repeater {
                model: [
                    { label: qsTr("标题"), value: root.targetConv ? root.targetConv.title : "" },
                    { label: qsTr("模式"), value: root.targetConv ? root.targetConv.modeId : "" },
                    { label: qsTr("ID"),   value: root.targetConv ? root.targetConv.id : "" },
                    { label: qsTr("创建"), value: root.targetConv ? Qt.formatDateTime(root.targetConv.created, "yyyy-MM-dd HH:mm") : "" },
                    { label: qsTr("更新"), value: root.targetConv ? Qt.formatDateTime(root.targetConv.updated, "yyyy-MM-dd HH:mm") : "" }
                ]
                delegate: Column {
                    width: parent.width; spacing: 1
                    Text { text: modelData.label; color: theme.inkSoft; font.family: theme.fontBody; font.pixelSize: 9; font.letterSpacing: 1; font.capitalization: Font.AllUppercase }
                    Text { width: parent.width; text: modelData.value; color: theme.ink; font.family: theme.fontMono; font.pixelSize: 11; wrapMode: Text.Wrap }
                }
            }
            Button {
                text: qsTr("关闭"); anchors.right: parent.right
                contentItem: Text { text: parent.text; color: "white"; font.family: theme.fontBody; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? theme.clayDeep : theme.clay; radius: theme.rPill; implicitWidth: 64; implicitHeight: 32 }
                onClicked: mProps.visible2 = false
            }
        }
    }

    // Route delete/properties to the platform-appropriate surface.
    function requestDelete(conv) {
        root.targetConv = conv
        const msg = qsTr("删除对话「%1」？此操作不可撤销。").arg(conv ? conv.title : "")
        if (root.isMobile) { mConfirmMsg.text = msg; mConfirm.visible2 = true }
        else deleteDlg.open(msg)
    }
    function requestProperties(conv) {
        root.targetConv = conv
        if (root.isMobile) mProps.visible2 = true
        else propsDlg.open(conv)
    }
    function openConversationMenu(item, localX, localY) {
        const host = Window.window && Window.window.contentItem ? Window.window.contentItem : root
        const p = item.mapToItem(host, localX, localY)
        const margin = 8
        const menuWidth = Math.max(ctxMenu.implicitWidth, ctxMenu.width)
        const menuHeight = Math.max(ctxMenu.implicitHeight, ctxMenu.height)
        ctxMenu.parent = host
        ctxMenu.x = Math.max(margin, Math.min(p.x, host.width - menuWidth - margin))
        ctxMenu.y = Math.max(margin, Math.min(p.y, host.height - menuHeight - margin))
        ctxMenu.open()
    }
    // Commit an inline rename using the live editingText. Called by the field
    // (Enter/blur) and by outside click handlers (another row, 新对话, 设置).
    // Empty text cancels. No-op if not editing.
    function commitRename() {
        if (root.editingId === "") return
        const convId = root.editingId
        root.editingId = ""
        const t = root.editingText.trim()
        root.editingText = ""
        if (t.length === 0) return
        const list = conversations.conversations
        for (let i = 0; i < list.length; i++) {
            if (list[i].id === convId) {
                conversations.rename(list[i], t)
                break
            }
        }
    }

    // Flat mirror of conversations.conversations with a derived `bucket` role
    // for ListView section grouping. Rebuilt by replacing the whole JS array
    // so ListView does not observe a burst of incremental remove/append ops.
    property var rowsData: []

    function bucketOf(updated) {
        // updated is a JS Date (from QDateTime). Bucket by calendar day diff.
        const now = new Date()
        const u = new Date(updated)
        if (u.getFullYear() === now.getFullYear() && u.getMonth() === now.getMonth() && u.getDate() === now.getDate())
            return qsTr("今天")
        const yesterday = new Date(now)
        yesterday.setDate(now.getDate() - 1)
        if (u.getFullYear() === yesterday.getFullYear() && u.getMonth() === yesterday.getMonth() && u.getDate() === yesterday.getDate())
            return qsTr("昨天")
        const days = Math.floor((now - u) / 86400000)
        if (days <= 7) return qsTr("7天内")
        return qsTr("更早")
    }

    function relTime(updated) {
        const u = new Date(updated)
        const now = new Date()
        const mins = Math.floor((now - u) / 60000)
        if (mins < 1) return qsTr("刚刚")
        if (mins < 60) return qsTr("%1分钟前").arg(mins)
        const hrs = Math.floor(mins / 60)
        if (hrs < 24) return qsTr("%1小时前").arg(hrs)
        const d = u
        return Qt.formatDateTime(d, "MM-dd HH:mm")
    }

    function rebuild() {
        const nextRows = []
        const list = conversations.conversations
        for (let i = 0; i < list.length; i++) {
            const c = list[i]
            nextRows.push({
                convId: c.id,
                title: c.title,
                bucket: bucketOf(c.updated),
                rel: relTime(c.updated),
                active: conversations.active && conversations.active.id === c.id
            })
        }
        rowsData = nextRows
    }

    Component.onCompleted: rebuild()
    Connections {
        target: conversations
        function onConversationsChanged() { rebuild() }
        function onActiveChanged() { rebuild() }
    }

    // surface
    Rectangle {
        id: surface
        anchors.fill: parent
        color: theme.surfaceAlt

        // Block hover/clicks from passing through sidebar gaps to content
        // behind the drawer (e.g. the hamburger button shining through).
        // First child → lowest z; button MouseAreas above still get their
        // own hover/clicks. Only catches the margins/spacing between them.
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
        }

        // hairline on the right edge
        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 1
            color: theme.line
        }

        Column {
            anchors.fill: parent
            anchors.margins: theme.sp3
            spacing: theme.sp2

            // brand row
            Item {
                id: brandRow
                width: parent.width
                height: 40
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: theme.sp2
                    text: "\u2726\uFE0E"
                    color: theme.clay
                    font.family: theme.fontDisplay
                    font.pixelSize: 20
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: theme.sp4 + 12
                    text: "StarryAgent"
                    color: theme.ink
                    font.family: theme.fontDisplay
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    font.letterSpacing: -0.3
                }
            }

            // 新对话 button
            Rectangle {
                id: newDialog
                width: parent.width
                height: 40
                radius: theme.rMd
                color: (!root.scrolling && newDialogMa.containsMouse) ? theme.accent(0.10) : theme.accent(0.06)
                border.color: theme.accent(0.20)
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    text: "+  " + qsTr("新对话")
                    color: theme.clayDeep
                    font.family: theme.fontBody
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }
                MouseArea {
                    id: newDialogMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.editingId !== "") { root.commitRename(); return }
                        root.requestNewConversation()
                    }
                }
            }

            // conversation list with section grouping
            ListView {
                id: list
                width: parent.width
                height: parent.height - brandRow.height - newDialog.height - settingsBtn.height - parent.spacing * 4
                clip: true
                model: root.rowsData
                currentIndex: -1
                section.property: "bucket"
                section.delegate: Rectangle {
                    width: parent ? parent.width : 0
                    height: 32
                    color: "transparent"

                    // Decorative hairline above section label
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        height: 1
                        color: theme.line
                        opacity: 0.4
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        leftPadding: theme.sp2
                        bottomPadding: 4
                        text: section
                        color: theme.inkSoft
                        font.family: theme.fontBody
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.8
                        font.pixelSize: 10
                        font.capitalization: Font.AllUppercase
                    }
                }
                section.criteria: ViewSection.FullString

                delegate: Rectangle {
                    property var row: modelData || ({})
                    // A row is "active" only when its conversation is the
                    // current destination — NOT while settings is showing,
                    // so the gear and a conversation never highlight together.
                    property bool isActive: !root.settingsActive
                        && conversations.active
                        && conversations.active.id === row.convId
                    width: parent ? parent.width : 0
                    height: 44
                    radius: theme.rSm
                    color: isActive ? theme.accent(0.10)
                                     : ((!root.scrolling && rowMa.containsMouse) ? Qt.rgba(0, 0, 0, 0.03) : "transparent")
                    Behavior on color { ColorAnimation { duration: 150 } }
                    MouseArea {
                        // Below the Column so the rename TextField (when visible)
                        // actually receives clicks to position the cursor.
                        id: rowMa
                        anchors.fill: parent
                        z: 0
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        // True after a long-press; suppresses the click that
                        // follows release so the context menu doesn't also
                        // select the conversation.
                        property bool held: false
                        onPressAndHold: (mouse) => {
                            held = true
                            if (root.editingId !== "") { root.commitRename(); return }
                            const c = conversations.conversations[index]
                            if (!c) return
                            root.targetConv = c
                            root.openConversationMenu(rowMa, mouse.x, mouse.y)
                        }
                        onClicked: (mouse) => {
                            if (held) { held = false; return }
                            // Any click while a rename is open commits it first
                            // and consumes this click (don't also select).
                            if (root.editingId !== "") {
                                root.commitRename()
                                return
                            }
                            const c = conversations.conversations[index]
                            if (!c) return
                            if (mouse.button === Qt.RightButton) {
                                root.targetConv = c
                                root.openConversationMenu(rowMa, mouse.x, mouse.y)
                            } else {
                                root.requestOpenConversation()
                                conversations.active = c
                            }
                        }
                    }
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: theme.sp3
                        anchors.rightMargin: theme.sp3
                        spacing: 1
                        z: 1   // above rowMa so the field is clickable

                        // Inline rename: a TextField replaces the title while
                        // editingId matches this row. Enter/blur commits, Esc cancels.
                        TextField {
                            id: renameField
                            visible: row.convId === root.editingId
                            width: parent.width
                            text: row.title || ""
                            color: theme.ink
                            font.family: theme.fontBody
                            font.pixelSize: 13
                            font.weight: isActive ? Font.Medium : Font.Normal
                            cursorDelegate: ClayCursor {}
                            background: Rectangle {
                                color: theme.paper
                                radius: theme.rSm
                                border.color: theme.clay
                                border.width: 1
                            }
                            onVisibleChanged: if (visible) {
                                root.editingText = row.title || ""
                                forceActiveFocus()
                                selectAll()
                            }
                            onTextChanged: root.editingText = text
                            onAccepted: root.commitRename()
                            onActiveFocusChanged: if (!activeFocus) root.commitRename()
                            Keys.onEscapePressed: {
                                root.editingId = ""
                                root.editingText = ""
                            }
                        }
                        Text {
                            width: parent.width
                            visible: row.convId !== root.editingId
                            text: row.title || qsTr("(untitled)")
                            color: theme.ink
                            font.family: theme.fontBody
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            font.weight: isActive ? Font.Medium : Font.Normal
                        }
                        Text {
                            text: row.rel || ""
                            visible: row.convId !== root.editingId
                            color: theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 10
                        }
                    }
                }
            }

            // spacer pushes settings to the bottom
            Item {
                width: parent.width
                height: parent.height - list.y - list.height - settingsBtn.height - theme.sp2
            }

            // settings button pinned at the bottom
            Rectangle {
                id: settingsBtn
                width: parent.width
                height: 38
                radius: theme.rSm
                color: root.settingsActive ? theme.accent(0.10)
                     : ((!root.scrolling && settingsMa.containsMouse) ? Qt.rgba(0, 0, 0, 0.04) : "transparent")
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: theme.sp3
                    text: Qt.platform.os === "windows" ? "\uE713" : "\u2699\uFE0E"
                    color: root.settingsActive ? theme.clay : theme.inkSoft
                    font.family: Qt.platform.os === "windows" ? "Segoe MDL2 Assets" : theme.fontBody
                    font.pixelSize: Qt.platform.os === "windows" ? 17 : 16
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    leftPadding: theme.sp3 + 24
                    text: qsTr("设置")
                    color: root.settingsActive ? theme.clayDeep : theme.ink
                    font.family: theme.fontBody
                    font.pixelSize: 13
                    font.weight: root.settingsActive ? Font.Medium : Font.Normal
                }
                MouseArea {
                    id: settingsMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.editingId !== "") { root.commitRename(); return }
                        root.requestSettings()
                    }
                }
            }
        }
    }

    // Drag-to-close handle: a thin strip on the right edge. A leftward drag
    // (dx < -20, mostly horizontal) fires requestClose so the parent can
    // animate the drawer shut. Sits above the surface but below the mobile
    // overlays, and only intercepts drags — clicks pass through to the list.
    MouseArea {
        id: dragHandle
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 16
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
            if (dx < -20 && Math.abs(dx) > dy * 2)
                root.requestClose()
        }
    }
}
