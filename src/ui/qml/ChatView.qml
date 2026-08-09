import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "components" as Components

// ChatView — binds to the active Conversation (a QAbstractListModel). Streams
// assistant text, shows ToolCallCards inline, and closes the tool loop by
// re-sending with tool results appended. Centered chat column per CLAUDE.md;
// assistant text sits on the background with a star avatar to the left (no
// bubble); user messages are a soft right-aligned bubble.
Item {
    id: root

    property bool isDemo: false
    property var active: conversations.active   // the Conversation* (or null)
    property var pendingImagePaths: []
    property bool imagePreviewVisible: false
    property string imagePreviewSource: ""
    property string imagePreviewName: ""
    property bool textSelectionVisible: false
    property string textSelectionValue: ""
    property string textSelectionTitle: ""
    property var commandSuggestions: []
    property int commandSelectionIndex: 0
    readonly property bool isAndroid: Qt.platform.os === "android"
    readonly property bool commandMode: inputField.text.length > 0 && inputField.text.charAt(0) === "/"
    readonly property bool commandMenuVisible: commandMode && commandSuggestions.length > 1
    readonly property string commandQuery: commandMode ? inputField.text.trim() : ""
    readonly property var slashCommands: [
        { name: "/help", usage: "/help", description: qsTr("Show available commands.") },
        { name: "/compact", usage: "/compact [instructions]", description: qsTr("Compact earlier history into a handoff summary.") },
        { name: "/model", usage: "/model [name]", description: qsTr("List or switch the active model.") },
        { name: "/plan", usage: "/plan [note|show|clear|open|ready|approve|reject|off]", description: qsTr("Enter plan mode, submit a plan, or approve it.") },
        { name: "/workdir", usage: "/workdir [path|reset]", description: qsTr("Show or change the default working directory for this conversation.") }
    ]

    function vlog(message) {
        if (typeof verboseLogging !== "undefined" && verboseLogging)
            console.log("[ChatView] " + message)
    }

    onActiveChanged: root.requestInitialBottomAnchor()

    function requestInitialBottomAnchor() {
        if (!list)
            return
        if (!active) {
            list.initialBottomRestore = false
            list.waitingForInitialRows = false
            list.initialRestoreSettledTimer.stop()
            return
        }
        if (list.followScrollAnim)
            list.followScrollAnim.stop()
        if (list.followScrollTimer)
            list.followScrollTimer.stop()
        if (list.initialRestoreSettledTimer)
            list.initialRestoreSettledTimer.stop()
        list.autoScroll = true
        list.initialRestoreRevision += 1
        list.waitingForInitialRows = true
        list.initialBottomRestore = list.count > 0
        if (list.initialBottomRestore)
            list.restoreInitialBottom(list.initialRestoreRevision)
    }

    function canAddMoreImages() {
        return pendingImagePaths.length < 5
    }

    function addPendingImage(path) {
        if (!path || path.length === 0)
            return
        if (pendingImagePaths.length >= 5)
            return
        pendingImagePaths = pendingImagePaths.concat([path])
    }

    function removePendingImageAt(index) {
        const next = pendingImagePaths.slice()
        next.splice(index, 1)
        pendingImagePaths = next
    }

    function fileUrl(path) {
        if (!path || path.length === 0)
            return ""
        let normalized = String(path)
        normalized = normalized.replace(/\\/g, "/")
        if (normalized.startsWith("file://"))
            return normalized
        if (normalized.startsWith("/"))
            return "file://" + normalized
        return "file:///" + normalized
    }

    function send(text) {
        if ((!text.trim() && pendingImagePaths.length === 0) || !root.active) return
        if (executeSlashCommand(text.trim())) {
            pendingImagePaths = []
            conversations.saveActive()
            return
        }
        if (list) {
            list.enableAutoScroll("send")
            list.scheduleFollowScroll(false)
        }
        root.active.sendWithImages(text, pendingImagePaths)
        pendingImagePaths = []
        conversations.saveActive()
    }

    function selectedCommand() {
        if (commandSelectionIndex < 0 || commandSelectionIndex >= commandSuggestions.length)
            return null
        return commandSuggestions[commandSelectionIndex]
    }

    function rebuildCommandSuggestions() {
        if (!commandMode) {
            commandSuggestions = []
            commandSelectionIndex = 0
            return
        }

        const raw = commandQuery
        const parts = raw.split(/\s+/)
        const head = parts[0].toLowerCase()
        const arg = raw.length > head.length ? raw.substring(head.length).trim() : ""
        let next = []

        if (parts.length <= 1 && !raw.endsWith(" ")) {
            const needle = head.length > 0 ? head : "/"
            next = slashCommands.filter(function(cmd) {
                return cmd.name.indexOf(needle) === 0
            }).map(function(cmd) {
                return {
                    kind: "command",
                    label: cmd.name,
                    insertText: cmd.name === "/model" ? "/model " : cmd.usage,
                    description: cmd.description
                }
            })
        } else if (head === "/model") {
            next = settings.models.filter(function(model) {
                return arg.length === 0 || model.toLowerCase().indexOf(arg.toLowerCase()) >= 0
            }).map(function(model) {
                return {
                    kind: "model",
                    label: model,
                    insertText: "/model " + model,
                    description: qsTr("Switch to %1").arg(model)
                }
            })
        }

        commandSuggestions = next
        if (commandSelectionIndex >= next.length)
            commandSelectionIndex = 0
    }

    function acceptCommandSuggestion(index) {
        if (index < 0 || index >= commandSuggestions.length)
            return
        const item = commandSuggestions[index]
        inputField.text = item.insertText
        inputField.cursorPosition = inputField.text.length
        commandSelectionIndex = index
        rebuildCommandSuggestions()
    }

    function executeSlashCommand(text) {
        if (!text || text.length === 0 || text.charAt(0) !== "/")
            return false
        const parts = text.split(/\s+/)
        const command = parts[0].toLowerCase()
        const arg = text.length > command.length ? text.substring(command.length).trim() : ""

        if (command === "/help") {
            root.active.appendAssistantText("/help\n/compact [instructions]\n/model [name]\n/plan [note|show|clear|open|ready|approve|reject|off]\n/workdir [path|reset]")
            return true
        }
        if (command === "/compact") {
            root.active.appendAssistantText(root.active.compactNow(arg))
            return true
        }
        if (command === "/model") {
            if (!arg || arg.length === 0) {
                root.active.appendAssistantText(qsTr("Current model: %1\nAvailable models:\n%2")
                                                .arg(settings.model, settings.models.join("\n")))
                return true
            }
            settings.model = arg
            root.active.appendAssistantText(qsTr("Switched model to %1").arg(settings.model))
            return true
        }
        if (command === "/plan") {
            if (!arg || arg.length === 0) {
                root.active.appendAssistantText(root.active.enterPlanMode())
                return true
            }
            if (arg === "ready") {
                root.active.appendAssistantText(root.active.submitPlanForApproval())
                return true
            }
            if (arg === "approve") {
                root.active.appendAssistantText(root.active.approvePlan())
                return true
            }
            if (arg === "reject") {
                root.active.appendAssistantText(root.active.rejectPlan())
                return true
            }
            if (arg.startsWith("reject ")) {
                root.active.appendAssistantText(root.active.rejectPlan(arg.substring(7).trim()))
                return true
            }
            if (arg === "off") {
                root.active.appendAssistantText(root.active.exitPlanMode())
                return true
            }
            if (arg === "show") {
                root.active.appendAssistantText(root.active.showPlan())
                return true
            }
            if (arg === "clear") {
                root.active.appendAssistantText(root.active.clearPlan())
                return true
            }
            if (arg === "open") {
                root.active.appendAssistantText(root.active.openPlan())
                return true
            }
            root.active.appendAssistantText(root.active.enterPlanMode(arg))
            return true
        }
        if (command === "/workdir") {
            if (!arg || arg.length === 0) {
                root.active.appendAssistantText(root.active.showWorkdir())
                return true
            }
            if (arg === "reset") {
                root.active.appendAssistantText(root.active.resetWorkdir())
                return true
            }
            root.active.appendAssistantText(root.active.setWorkdir(arg))
            return true
        }

        root.active.appendAssistantText(qsTr("Unknown command: %1").arg(command))
        return true
    }

    function imageDisplayName(source, fallback) {
        if (fallback && fallback.length > 0)
            return fallback
        if (!source || source.length === 0)
            return ""
        let normalized = String(source)
        const qmark = normalized.indexOf("?")
        if (qmark >= 0)
            normalized = normalized.substring(0, qmark)
        normalized = normalized.replace(/\\/g, "/")
        const parts = normalized.split("/")
        return parts.length > 0 ? parts[parts.length - 1] : ""
    }

    function openImagePreview(source, name) {
        if (!source || source.length === 0)
            return
        source = String(source)
        const finalName = imageDisplayName(source, name)
        if (root.isAndroid) {
            root.imagePreviewSource = source
            root.imagePreviewName = finalName
            root.imagePreviewVisible = true
        } else {
            imageWindow.openFor(source, finalName)
        }
    }

    function requestImageDownload(source, name) {
        source = String(source)
        imageTransfer.download(source, imageDisplayName(source, name))
    }

    function openTextSelection(title, text) {
        const value = String(text || "")
        if (value.length === 0)
            return
        if (!root.isAndroid) {
            textSelectionWindow.openFor(title || "", value)
            return
        }
        textSelectionTitle = title || ""
        textSelectionValue = value
        textSelectionVisible = true
    }

    function estimatedRowHeight(row) {
        if (!row)
            return 40
        if (row.kind === "tool") {
            let h = 44
            if ((row.status === "done" || row.status === "error") && row.result && row.result.length > 0)
                h += 22
            if (row.argsText && row.argsText.length > 0)
                h += 56
            return h
        }
        if (row.kind === "user") {
            const textLen = row.text ? row.text.length : 0
            const lineEstimate = Math.max(1, Math.ceil(textLen / 28))
            const imageCount = row.imagePaths ? row.imagePaths.length : 0
            return 24 + Math.min(8, lineEstimate) * 20 + imageCount * 168
        }
        if (row.kind === "assistant") {
            const visibleText = filterThink(row.text || "")
            if (visibleText.length === 0)
                return theme.sp4
            const lineEstimate = Math.max(1, Math.ceil(visibleText.length / 42))
            return 20 + Math.min(20, lineEstimate) * 21
        }
        if (row.kind === "compact") {
            return 34
        }
        return 40
    }

    // Display-only filter: hide reasoning-model thinking blocks (<think>…</think>)
    // from the chat UI. The raw text (with tags) is still persisted and sent to
    // the API — this is UI-level only, per the user's "ui层面隐藏" instruction.
    //   - complete <think>…</think> span → stripped entirely
    //   - trailing unclosed <think> (still streaming) → hides the rest until
    //     </think> arrives
    //   - standalone </think> with no preceding <think> (opening tag was in an
    //     earlier chunk) → hide everything before & including </think>
    function filterThink(raw) {
        if (!raw || raw.length === 0) return ""
        const OPEN = "<think>"
        const CLOSE = "</think>"
        let out = ""
        let i = 0
        while (i < raw.length) {
            const open = raw.indexOf(OPEN, i)
            if (open === -1) {
                // No more opening tags — keep the rest, but sweep any stray
                // closing-tag leftovers (model emitted /think with no matching
                // think opener) out of the visible text so they don't leak.
                out += raw.substring(i).split(CLOSE).join("")
                break
            }
            out += raw.substring(i, open)
            const close = raw.indexOf(CLOSE, open + OPEN.length)
            if (close === -1) break   // opener still open (streaming) — hide rest
            i = close + CLOSE.length
        }
        return out
    }

    Component.onCompleted: {
        vlog("completed demo=" + root.isDemo + " hasActive=" + !!root.active)
        if (root.isDemo) {
            // seed a fake user turn for the scripted assistant reply
            if (root.active) root.active.appendUser("Write a test file for me.")
        }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.pageOverlay
    }

    Loader {
        anchors.fill: parent
        active: Qt.platform.os !== "android"
        sourceComponent: Components.PaperGrain {
            dark: theme.dark
            intensity: 0.018
        }
    }

    Loader {
        id: cameraWindowLoader
        active: false
        sourceComponent: CameraCaptureWindow {
            dark: theme.dark
            onCaptured: function(path) {
                if (!root.active || !path || path.length === 0)
                    return
                const imported = root.active.importImage(path)
                if (imported && imported.length > 0)
                    root.addPendingImage(imported)
            }
        }
    }

    Connections {
        target: cameraBridge
        function onCaptured(path) {
            if (!root.active || !path || path.length === 0)
                return
            const imported = root.active.importImage(path)
            if (imported && imported.length > 0)
                root.addPendingImage(imported)
        }
        function onErrorOccurred(message) {
            if (message && message.length > 0)
                toast.showMessage(message)
        }
        function onPermissionRequestLaunched(message) {
            if (message && message.length > 0)
                toast.showMessage(message)
        }
    }

    Connections {
        target: filePicker
        function onImagesPicked(paths) {
            if (!root.active || !paths)
                return
            for (let i = 0; i < paths.length && root.pendingImagePaths.length < 5; ++i) {
                const imported = root.active.importImage(paths[i])
                if (imported && imported.length > 0)
                    root.addPendingImage(imported)
            }
        }
        function onErrorOccurred(message) {
            if (message && message.length > 0 && message !== "Image selection cancelled")
                toast.showMessage(message)
        }
    }

    ImagePreviewWindow {
        id: imageWindow
        dark: theme.dark
        onDownloadRequested: function(source, name) {
            root.requestImageDownload(source, name)
        }
    }

    TextSelectionWindow {
        id: textSelectionWindow
        dark: theme.dark
    }

    Connections {
        target: imageTransfer
        function onDownloadFinished(source, savedPath, error) {
            // Download feedback is emitted from C++ so Android does not depend on
            // QML->JNI toast delivery.
        }
    }

    // centered chat column (CLAUDE.md: min(available, max-width))
    Item {
        id: column
        anchors.top: parent.top
        anchors.bottom: inputBar.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - theme.sp5 * 2, 868)
        anchors.topMargin: theme.sp4
        anchors.bottomMargin: theme.sp4

        ListView {
            id: list
            anchors.fill: parent
            clip: true
            interactive: true   // ensure wheel + drag scrolling work
            spacing: theme.sp3
            // Markdown delegates are expensive (rich text, tables and code
            // blocks). A small look-ahead keeps flick scrolling smooth without
            // prebuilding several full screens of history.
            cacheBuffer: Math.max(320, height * 0.5)
            model: root.active   // Conversation* (QAbstractListModel) or null
            currentIndex: -1

            // Auto-scroll: stay pinned to the bottom while the assistant
            // produces content. If the user scrolls up, auto-scroll breaks
            // (so they can read history); scrolling back to the bottom resumes.
            property bool autoScroll: true
            // History restoration must start from the tail.  Chasing contentHeight
            // while lazy delegates are materialized creates a feedback loop on long
            // conversations and can leave the view just above the last message.
            property bool waitingForInitialRows: false
            property bool initialBottomRestore: false
            property int initialRestoreRevision: 0
            property int initialRestoreTimerRevision: 0
            property bool suppressAutoScrollTracking: false
            readonly property real _bottomSlack: 40   // px tolerance for "at bottom"
            readonly property bool shouldFollowAssistant: root.active
                && root.active.streaming
            readonly property bool shouldKeepBottomPinned: autoScroll && root.active
                && (root.active.streaming || pendingImagePaths.length > 0)
            property bool pendingAnimatedFollow: false

            function targetBottomY() {
                const origin = Number.isFinite(originY) ? originY : 0
                const extent = Number.isFinite(contentHeight) ? contentHeight : 0
                const viewport = Number.isFinite(height) ? height : 0
                return origin + Math.max(0, extent - viewport)
            }

            function atBottom() {
                if (!Number.isFinite(contentY) || !Number.isFinite(originY)
                        || !Number.isFinite(contentHeight) || !Number.isFinite(height))
                    return false
                return contentY + height >= originY + contentHeight - _bottomSlack
            }

            function enableAutoScroll(reason) {
                autoScroll = true
                followScrollTimer.stop()
                if (followScrollAnim.running)
                    followScrollAnim.stop()
                root.vlog("enableAutoScroll reason=" + reason)
            }

            function restoreInitialBottom(revision) {
                if (revision !== initialRestoreRevision
                        || !initialBottomRestore || !root.active || count <= 0) {
                    initialBottomRestore = false
                    return
                }
                followScrollAnim.stop()
                // This also puts the ListView's own delegate window at the tail
                // before its first frame, so the row loaders never walk history
                // from index zero just to restore a conversation.
                positionViewAtEnd()
                forceLayout()
                positionViewAtEnd()
                queueInitialBottomSettle(revision)
            }

            function queueInitialBottomSettle(revision) {
                if (revision !== initialRestoreRevision || !initialBottomRestore)
                    return
                initialRestoreTimerRevision = revision
                initialRestoreSettledTimer.restart()
            }

            function scheduleFollowScroll() {
                if (!autoScroll || !root.active || initialBottomRestore)
                    return
                if (!followScrollTimer.running)
                    followScrollTimer.start()
            }

            // a new row appeared → pin to bottom if auto (covers count changes
            // that onContentHeightChanged might miss on some Qt versions)
            onCountChanged: {
                if (waitingForInitialRows && root.active && count > 0) {
                    initialBottomRestore = true
                    restoreInitialBottom(initialRestoreRevision)
                }
                if (autoScroll || shouldKeepBottomPinned)
                    scheduleFollowScroll(false)
            }
            // Detect the user scrolling away from / back to the bottom.
            // positionViewAtEnd also fires this, but it lands at the bottom so
            // autoScroll stays true — no feedback loop.
            onContentYChanged: {
                if (!Number.isFinite(contentY) || !Number.isFinite(contentHeight)
                        || !Number.isFinite(originY) || contentHeight <= 0
                        || suppressAutoScrollTracking)
                    return
                autoScroll = atBottom()
            }
            onContentHeightChanged: {
                if (initialBottomRestore) {
                    // contentHeight has already been updated by the completed
                    // Markdown layout. Pin cheaply here; forceLayout for every
                    // rich-text segment is what previously caused the hitching.
                    suppressAutoScrollTracking = true
                    contentY = targetBottomY()
                    suppressAutoScrollTracking = false
                    queueInitialBottomSettle(initialRestoreRevision)
                }
                if (autoScroll || shouldKeepBottomPinned)
                    scheduleFollowScroll(true)
            }
            Component.onCompleted: {
                root.vlog("list completed width=" + width)
                root.requestInitialBottomAnchor()
            }
            onModelChanged: {
                root.vlog("model changed hasActive=" + !!root.active)
                root.requestInitialBottomAnchor()
            }

            Connections {
                target: root.active
                ignoreUnknownSignals: true
                function onStreamingChanged() {
                    if (list.initialBottomRestore)
                        return
                    if (root.active && root.active.streaming) {
                        list.enableAutoScroll("streaming")
                        list.scheduleFollowScroll(false)
                    }
                }
                function onRowsInserted() {
                    if (list.initialBottomRestore)
                        return
                    if (list.autoScroll || list.shouldKeepBottomPinned)
                        list.scheduleFollowScroll(false)
                }
                function onDataChanged() {
                    if (list.initialBottomRestore)
                        return
                    if (list.autoScroll || list.shouldKeepBottomPinned)
                        list.scheduleFollowScroll(true)
                }
            }
            onMovementStarted: {
                if (moving) {
                    followScrollAnim.stop()
                    followScrollTimer.stop()
                    if (flicking || dragging)
                        autoScroll = false
                }
            }
            onMovementEnded: {
                if (contentHeight <= 0)
                    return
                autoScroll = atBottom()
                if (autoScroll && shouldKeepBottomPinned)
                    scheduleFollowScroll(false)
            }

            NumberAnimation {
                id: followScrollAnim
                target: list
                property: "contentY"
                duration: 110
                easing.type: Easing.OutCubic
                onStopped: list.suppressAutoScrollTracking = false
            }

            Timer {
                id: followScrollTimer
                interval: 32
                repeat: false
                running: false

                onTriggered: {
                    if (!list.autoScroll || !root.active || list.initialBottomRestore)
                        return
                    const target = list.targetBottomY()
                    if (!Number.isFinite(target) || !Number.isFinite(list.contentY))
                        return
                    if (Math.abs(target - list.contentY) < 1)
                        return
                    list.suppressAutoScrollTracking = true
                    list.contentY = target
                    list.suppressAutoScrollTracking = false
                }
            }

            Timer {
                id: initialRestoreSettledTimer
                interval: 160
                repeat: false
                running: false
                onTriggered: {
                    if (list.initialRestoreTimerRevision !== list.initialRestoreRevision
                            || !list.initialBottomRestore || !root.active)
                        return
                    // Do exactly one final layout after the Markdown tree has
                    // been quiet, then align with all measured row heights.
                    list.forceLayout()
                    list.positionViewAtEnd()
                    list.initialBottomRestore = false
                    list.waitingForInitialRows = false
                }
            }

            delegate: Item {
                id: rowDelegate
                width: list.width
                readonly property real estimatedHeight: root.estimatedRowHeight(rowData)
                height: rowLoader.item
                    ? Math.max(0, rowLoader.item.implicitHeight)
                    : estimatedHeight
                readonly property var rowData: model
                readonly property int rowIndex: index

                function syncLoadedItem() {
                    if (!rowLoader.item)
                        return
                    rowLoader.item.rowData = Qt.binding(function() { return rowDelegate.rowData })
                    rowLoader.item.rowIndex = Qt.binding(function() { return rowDelegate.rowIndex })
                }

                Component.onCompleted: root.vlog("delegate create index=" + index
                                                  + " kind=" + model.kind
                                                  + " textLen=" + ((model.text || "").length)
                                                  + " images=" + (((model.imagePaths || []).length) || 0))
                Component.onDestruction: root.vlog("delegate destroy index=" + index
                                                    + " kind=" + model.kind)
                onRowDataChanged: syncLoadedItem()
                onRowIndexChanged: syncLoadedItem()

                Loader {
                    id: rowLoader
                    anchors.fill: parent
                    // ListView creates only its viewport and cacheBuffer. Do not
                    // bind Loader.active to y/height: that cycle is what caused
                    // the repeated QML binding-loop warnings and unstable tails.
                    active: true
                    // Keep chat row creation synchronous. Entering an
                    // unrendered region while ListView is still incubating
                    // delegates can leave the viewport briefly empty on Qt,
                    // which shows up as white-screen flashing.
                    asynchronous: false
                    onLoaded: {
                        if (!item)
                            return
                        rowDelegate.syncLoadedItem()
                        if (list.initialBottomRestore)
                            list.queueInitialBottomSettle(list.initialRestoreRevision)
                    }
                    sourceComponent: {
                        if (rowDelegate.rowData.kind === "user")
                            return userRowComponent
                        if (rowDelegate.rowData.kind === "assistant")
                            return assistantRowComponent
                        if (rowDelegate.rowData.kind === "compact")
                            return compactRowComponent
                        if (rowDelegate.rowData.kind === "tool")
                            return toolRowComponent
                        return null
                    }
                }
            }
        }
    }

    // error banner — driven by the active conversation's error property
    Rectangle {
        visible: root.isAndroid && root.imagePreviewVisible
        anchors.fill: parent
        color: "transparent"
        z: 500

        ImagePreviewPane {
            anchors.fill: parent
            imageSource: root.imagePreviewSource
            imageName: root.imagePreviewName
            dark: theme.dark
            onCloseRequested: root.imagePreviewVisible = false
            onDownloadRequested: function(source, name) {
                root.requestImageDownload(source, name)
            }
        }
    }

    Rectangle {
        visible: root.textSelectionVisible
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.30)
        z: 520

        MouseArea {
            anchors.fill: parent
            onClicked: root.textSelectionVisible = false
        }

        Rectangle {
            width: Math.min(parent.width - theme.sp6 * 2, 820)
            height: Math.min(parent.height - theme.sp6 * 2, 520)
            anchors.centerIn: parent
            radius: theme.rLg
            color: theme.surface
            border.color: theme.line
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: theme.sp3
                spacing: theme.sp2

                Row {
                    width: parent.width
                    spacing: theme.sp2

                    Text {
                        width: parent.width - copySelectionBtn.width - closeSelectionBtn.width - parent.spacing * 2
                        text: root.textSelectionTitle
                        color: theme.ink
                        font.family: theme.fontBody
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        id: copySelectionBtn
                        width: 52
                        height: 28
                        radius: theme.rSm
                        color: copySelectionMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(0, 0, 0, 0.04)
                        border.color: theme.line
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "Copy"
                            color: theme.ink
                            font.family: theme.fontBody
                            font.pixelSize: 12
                        }
                        MouseArea {
                            id: copySelectionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                clipboard.setText(root.textSelectionValue)
                                if (!clipboard.showCopyFeedback())
                                    toast.showMessage("Copied")
                            }
                        }
                    }

                    Rectangle {
                        id: closeSelectionBtn
                        width: 28
                        height: 28
                        radius: 14
                        color: closeSelectionMouse.containsMouse ? Qt.rgba(0, 0, 0, 0.08) : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: "\u00d7"
                            color: theme.ink
                            font.family: theme.fontBody
                            font.pixelSize: 16
                        }
                        MouseArea {
                            id: closeSelectionMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.textSelectionVisible = false
                        }
                    }
                }

                TextArea {
                    id: selectionArea
                    width: parent.width
                    height: parent.height - 40
                    text: root.textSelectionValue
                    readOnly: true
                    selectByMouse: true
                    selectByKeyboard: true
                    activeFocusOnPress: true
                    wrapMode: TextEdit.Wrap
                    color: theme.ink
                    font.family: theme.fontBody
                    font.pixelSize: 14
                    background: Rectangle {
                        radius: theme.rSm
                        color: theme.paper
                        border.color: theme.line
                        border.width: 1
                    }
                }
            }
        }
    }

    Rectangle {
        id: errorBanner
        visible: root.active && root.active.error.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: inputBar.top
        height: errorText.implicitHeight + theme.sp2
        color: Qt.rgba(0.62, 0.24, 0.12, 0.10)
        Text {
            id: errorText
            anchors.fill: parent
            anchors.margins: theme.sp2
            text: root.active ? root.active.error : ""
            color: theme.clayDeep
            font.family: theme.fontMono
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }

    // input dock — a pseudo-floating card. Side+bottom margins detach it from
    // the viewport edges so it reads as floating, but it still occupies layout
    // space (the chat list's bottom anchors here) so it never overlaps chat
    // content. No side shadow (reference's blue glow rejected); just a hairline.
    Item {
        id: inputBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: theme.sp3
        anchors.rightMargin: theme.sp3
        anchors.bottomMargin: theme.sp3
        height: root.pendingImagePaths.length > 0 ? 224 : ((root.commandMode && !root.commandMenuVisible) ? 138 : 110)

        Rectangle {
            id: card
            anchors.fill: parent
            color: theme.surface
            radius: theme.rLg
            border.color: inputField.activeFocus ? theme.accent(theme.dark ? 0.52 : 0.36) : theme.line
            border.width: 1
            Behavior on border.color { ColorAnimation { duration: 140 } }

            Column {
                anchors.fill: parent
                anchors.margins: theme.sp2
                spacing: theme.sp2

                Rectangle {
                    visible: root.pendingImagePaths.length > 0
                    width: parent.width
                    height: visible ? 76 : 0
                    radius: theme.rSm
                    color: theme.paper
                    border.color: theme.line
                    border.width: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.sp2
                        spacing: theme.sp2

                        Text {
                            width: parent.width
                            text: qsTr("Selected %1/5 images").arg(root.pendingImagePaths.length)
                            color: theme.ink
                            font.family: theme.fontBody
                            font.pixelSize: 12
                        }

                        Row {
                            spacing: theme.sp2

                            Repeater {
                                model: root.pendingImagePaths
                                delegate: Item {
                                    width: 42
                                    height: 42

                                    Image {
                                        anchors.fill: parent
                                        fillMode: Image.PreserveAspectCrop
                                        source: root.fileUrl(modelData || "")
                                        cache: false
                                    }

                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 9
                                        anchors.top: parent.top
                                        anchors.right: parent.right
                                        color: Qt.rgba(0, 0, 0, 0.65)
                                        Text {
                                            anchors.centerIn: parent
                                            text: "\u00d7"
                                            color: "white"
                                            font.pixelSize: 11
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.removePendingImageAt(index)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: root.commandMode && !root.commandMenuVisible
                    width: parent.width
                    height: visible ? 28 : 0
                    radius: theme.rSm
                    color: theme.accent(0.08)
                    border.color: theme.line
                    border.width: 1

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: theme.sp2
                        anchors.rightMargin: theme.sp2
                        text: root.selectedCommand()
                              ? (root.selectedCommand().label + "  " + root.selectedCommand().description)
                              : qsTr("Command mode")
                        color: theme.clayDeep
                        font.family: theme.fontBody
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                TextArea {
                    id: inputField
                    width: parent.width
                    height: 52
                    color: theme.ink   // black text in light mode
                    selectedTextColor: theme.dark ? theme.paper : "white"
                    selectionColor: theme.clay
                    palette.highlight: theme.clay
                    palette.highlightedText: theme.dark ? theme.paper : "white"
                    placeholderText: qsTr("Message StarryAgent\u2026")
                    font.family: theme.fontBody
                    font.pixelSize: 14
                    cursorDelegate: ClayCursor {}
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    selectByKeyboard: true
                    activeFocusOnPress: true
                    padding: 0
                    topPadding: 2
                    bottomPadding: 2
                    leftPadding: 0
                    rightPadding: 0
                    background: Rectangle { color: "transparent" }   // card is the container
                    onTextChanged: root.rebuildCommandSuggestions()

                    Keys.onPressed: function(event) {
                        if (root.commandMenuVisible && event.key === Qt.Key_Down) {
                            root.commandSelectionIndex = Math.min(root.commandSelectionIndex + 1, root.commandSuggestions.length - 1)
                            event.accepted = true
                            return
                        }
                        if (root.commandMenuVisible && event.key === Qt.Key_Up) {
                            root.commandSelectionIndex = Math.max(root.commandSelectionIndex - 1, 0)
                            event.accepted = true
                            return
                        }
                        if (root.commandMenuVisible && event.key === Qt.Key_Tab) {
                            root.acceptCommandSuggestion(root.commandSelectionIndex)
                            event.accepted = true
                            return
                        }
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                && !(event.modifiers & Qt.ShiftModifier)) {
                            if (root.active && root.active.streaming) {
                                event.accepted = true
                                return
                            }
                            const t = text
                            text = ""
                            root.send(t)
                            event.accepted = true
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: theme.sp2

                    // attach-files button (only surviving function button)
                    Item {
                        id: attachBtn
                        width: 32
                        height: 32
                        ThemedMenu {
                            id: attachMenu
                            minMenuWidth: 164
                            items: [qsTr("Image"), qsTr("Camera")]
                            onTriggered: function(index) {
                                if (index === 0) {
                                    if (!root.active)
                                        return
                                    if (root.isAndroid) {
                                        filePicker.pickImages()
                                        return
                                    }
                                    const picked = filePicker.pickImages()
                                    if (!picked || picked.length === 0)
                                        return
                                    for (let i = 0; i < picked.length && root.pendingImagePaths.length < 5; ++i) {
                                        const imported = root.active.importImage(picked[i])
                                        if (imported && imported.length > 0)
                                            root.addPendingImage(imported)
                                    }
                                } else if (index === 1) {
                                    if (root.isAndroid) {
                                        cameraBridge.launchSystemCamera()
                                    } else {
                                        if (!root.canAddMoreImages())
                                            return
                                        cameraWindowLoader.active = true
                                        cameraWindowLoader.item.show()
                                        cameraWindowLoader.item.raise()
                                        cameraWindowLoader.item.requestActivate()
                                    }
                                }
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "+"
                            color: theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 24
                            font.weight: Font.Light
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const host = Window.window && Window.window.contentItem ? Window.window.contentItem : attachBtn.parent
                                const p = attachBtn.mapToItem(host, 0, 0)
                                attachMenu.parent = host
                                attachMenu.x = Math.max(theme.sp2, Math.min(p.x, host.width - attachMenu.width - theme.sp2))
                                attachMenu.y = Math.max(theme.sp2, p.y - attachMenu.height - theme.sp2)
                                attachMenu.open()
                            }
                        }
                    }

                    Item {
                        width: parent.width - attachBtn.width - sendBtn.width - parent.spacing * 2
                        height: 1
                    }

                    // send button — clay circle with an up-arrow
                    Rectangle {
                        id: sendBtn
                        width: 32
                        height: 32
                        radius: 16
                        // Dim + disable while this conversation's turn is in
                        // flight (streaming or tool executing) so the user
                        // can't queue a second message into the same turn.
                        property bool busy: root.active && root.active.streaming
                        color: (inputField.text.trim().length > 0 || root.pendingImagePaths.length > 0) && !sendBtn.busy
                               ? (sendMa.pressed ? theme.clayDeep : theme.clay)
                               : theme.accent(0.30)
                        Behavior on color { ColorAnimation { duration: 120 } }
                        Text {
                            anchors.centerIn: parent
                            text: "\u2191"   // ↑
                            color: "white"
                            font.family: theme.fontBody
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        MouseArea {
                            id: sendMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            enabled: (inputField.text.trim().length > 0 || root.pendingImagePaths.length > 0)
                                && root.active && !sendBtn.busy
                            onClicked: {
                                const t = inputField.text
                                inputField.text = ""
                                root.send(t)
                            }
                        }
                    }
                }
            }

        }
    }

    Rectangle {
        id: commandMenu
        parent: Overlay.overlay
        visible: root.commandMenuVisible
        z: 1000
        readonly property point menuOrigin: inputBar.mapToItem(parent, theme.sp2, 0)
        width: Math.min(column.width - theme.sp4, 720)
        height: visible ? Math.min(144, commandColumn.implicitHeight + theme.sp2) : 0
        x: menuOrigin.x
        y: menuOrigin.y - height - theme.sp1
        radius: theme.rSm
        color: theme.surface
        border.color: theme.line
        border.width: 1

        Column {
            id: commandColumn
            anchors.fill: parent
            anchors.margins: theme.sp1
            spacing: 2

            Repeater {
                model: root.commandSuggestions
                delegate: Rectangle {
                    width: parent.width
                    height: 32
                    radius: theme.rSm
                    color: index === root.commandSelectionIndex ? theme.accent(0.10) : "transparent"

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: theme.sp2
                        anchors.rightMargin: theme.sp2
                        spacing: theme.sp2

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: theme.ink
                            font.family: theme.fontMono
                            font.pixelSize: 12
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, parent.width - 140)
                            text: modelData.description
                            color: theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.commandSelectionIndex = index
                        onClicked: root.acceptCommandSuggestion(index)
                    }
                }
            }
        }
    }

    Component {
        id: userRowComponent
        Item {
            property var rowData
            property int rowIndex: -1
            readonly property var rowModel: rowData || ({})
            readonly property var rowImages: (rowData && rowData.imagePaths) ? rowData.imagePaths : []
            width: parent ? parent.width : 0
            implicitHeight: bubble.height

            Rectangle {
                id: bubble
                anchors.right: parent.right
                readonly property real maxTextWidth: Math.min(parent.width - theme.sp5, parent.width * 0.72)
                readonly property real textBubbleWidth: Math.min(maxTextWidth,
                                                                Math.max(userText.implicitWidth + theme.sp4 * 2,
                                                                         72))
                width: rowImages.length > 0
                    ? Math.min(parent.width - theme.sp5, 280)
                    : textBubbleWidth
                color: theme.userBubble
                border.color: theme.userBubbleBorder
                border.width: 1
                radius: theme.rLg
                height: userContent.implicitHeight + theme.sp3 * 2

                Column {
                    id: userContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: theme.sp4
                    spacing: theme.sp2

                    Repeater {
                        model: rowImages
                        delegate: Image {
                            width: Math.min(240, bubble.width - theme.sp4 * 2)
                            height: 160
                            fillMode: Image.PreserveAspectFit
                            source: root.fileUrl(modelData || "")
                            cache: false
                            MouseArea {
                                anchors.fill: parent
                                preventStealing: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.openImagePreview(parent.source, "")
                            }
                        }
                    }

                    TextEdit {
                        id: userText
                        visible: rowData && rowData.text && rowData.text.length > 0
                        width: parent.width
                        text: (rowData && rowData.text) ? rowData.text : ""
                        readOnly: true
                        selectByMouse: true
                        selectedTextColor: theme.dark ? theme.paper : "white"
                        selectionColor: theme.paper
                        color: theme.userBubbleText
                        font.family: theme.fontBody
                        font.pixelSize: 14
                        wrapMode: Text.Wrap
                        selectByKeyboard: true
                        activeFocusOnPress: true
                    }
                }
            }
        }
    }

    Component {
        id: assistantRowComponent
        Item {
            id: assistantRow
            property var rowData
            property int rowIndex: -1
            readonly property string filtered: root.filterThink((rowData && rowData.text) ? rowData.text : "")
            readonly property bool hasRawText: (rowData && rowData.text ? rowData.text.length : 0) > 0
            readonly property bool thinking: hasRawText && filtered.length === 0
            readonly property bool isActiveStreamingRow: root.active && root.active.streaming && rowIndex === list.count - 1
            readonly property bool shouldShowStreamingText: isActiveStreamingRow
            readonly property bool hasRenderableText: filtered.length > 0
            readonly property bool shouldShowMarkdown: hasRenderableText && !thinking && !isActiveStreamingRow
            readonly property bool hasVisibleBody: shouldShowStreamingText || shouldShowMarkdown
            property bool hovering: false
            property bool actionBarHovered: false
            property bool actionBarPressed: false
            function syncMarkdownView() {
                if (!markdownLoader.item)
                    return
                markdownLoader.item.isStreaming = false
                markdownLoader.item.deferSegments = !list.initialBottomRestore
                markdownLoader.item.rawText = filtered
            }
            width: parent.width
            implicitHeight: hasVisibleBody ? contentWrap.implicitHeight : 0
            opacity: hasVisibleBody ? 1 : 0
            Component.onCompleted: root.vlog("assistant row index=" + rowIndex
                                              + " rawLen=" + ((rowData && rowData.text) ? rowData.text.length : 0)
                                              + " filteredLen=" + filtered.length
                                              + " thinking=" + thinking)
            onFilteredChanged: {
                root.vlog("assistant filtered index=" + rowIndex
                          + " filteredLen=" + filtered.length)
                syncMarkdownView()
            }
            onHasRenderableTextChanged: syncMarkdownView()
            onRowDataChanged: {
                hovering = false
                actionBarHovered = false
                actionBarPressed = false
            }

            Timer {
                id: hoverCloseTimer
                interval: 120
                repeat: false
                onTriggered: {
                    if (!assistantRow.actionBarHovered && !assistantRow.actionBarPressed)
                        assistantRow.hovering = false
                }
            }

            Row {
                id: contentWrap
                visible: parent.hasVisibleBody
                width: parent.width
                spacing: theme.sp2

                Text {
                    id: avatar
                    anchors.top: parent.top
                    text: "\u2726\uFE0E"
                    color: theme.clay
                    font.family: theme.fontDisplay
                    font.pixelSize: 16
                }

                Item {
                    width: Math.max(0, parent.width - avatar.width - parent.spacing)
                    implicitHeight: assistantRow.shouldShowMarkdown
                        ? (markdownLoader.item ? markdownLoader.item.implicitHeight : 0)
                        : Math.max(streamingText.implicitHeight, theme.sp4)

                    TextEdit {
                        id: streamingText
                        visible: !assistantRow.shouldShowMarkdown
                        width: parent.width
                        text: assistantRow.filtered
                        readOnly: true
                        selectByMouse: true
                        selectByKeyboard: true
                        activeFocusOnPress: true
                        wrapMode: TextEdit.Wrap
                        color: theme.ink
                        font.family: theme.fontBody
                        font.pixelSize: 14
                    }

                    Loader {
                        id: markdownLoader
                        active: parent.width > 0 && assistantRow.shouldShowMarkdown
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        sourceComponent: markdownViewComponent
                        onLoaded: {
                            assistantRow.syncMarkdownView()
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                hoverEnabled: true
                onEntered: {
                    hoverCloseTimer.stop()
                    parent.hovering = true
                }
                onExited: hoverCloseTimer.restart()
            }

            Loader {
                anchors.top: parent.top
                anchors.right: parent.right
                active: parent.hasVisibleBody && (parent.hovering || parent.actionBarHovered || parent.actionBarPressed)
                sourceComponent: Item {
                    property var sourceRow: assistantRow
                    implicitWidth: actionRow.implicitWidth
                    implicitHeight: actionRow.implicitHeight

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.NoButton
                        hoverEnabled: true
                        onEntered: {
                            sourceRow.actionBarHovered = true
                            hoverCloseTimer.stop()
                        }
                        onExited: {
                            sourceRow.actionBarHovered = false
                            if (!sourceRow.actionBarPressed)
                                hoverCloseTimer.restart()
                        }
                    }

                    Row {
                        id: actionRow
                        spacing: theme.sp1

                        Item {
                            width: 40
                            height: 24

                            Rectangle {
                                anchors.fill: parent
                                radius: theme.rSm
                                color: Qt.rgba(0.97, 0.95, 0.91, selectionCopyMouse.containsMouse ? 0.34 : 0.26)
                                border.color: theme.line
                                border.width: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "Copy"
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: selectionCopyMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onPressed: sourceRow.actionBarPressed = true
                                onReleased: sourceRow.actionBarPressed = false
                                onCanceled: sourceRow.actionBarPressed = false
                                onEntered: hoverCloseTimer.stop()
                                onExited: hoverCloseTimer.restart()
                                onClicked: {
                                    clipboard.setText(sourceRow.filtered)
                                    if (!clipboard.showCopyFeedback())
                                        toast.showMessage("Copied")
                                }
                            }
                        }

                        Item {
                            width: 46
                            height: 24

                            Rectangle {
                                anchors.fill: parent
                                radius: theme.rSm
                                color: Qt.rgba(0.97, 0.95, 0.91, selectionOpenMouse.containsMouse ? 0.34 : 0.26)
                                border.color: theme.line
                                border.width: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("Select")
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: selectionOpenMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onPressed: sourceRow.actionBarPressed = true
                                onReleased: sourceRow.actionBarPressed = false
                                onCanceled: sourceRow.actionBarPressed = false
                                onEntered: hoverCloseTimer.stop()
                                onExited: hoverCloseTimer.restart()
                                onClicked: root.openTextSelection(qsTr("Assistant Message"), sourceRow.filtered)
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: compactRowComponent
        Item {
            property var rowData
            property int rowIndex: -1
            width: parent ? parent.width : 0
            implicitHeight: badge.implicitHeight + theme.sp1

            Rectangle {
                id: badge
                anchors.horizontalCenter: parent.horizontalCenter
                width: compactText.implicitWidth + theme.sp4
                height: 26
                radius: 13
                color: Qt.rgba(0, 0, 0, 0.04)
                border.color: theme.line
                border.width: 1

                Text {
                    id: compactText
                    anchors.centerIn: parent
                    text: (rowData && rowData.text) ? rowData.text : qsTr("Context compacted")
                    color: theme.inkSoft
                    font.family: theme.fontBody
                    font.pixelSize: 12
                }
            }
        }
    }


    Component {
        id: markdownViewComponent
        MarkdownView {
            onImageActivated: function(source, alt) {
                root.openImagePreview(source, alt)
            }
            onSelectionRequested: function(text) {
                root.openTextSelection(qsTr("Assistant Message"), text)
            }
        }
    }

    Component {
        id: toolRowComponent
        Item {
            property var rowData
            property int rowIndex: -1
            width: parent ? parent.width : 0
            implicitHeight: card.height

            ToolCallCard {
                id: card
                anchors.left: parent.left
                anchors.right: parent.right
                toolCallId: rowData ? rowData.toolCallId : ""
                toolName: rowData ? rowData.toolName : ""
                argsText: rowData ? rowData.argsText : ""
                status: rowData ? rowData.status : "composing"
                result: rowData ? rowData.result : ""
                needsApproval: rowData ? rowData.needsApproval : true
                onApprove: if (root.active && rowData) root.active.dispatch(rowData.toolCallId, rowData.toolName, rowData.argsText)
                onDeny: if (root.active && rowData) root.active.denyTool(rowData.toolCallId)
            }
        }
    }

    Connections {
        target: list
        function onContentYChanged() {
            if (!verboseLogging)
                return
            root.vlog("viewport contentY=" + list.contentY + " height=" + list.height)
        }
    }

}
