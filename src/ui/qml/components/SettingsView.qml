import QtQuick
import QtQuick.Controls
import "." as Components

// SettingsView — a peer view of ChatView in the right pane. No back button:
// you leave settings by clicking a conversation or 新对话 in the sidebar,
// the same way you leave one conversation for another.
//
// Edits bind directly to the Settings Q_PROPERTY setters (each persists to
// settings.json), so changes save immediately. The Storage section lets the
// user switch the `.starryagent` root via the same presets offered on first
// launch — picking one calls config.setRoot(), then settings.load() to pull
// that root's profile.
Item {
    id: root
    property int selectedTab: 0
    property string themeUiError: ""
    property string skillUiError: ""
    property int developerTapCount: 0
    readonly property bool developerSettingsVisibleThisSession: developerSettingsUnlockedAtStartup
    property bool developerSettingsUnlockedAtStartup: false
    property bool developerSettingsDraftEnabled: settings.developerSettingsEnabled
    property bool developerThemeOnAndroidDraftEnabled: settings.developerThemeOnAndroidEnabled
    property string androidBackgroundStatusMessage: ""
    property string rootSwitchStatusMessage: ""
    readonly property bool developerSettingsDirty:
        developerSettingsDraftEnabled !== settings.developerSettingsEnabled ||
        developerThemeOnAndroidDraftEnabled !== settings.developerThemeOnAndroidEnabled
    readonly property bool narrowLayout: width < 600
    readonly property int headerLeadingInset: narrowLayout ? theme.sp6 + theme.sp5 : theme.sp4
    readonly property int headerTitleTopMargin: narrowLayout ? theme.sp5 : theme.sp2

    function resetDeveloperDraftsToSaved() {
        developerSettingsDraftEnabled = settings.developerSettingsEnabled
        developerThemeOnAndroidDraftEnabled = settings.developerThemeOnAndroidEnabled
    }

    function resetDeveloperDraftsToDefaults() {
        developerSettingsDraftEnabled = false
        developerThemeOnAndroidDraftEnabled = false
    }

    Component.onCompleted: {
        developerSettingsUnlockedAtStartup = settings.developerSettingsUnlocked
        resetDeveloperDraftsToSaved()
    }

    // page surface
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

    // --- Header bar ---
    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 88
        color: theme.surface
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: theme.line
        }
        Text {
            anchors.top: parent.top
            anchors.topMargin: root.headerTitleTopMargin
            anchors.left: parent.left
            anchors.leftMargin: root.headerLeadingInset
            text: qsTr("Settings")
            color: theme.ink
            font.family: theme.fontDisplay
            font.pixelSize: 16
            font.weight: Font.Medium
        }
        Row {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: root.headerLeadingInset
            spacing: theme.sp4
            Repeater {
                model: [qsTr("General"), qsTr("Themes"), "Skill", qsTr("Scheduled Tasks")]
                delegate: Item {
                    width: label.implicitWidth + theme.sp2
                    height: 32
                    Text {
                        id: label
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        color: root.selectedTab === index ? theme.clayDeep : theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 12
                        font.weight: root.selectedTab === index ? Font.Medium : Font.Normal
                    }
                    Rectangle {
                        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                        height: 2; radius: 1
                        color: theme.clay
                        visible: root.selectedTab === index
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.selectedTab = index }
                }
            }
        }
    }

    // --- Scrollable centered content ---
    Flickable {
        visible: root.selectedTab === 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: theme.sp4
        contentHeight: col.implicitHeight + theme.sp6
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: col
            width: Math.min(parent.width - theme.sp6 * 2, 640)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: theme.sp5

            // ===================== API =====================
            Text {
                text: qsTr("API")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
            }
            Text {
                text: qsTr("Base URL")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextField {
                width: parent.width
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: settings.apiBaseUrl
                onTextEdited: settings.apiBaseUrl = text
                font.family: theme.fontMono
                font.pixelSize: 12
                background: FieldBg {}
                placeholderText: "https://api.openai.com/v1"
            }
            Text {
                text: qsTr("API Key")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextField {
                width: parent.width
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: settings.apiKey
                onTextEdited: settings.apiKey = text
                echoMode: TextInput.Password
                font.family: theme.fontMono
                font.pixelSize: 12
                background: FieldBg {}
                placeholderText: "sk-\u2026"
            }
            Text {
                text: qsTr("Current Model")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            ComboBox {
                id: modelCombo
                width: parent.width
                model: settings.models
                currentIndex: Math.max(0, settings.models.indexOf(settings.model))
                onActivated: if (currentIndex >= 0) settings.model = settings.models[currentIndex]
                font.family: theme.fontMono
                font.pixelSize: 12
                padding: 0
                topPadding: 0
                bottomPadding: 0
                leftPadding: 0
                rightPadding: 0

                delegate: ItemDelegate {
                    id: modelDelegate
                    required property int index
                    required property var modelData
                    width: modelCombo.width - theme.sp2 * 2
                    height: 44
                    padding: 0
                    hoverEnabled: true

                    background: Rectangle {
                        Behavior on color { ColorAnimation { duration: 140 } }
                        Behavior on border.color { ColorAnimation { duration: 140 } }
                        radius: theme.rSm
                        color: modelDelegate.highlighted
                               ? (theme.accent(theme.dark ? 0.14 : 0.10))
                               : (modelCombo.currentIndex === modelDelegate.index
                                  ? (theme.accent(theme.dark ? 0.09 : 0.06))
                                  : "transparent")
                        border.width: modelCombo.currentIndex === modelDelegate.index ? 1 : 0
                        border.color: modelCombo.currentIndex === modelDelegate.index
                                      ? (theme.accent(theme.dark ? 0.22 : 0.18))
                                      : "transparent"
                    }

                    contentItem: Row {
                        anchors.fill: parent
                        anchors.leftMargin: theme.sp3
                        anchors.rightMargin: theme.sp3
                        spacing: theme.sp2

                        Text {
                            width: parent.width - checkMark.width - parent.spacing
                            anchors.verticalCenter: parent.verticalCenter
                            text: String(modelDelegate.modelData || "")
                            color: modelCombo.currentIndex === modelDelegate.index ? theme.ink : theme.inkSoft
                            font.family: theme.fontMono
                            font.pixelSize: 13
                            font.weight: modelCombo.currentIndex === modelDelegate.index ? Font.Medium : Font.Normal
                            elide: Text.ElideRight
                            Behavior on color { ColorAnimation { duration: 140 } }
                        }

                        Text {
                            id: checkMark
                            width: 18
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            text: modelCombo.currentIndex === modelDelegate.index ? "\u2713\uFE0E" : ""
                            color: theme.clay
                            font.family: theme.fontBody
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            opacity: modelCombo.currentIndex === modelDelegate.index ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                    }
                }

                indicator: Item {
                    width: 22
                    height: 22
                    x: modelCombo.width - width - theme.sp3
                    y: (modelCombo.height - height) / 2

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = theme.dark ? "#D4C8B4" : "#4B4338"
                            ctx.lineWidth = 2.2
                            ctx.lineCap = "round"

                            ctx.beginPath()
                            ctx.moveTo(width * 0.22, height * 0.38)
                            ctx.lineTo(width * 0.50, height * 0.66)
                            ctx.lineTo(width * 0.78, height * 0.38)
                            ctx.stroke()
                        }
                        Connections {
                            target: theme
                            function onDarkChanged() { parent.requestPaint() }
                        }
                    }
                }

                contentItem: Text {
                    leftPadding: theme.sp3
                    rightPadding: 42
                    verticalAlignment: Text.AlignVCenter
                    text: modelCombo.displayText
                    color: theme.ink
                    font.family: theme.fontMono
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitHeight: 46
                    radius: 14
                    color: theme.dark ? "#2A251F" : "#F7F1E7"
                    border.width: modelCombo.popup.visible ? 1 : 1
                    border.color: modelCombo.popup.visible
                                  ? (theme.accent(theme.dark ? 0.24 : 0.20))
                                  : theme.line

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: parent.radius - 1
                        color: "transparent"
                        border.width: modelCombo.visualFocus ? 1 : 0
                        border.color: theme.accent(theme.dark ? 0.14 : 0.12)
                    }
                }

                popup: Popup {
                    id: modelPopup
                    y: modelCombo.height + theme.sp2
                    width: modelCombo.width
                    padding: theme.sp1
                    margins: 0
                    transformOrigin: Item.Top
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    enter: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "y"; from: modelCombo.height + theme.sp1; to: modelCombo.height + theme.sp2; duration: 180; easing.type: Easing.OutCubic }
                        }
                    }
                    exit: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120; easing.type: Easing.InCubic }
                            NumberAnimation { property: "scale"; from: 1.0; to: 0.98; duration: 120; easing.type: Easing.InCubic }
                            NumberAnimation { property: "y"; from: modelCombo.height + theme.sp2; to: modelCombo.height + theme.sp1; duration: 120; easing.type: Easing.InCubic }
                        }
                    }

                    background: Item {
                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 6
                            radius: 18
                            color: theme.dark ? Qt.rgba(0, 0, 0, 0.26) : Qt.rgba(0.15, 0.10, 0.05, 0.10)
                            opacity: modelPopup.opacity
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 18
                            color: theme.dark ? "#2A251F" : "#FBF7EF"
                            border.width: 1
                            border.color: theme.dark ? "#4A4035" : "#DED5C7"
                        }
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: modelCombo.popup.visible ? modelCombo.delegateModel : null
                        currentIndex: modelCombo.highlightedIndex
                        spacing: 4
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { }
                    }
                }
            }
            Text {
                text: qsTr("Models")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextArea {
                width: parent.width
                height: 108
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: settings.modelsText
                onTextChanged: settings.modelsText = text
                font.family: theme.fontMono
                font.pixelSize: 12
                wrapMode: TextEdit.NoWrap
                background: FieldBg {}
                placeholderText: "gpt-4o-mini\ngpt-4.1\ngpt-5"
            }

            Text {
                text: qsTr("Web Search")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }
            Text {
                text: qsTr("Web Search Implementation")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            ComboBox {
                id: webSearchImplCombo
                width: parent.width
                model: [
                    { value: "bing_legacy", label: qsTr("Bing Search (Legacy)") },
                    { value: "current_api", label: qsTr("Use Current API Search Interface") },
                    { value: "aliyun_dashscope_internal", label: qsTr("Use Aliyun DashScope Internal Search") },
                    { value: "external_api", label: qsTr("Use External API") }
                ]
                currentIndex: {
                    for (let i = 0; i < model.length; ++i) {
                        if (model[i].value === settings.webSearchImplementation)
                            return i
                    }
                    return 0
                }
                onActivated: {
                    if (currentIndex >= 0)
                        settings.webSearchImplementation = model[currentIndex].value
                }
                textRole: "label"
                font.family: theme.fontBody
                font.pixelSize: 12
                padding: 0
                topPadding: 0
                bottomPadding: 0
                leftPadding: 0
                rightPadding: 0

                delegate: ItemDelegate {
                    id: webImplDelegate
                    required property int index
                    required property var modelData
                    width: webSearchImplCombo.width - theme.sp2 * 2
                    height: 44
                    padding: 0
                    hoverEnabled: true

                    background: Rectangle {
                        Behavior on color { ColorAnimation { duration: 140 } }
                        Behavior on border.color { ColorAnimation { duration: 140 } }
                        radius: theme.rSm
                        color: webImplDelegate.highlighted
                               ? (theme.accent(theme.dark ? 0.14 : 0.10))
                               : (webSearchImplCombo.currentIndex === webImplDelegate.index
                                  ? (theme.accent(theme.dark ? 0.09 : 0.06))
                                  : "transparent")
                        border.width: webSearchImplCombo.currentIndex === webImplDelegate.index ? 1 : 0
                        border.color: webSearchImplCombo.currentIndex === webImplDelegate.index
                                      ? (theme.accent(theme.dark ? 0.22 : 0.18))
                                      : "transparent"
                    }

                    contentItem: Row {
                        anchors.fill: parent
                        anchors.leftMargin: theme.sp3
                        anchors.rightMargin: theme.sp3
                        spacing: theme.sp2

                        Text {
                            width: parent.width - implCheckMark.width - parent.spacing
                            anchors.verticalCenter: parent.verticalCenter
                            text: String(webImplDelegate.modelData.label || "")
                            color: webSearchImplCombo.currentIndex === webImplDelegate.index ? theme.ink : theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 13
                            font.weight: webSearchImplCombo.currentIndex === webImplDelegate.index ? Font.Medium : Font.Normal
                            elide: Text.ElideRight
                            Behavior on color { ColorAnimation { duration: 140 } }
                        }

                        Text {
                            id: implCheckMark
                            width: 18
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            text: webSearchImplCombo.currentIndex === webImplDelegate.index ? "\u2713\uFE0E" : ""
                            color: theme.clay
                            font.family: theme.fontBody
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            opacity: webSearchImplCombo.currentIndex === webImplDelegate.index ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                    }
                }

                indicator: Item {
                    width: 22
                    height: 22
                    x: webSearchImplCombo.width - width - theme.sp3
                    y: (webSearchImplCombo.height - height) / 2

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = theme.dark ? "#D4C8B4" : "#4B4338"
                            ctx.lineWidth = 2.2
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.moveTo(width * 0.22, height * 0.38)
                            ctx.lineTo(width * 0.50, height * 0.66)
                            ctx.lineTo(width * 0.78, height * 0.38)
                            ctx.stroke()
                        }
                        Connections {
                            target: theme
                            function onDarkChanged() { parent.requestPaint() }
                        }
                    }
                }

                contentItem: Text {
                    leftPadding: theme.sp3
                    rightPadding: 42
                    verticalAlignment: Text.AlignVCenter
                    text: webSearchImplCombo.currentIndex >= 0 ? webSearchImplCombo.model[webSearchImplCombo.currentIndex].label : ""
                    color: theme.ink
                    font.family: theme.fontBody
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitHeight: 46
                    radius: 14
                    color: theme.dark ? "#2A251F" : "#F7F1E7"
                    border.width: 1
                    border.color: webSearchImplCombo.popup.visible
                                  ? (theme.accent(theme.dark ? 0.24 : 0.20))
                                  : theme.line
                }

                popup: Popup {
                    id: webSearchImplPopup
                    y: webSearchImplCombo.height + theme.sp2
                    width: webSearchImplCombo.width
                    padding: theme.sp1
                    margins: 0
                    transformOrigin: Item.Top
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    enter: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "y"; from: webSearchImplCombo.height + theme.sp1; to: webSearchImplCombo.height + theme.sp2; duration: 180; easing.type: Easing.OutCubic }
                        }
                    }
                    exit: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120; easing.type: Easing.InCubic }
                            NumberAnimation { property: "scale"; from: 1.0; to: 0.98; duration: 120; easing.type: Easing.InCubic }
                            NumberAnimation { property: "y"; from: webSearchImplCombo.height + theme.sp2; to: webSearchImplCombo.height + theme.sp1; duration: 120; easing.type: Easing.InCubic }
                        }
                    }

                    background: Item {
                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 6
                            radius: 18
                            color: theme.dark ? Qt.rgba(0, 0, 0, 0.26) : Qt.rgba(0.15, 0.10, 0.05, 0.10)
                            opacity: webSearchImplPopup.opacity
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 18
                            color: theme.dark ? "#2A251F" : "#FBF7EF"
                            border.width: 1
                            border.color: theme.dark ? "#4A4035" : "#DED5C7"
                        }
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: webSearchImplCombo.popup.visible ? webSearchImplCombo.delegateModel : null
                        currentIndex: webSearchImplCombo.highlightedIndex
                        spacing: 4
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { }
                    }
                }
            }
            Text {
                visible: settings.webSearchImplementation !== "bing_legacy"
                         && settings.webSearchImplementation !== "aliyun_dashscope_internal"
                text: qsTr("Web Search Model")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextField {
                visible: settings.webSearchImplementation !== "bing_legacy"
                         && settings.webSearchImplementation !== "aliyun_dashscope_internal"
                width: parent.width
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: settings.webSearchModel
                onTextEdited: settings.webSearchModel = text
                font.family: theme.fontMono
                font.pixelSize: 12
                background: FieldBg {}
                placeholderText: settings.webSearchImplementation === "aliyun_dashscope_internal"
                                 ? "qwen-plus"
                                 : "gpt-4o-mini"
            }
            Text {
                visible: settings.webSearchImplementation === "external_api"
                         || settings.webSearchImplementation === "aliyun_dashscope_internal"
                text: qsTr("External Web Search Base URL")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextField {
                visible: settings.webSearchImplementation === "external_api"
                         || settings.webSearchImplementation === "aliyun_dashscope_internal"
                width: parent.width
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: settings.webSearchExternalBaseUrl
                onTextEdited: settings.webSearchExternalBaseUrl = text
                font.family: theme.fontMono
                font.pixelSize: 12
                background: FieldBg {}
                placeholderText: settings.webSearchImplementation === "aliyun_dashscope_internal"
                                 ? "https://xxxx-hangzhou.opensearch.aliyuncs.com"
                                 : "https://api.openai.com/v1"
            }
            Text {
                visible: settings.webSearchImplementation === "external_api"
                         || settings.webSearchImplementation === "aliyun_dashscope_internal"
                text: qsTr("External Web Search API Key")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextField {
                visible: settings.webSearchImplementation === "external_api"
                         || settings.webSearchImplementation === "aliyun_dashscope_internal"
                width: parent.width
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: settings.webSearchExternalApiKey
                onTextEdited: settings.webSearchExternalApiKey = text
                echoMode: TextInput.Password
                font.family: theme.fontMono
                font.pixelSize: 12
                background: FieldBg {}
                placeholderText: "sk-\u2026"
            }

            // =================== Behavior ===================
            Text {
                text: qsTr("Behavior")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }
            ToggleRow {
                title: qsTr("Streaming")
                description: qsTr("Stream the reply token-by-token. Off → one complete response.")
                checked: settings.streaming
                onToggled: settings.streaming = checked
            }
            ToggleRow {
                title: qsTr("Bypass permissions")
                description: qsTr("Skip per-tool-call approval. DANGEROUS — tools execute without asking.")
                checked: settings.bypassPermissions
                onToggled: settings.bypassPermissions = checked
            }
            ToggleRow {
                title: qsTr("Compact")
                description: qsTr("Summarize earlier turns when a conversation nears the model's context limit. On by default.")
                checked: settings.compact
                onToggled: settings.compact = checked
            }
            ToggleRow {
                visible: Qt.platform.os !== "android"
                title: qsTr("Start on login")
                description: qsTr("Launch StarryAgent automatically when you sign in.")
                checked: settings.startOnLogin
                onToggled: settings.startOnLogin = checked
            }
            ToggleRow {
                visible: Qt.platform.os !== "android"
                title: qsTr("Close to tray")
                description: qsTr("Closing the window keeps StarryAgent running in the system tray.")
                checked: settings.closeToTray
                onToggled: settings.closeToTray = checked
            }

            Rectangle {
                visible: Qt.platform.os === "android"
                width: parent.width
                height: androidBackgroundColumn.implicitHeight + theme.sp4 * 2
                radius: theme.rMd
                color: theme.surface
                border.color: theme.line
                border.width: 1

                Column {
                    id: androidBackgroundColumn
                    width: parent.width - theme.sp4 * 2
                    anchors.top: parent.top
                    anchors.topMargin: theme.sp4
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: theme.sp3

                    Text {
                        text: qsTr("Background Runtime")
                        color: theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 10
                        font.letterSpacing: 1
                        font.capitalization: Font.AllUppercase
                    }

                    Text {
                        width: parent.width
                        text: androidBackgroundRuntime.batteryOptimizationIgnored
                              ? qsTr("Battery optimization is already disabled for StarryAgent.")
                              : qsTr("Allow unrestricted background running so scheduled tasks and long tool calls are less likely to be paused.")
                        color: theme.ink
                        font.family: theme.fontBody
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Android only offers the standard battery-optimization exemption and app settings pages. Some OEM ROMs still block startup or background execution.")
                        color: theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }

                    Flow {
                        width: parent.width
                        spacing: theme.sp2

                        ThemeButton {
                            text: androidBackgroundRuntime.batteryOptimizationIgnored
                                  ? qsTr("Refresh status")
                                  : qsTr("Allow background running")
                            variant: "primary"
                            onClicked: {
                                if (androidBackgroundRuntime.batteryOptimizationIgnored)
                                    androidBackgroundRuntime.refreshBatteryOptimizationState()
                                else
                                    androidBackgroundRuntime.requestIgnoreBatteryOptimizations()
                            }
                        }

                        ThemeButton {
                            text: qsTr("Open app settings")
                            onClicked: androidBackgroundRuntime.openBackgroundSettings()
                        }
                    }

                    Text {
                        width: parent.width
                        text: qsTr("If your phone still kills the app, check vendor-specific startup and battery steps on Don’t Kill My App.")
                        color: theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }

                    Text {
                        width: parent.width
                        text: qsTr("Open Don’t Kill My App")
                        color: theme.clayDeep
                        font.family: theme.fontBody
                        font.pixelSize: 12
                        font.underline: true
                        wrapMode: Text.Wrap

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.openUrlExternally("https://dontkillmyapp.com/")
                        }
                    }

                    Text {
                        visible: root.androidBackgroundStatusMessage.length > 0
                        width: parent.width
                        text: root.androidBackgroundStatusMessage
                        color: theme.clay
                        font.family: theme.fontBody
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                    }
                }
            }

            // ================= Appearance =================
            Text {
                text: qsTr("Appearance")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }
            Text {
                text: qsTr("Language")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            ComboBox {
                id: languageCombo
                width: parent.width
                model: languageManager.availableLanguages
                currentIndex: {
                    for (let i = 0; i < model.length; ++i) {
                        if (model[i].value === languageManager.currentLanguage)
                            return i
                    }
                    return 0
                }
                onActivated: {
                    if (currentIndex >= 0)
                        languageManager.setCurrentLanguage(model[currentIndex].value)
                }
                textRole: "label"
                font.family: theme.fontBody
                font.pixelSize: 12
                padding: 0
                topPadding: 0
                bottomPadding: 0
                leftPadding: 0
                rightPadding: 0

                delegate: ItemDelegate {
                    id: languageDelegate
                    required property int index
                    required property var modelData
                    width: languageCombo.width - theme.sp2 * 2
                    height: 44
                    padding: 0
                    hoverEnabled: true

                    background: Rectangle {
                        Behavior on color { ColorAnimation { duration: 140 } }
                        Behavior on border.color { ColorAnimation { duration: 140 } }
                        radius: theme.rSm
                        color: languageDelegate.highlighted
                               ? (theme.accent(theme.dark ? 0.14 : 0.10))
                               : (languageCombo.currentIndex === languageDelegate.index
                                  ? (theme.accent(theme.dark ? 0.09 : 0.06))
                                  : "transparent")
                        border.width: languageCombo.currentIndex === languageDelegate.index ? 1 : 0
                        border.color: languageCombo.currentIndex === languageDelegate.index
                                      ? (theme.accent(theme.dark ? 0.22 : 0.18))
                                      : "transparent"
                    }

                    contentItem: Row {
                        anchors.fill: parent
                        anchors.leftMargin: theme.sp3
                        anchors.rightMargin: theme.sp3
                        spacing: theme.sp2

                        Text {
                            width: parent.width - languageCheckMark.width - parent.spacing
                            anchors.verticalCenter: parent.verticalCenter
                            text: String(languageDelegate.modelData.label || "")
                            color: languageCombo.currentIndex === languageDelegate.index ? theme.ink : theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 13
                            font.weight: languageCombo.currentIndex === languageDelegate.index ? Font.Medium : Font.Normal
                            elide: Text.ElideRight
                            Behavior on color { ColorAnimation { duration: 140 } }
                        }

                        Text {
                            id: languageCheckMark
                            width: 18
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignRight
                            text: languageCombo.currentIndex === languageDelegate.index ? "\u2713\uFE0E" : ""
                            color: theme.clay
                            font.family: theme.fontBody
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            opacity: languageCombo.currentIndex === languageDelegate.index ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        }
                    }
                }

                indicator: Item {
                    width: 22
                    height: 22
                    x: languageCombo.width - width - theme.sp3
                    y: (languageCombo.height - height) / 2

                    Canvas {
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = theme.dark ? "#D4C8B4" : "#4B4338"
                            ctx.lineWidth = 2.2
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.moveTo(width * 0.22, height * 0.38)
                            ctx.lineTo(width * 0.50, height * 0.66)
                            ctx.lineTo(width * 0.78, height * 0.38)
                            ctx.stroke()
                        }
                        Connections {
                            target: theme
                            function onDarkChanged() { parent.requestPaint() }
                        }
                    }
                }

                contentItem: Text {
                    leftPadding: theme.sp3
                    rightPadding: 42
                    verticalAlignment: Text.AlignVCenter
                    text: languageCombo.currentIndex >= 0 ? languageCombo.model[languageCombo.currentIndex].label : ""
                    color: theme.ink
                    font.family: theme.fontBody
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    implicitHeight: 46
                    radius: 14
                    color: theme.dark ? "#2A251F" : "#F7F1E7"
                    border.width: 1
                    border.color: languageCombo.popup.visible
                                  ? (theme.accent(theme.dark ? 0.24 : 0.20))
                                  : theme.line
                }

                popup: Popup {
                    id: languagePopup
                    y: languageCombo.height + theme.sp2
                    width: languageCombo.width
                    padding: theme.sp1
                    margins: 0
                    transformOrigin: Item.Top
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    enter: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 220; easing.type: Easing.OutCubic }
                            NumberAnimation { property: "y"; from: languageCombo.height + theme.sp1; to: languageCombo.height + theme.sp2; duration: 180; easing.type: Easing.OutCubic }
                        }
                    }
                    exit: Transition {
                        ParallelAnimation {
                            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120; easing.type: Easing.InCubic }
                            NumberAnimation { property: "scale"; from: 1.0; to: 0.98; duration: 120; easing.type: Easing.InCubic }
                            NumberAnimation { property: "y"; from: languageCombo.height + theme.sp2; to: languageCombo.height + theme.sp1; duration: 120; easing.type: Easing.InCubic }
                        }
                    }

                    background: Item {
                        Rectangle {
                            anchors.fill: parent
                            anchors.topMargin: 6
                            radius: 18
                            color: theme.dark ? Qt.rgba(0, 0, 0, 0.26) : Qt.rgba(0.15, 0.10, 0.05, 0.10)
                            opacity: languagePopup.opacity
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: 18
                            color: theme.dark ? "#2A251F" : "#FBF7EF"
                            border.width: 1
                            border.color: theme.dark ? "#4A4035" : "#DED5C7"
                        }
                    }

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: languageCombo.popup.visible ? languageCombo.delegateModel : null
                        currentIndex: languageCombo.highlightedIndex
                        spacing: 4
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { }
                    }
                }
            }
            Row {
                width: parent.width
                spacing: theme.sp2
                Repeater {
                    model: [ { id: "light", label: qsTr("Light") }, { id: "dark", label: qsTr("Dark") } ]
                    delegate: Rectangle {
                        width: (parent.width - parent.spacing) / 2
                        height: 36
                        radius: theme.rPill
                        border.color: settings.theme === modelData.id ? theme.clay : theme.line
                        border.width: 1
                        color: settings.theme === modelData.id ? theme.accent(0.08) : "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: settings.theme === modelData.id ? theme.clayDeep : theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 13
                            font.weight: settings.theme === modelData.id ? Font.Medium : Font.Normal
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: settings.theme = modelData.id
                        }
                    }
                }
            }

            // ================== Storage ==================
            Text {
                text: qsTr("Storage")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }
            Text {
                text: qsTr(".starryagent Root Directory")
                color: theme.ink
                font.family: theme.fontBody
                font.pixelSize: 12
            }
            TextField {
                width: parent.width
                color: theme.ink
                cursorDelegate: ClayCursor {}
                text: config.rootDir
                readOnly: true
                font.family: theme.fontMono
                font.pixelSize: 11
                background: FieldBg {}
            }
            Text {
                width: parent.width
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                wrapMode: Text.Wrap
                text: qsTr("Switching to a different root directory reloads the settings stored there.")
            }
            Text {
                visible: root.rootSwitchStatusMessage.length > 0
                width: parent.width
                color: theme.clay
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
                text: root.rootSwitchStatusMessage
            }
            // preset roots — same set as the first-launch DirPromptView
            Row {
                width: parent.width
                spacing: theme.sp2
                Repeater {
                    id: presetRep
                    model: config.presetRoots()
                    delegate: Rectangle {
                        width: (parent.width - parent.spacing * Math.max(0, presetRep.count - 1)) / Math.max(1, presetRep.count)
                        height: 34
                        radius: theme.rPill
                        border.color: config.rootDir === modelData ? theme.clay : theme.line
                        border.width: 1
                        color: config.rootDir === modelData ? theme.accent(0.08) : "transparent"
                        Text {
                            anchors.centerIn: parent
                            width: parent.width - theme.sp3
                            text: {
                                if (Qt.platform.os === "android") {
                                    if (index === 0)
                                        return qsTr("App private")
                                    if (index === 1)
                                        return qsTr("Android/data")
                                    return qsTr("Shared storage")
                                }
                                return modelData
                            }
                            color: config.rootDir === modelData ? theme.clayDeep : theme.inkSoft
                            font.family: theme.fontMono
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignHCenter
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: androidPermissionBridge.ensureRootAccessAndSetRoot(modelData)
                        }
                    }
                }
            }
            PathRow { label: qsTr("workspace");  path: config.workspacePath() }
            PathRow { label: qsTr("tools.jsonc"); path: config.toolsJsoncPath() }
            PathRow { label: qsTr("index.md");    path: config.indexMdPath() }
            PathRow { label: qsTr("skills");      path: config.skillsPath() }
            PathRow { label: qsTr("memories");    path: config.memoriesPath() }

            // =================== About ===================
            Text {
                text: qsTr("About")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }
            Item {
                width: parent.width
                height: aboutColumn.implicitHeight

                Column {
                    id: aboutColumn
                    width: parent.width
                    spacing: 0

                    Text {
                        width: parent.width
                        color: theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        text: qsTr("StarryAgent 0.3.1-alpha — cross-platform AI agent.\nChanges save automatically.")
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (settings.developerSettingsUnlocked)
                            return
                        root.developerTapCount += 1
                        if (root.developerTapCount >= 10) {
                            settings.developerSettingsUnlocked = true
                            root.themeUiError = qsTr("Developer Settings unlocked. Restart the app to show them.")
                        }
                    }
                }
            }

            Item {
                visible: root.developerSettingsVisibleThisSession
                width: parent.width
                height: developerSettingsColumn.implicitHeight

                Column {
                    id: developerSettingsColumn
                    width: parent.width
                    spacing: theme.sp3

                    Text {
                        text: qsTr("Developer Settings")
                        color: theme.inkSoft
                        font.family: theme.fontBody
                        font.pixelSize: 10
                        font.letterSpacing: 1
                        font.capitalization: Font.AllUppercase
                    }

                    ToggleRow {
                        title: qsTr("Enable Developer Settings")
                        description: qsTr("Required before Android theme package install can be enabled.")
                        checked: root.developerSettingsDraftEnabled
                        onToggled: root.developerSettingsDraftEnabled = checked
                    }

                    ToggleRow {
                        visible: Qt.platform.os === "android"
                        title: qsTr("Enable Theme On Android")
                        description: qsTr("Allows theme package install on Android after saving.")
                        checked: root.developerThemeOnAndroidDraftEnabled
                        enabled: root.developerSettingsDraftEnabled
                        onToggled: root.developerThemeOnAndroidDraftEnabled = checked
                    }

                    Row {
                        spacing: theme.sp2

                        ThemeButton {
                            text: qsTr("Save")
                            variant: "primary"
                            enabled: root.developerSettingsDirty
                            onClicked: {
                                settings.developerSettingsEnabled = root.developerSettingsDraftEnabled
                                settings.developerThemeOnAndroidEnabled = root.developerSettingsDraftEnabled
                                        ? root.developerThemeOnAndroidDraftEnabled
                                        : false
                                root.resetDeveloperDraftsToSaved()
                                root.themeUiError = qsTr("Developer settings saved.")
                            }
                        }

                        ThemeButton {
                            text: qsTr("Reset")
                            enabled: root.developerSettingsDraftEnabled || root.developerThemeOnAndroidDraftEnabled
                            onClicked: root.resetDeveloperDraftsToDefaults()
                        }
                    }
                }
            }
        }
    }

    ScheduledTasksView {
        visible: root.selectedTab === 3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
    }

    Connections {
        target: androidPermissionBridge
        function onRootApplied(path) {
            root.rootSwitchStatusMessage = qsTr("Switched root directory.")
            settings.load()
            toast.showMessage(root.rootSwitchStatusMessage)
        }
        function onErrorOccurred(message) {
            root.rootSwitchStatusMessage = message
            toast.showMessage(message)
        }
        function onPermissionRequestLaunched(message) {
            root.rootSwitchStatusMessage = message
            toast.showMessage(message)
        }
    }
    Connections {
        target: filePicker
        function onThemePackagePicked(path) {
            if (path && path.length > 0)
                themeManager.installTheme(path)
        }
        function onSkillPackagePicked(path) {
            if (path && path.length > 0)
                skillInstallManager.installSkillPackage(path)
        }
        function onErrorOccurred(message) {
            root.themeUiError = message
            root.skillUiError = message
        }
    }
    Connections {
        target: themeManager
        function onLastErrorChanged() {
            root.themeUiError = themeManager.lastError
        }
        function onThemeInstalled(themeId) {
            root.themeUiError = ""
        }
    }
    Connections {
        target: skillInstallManager
        function onLastErrorChanged() {
            root.skillUiError = skillInstallManager.lastError
        }
        function onSkillInstalled(skillId) {
            root.skillUiError = ""
        }
        function onSkillInstallFailed(error) {
            root.skillUiError = error
        }
    }
    Connections {
        target: androidBackgroundRuntime
        function onBatteryOptimizationIgnoredChanged() {
            androidBackgroundRuntime.refreshBatteryOptimizationState()
        }
        function onErrorOccurred(message) {
            root.androidBackgroundStatusMessage = message
            toast.showMessage(message)
        }
        function onRequestLaunched(message) {
            root.androidBackgroundStatusMessage = message
            toast.showMessage(message)
        }
    }

    Flickable {
        visible: root.selectedTab === 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: theme.sp4
        contentHeight: themeCol.implicitHeight + theme.sp6
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: themeCol
            width: Math.min(parent.width - theme.sp6 * 2, 720)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: theme.sp4

            Text {
                text: qsTr("Theme")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
            }

            Row {
                width: parent.width
                spacing: theme.sp2

                ThemeButton {
                    text: Qt.platform.os === "android"
                          ? (settings.developerSettingsEnabled && settings.developerThemeOnAndroidEnabled
                             ? qsTr("Install theme package")
                             : qsTr("Theme packages disabled"))
                          : qsTr("Install theme package")
                    variant: "primary"
                    enabled: Qt.platform.os !== "android"
                             || (settings.developerSettingsEnabled && settings.developerThemeOnAndroidEnabled)
                    onClicked: {
                        const path = filePicker.pickThemePackage()
                        if (path && path.length > 0)
                            themeManager.installTheme(path)
                    }
                }

                Text {
                    width: parent.width - 220
                    anchors.verticalCenter: parent.verticalCenter
                    text: Qt.platform.os === "android"
                          ? (settings.developerSettingsEnabled && settings.developerThemeOnAndroidEnabled
                             ? qsTr("Current: %1").arg(themeManager.currentThemeId)
                             : qsTr("Unlock About, restart, then enable Developer Settings and Theme On Android."))
                          : qsTr("Current: %1").arg(themeManager.currentThemeId)
                    color: theme.inkSoft
                    font.family: theme.fontMono
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    wrapMode: Text.Wrap
                }
            }

            Text {
                visible: root.themeUiError.length > 0
                width: parent.width
                text: root.themeUiError
                color: theme.clay
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Repeater {
                model: themeManager
                delegate: Rectangle {
                    required property string themeId
                    required property string name
                    required property string version
                    required property string author
                    required property string description
                    required property string previewPath
                    required property bool builtIn

                    readonly property bool compactCard: width < 440

                    width: themeCol.width
                    height: compactCard ? Math.max(140, compactContent.implicitHeight + theme.sp4)
                                        : Math.max(92, wideContent.implicitHeight + theme.sp4)
                    radius: theme.rMd
                    color: themeManager.currentThemeId === themeId
                           ? (theme.accent(theme.dark ? 0.10 : 0.08))
                           : theme.surface
                    border.width: 1
                    border.color: themeManager.currentThemeId === themeId ? theme.clay : theme.line

                    Row {
                        id: wideContent
                        visible: !compactCard
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: theme.sp3
                        spacing: theme.sp3

                        Rectangle {
                            width: 88
                            height: 56
                            radius: theme.rSm
                            color: theme.surfaceAlt
                            clip: true
                            Image {
                                anchors.fill: parent
                                source: previewPath
                                visible: previewPath.length > 0
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                        }

                        Column {
                            width: Math.max(120, parent.width - 88 - wideActions.width - theme.sp3 * 2)
                            spacing: theme.sp1
                            Text {
                                width: parent.width
                                text: name + (version.length > 0 ? ("  " + version) : "")
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: author
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: description
                                color: theme.inkSoft
                                font.family: theme.fontBody
                                font.pixelSize: 12
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }
                        }

                        Row {
                            id: wideActions
                            width: 168
                            spacing: theme.sp2
                            ThemeButton {
                                width: 76
                                text: themeManager.currentThemeId === themeId ? qsTr("Active") : qsTr("Use")
                                enabled: themeManager.currentThemeId !== themeId
                                variant: themeManager.currentThemeId === themeId ? "secondary" : "primary"
                                onClicked: themeManager.switchTheme(themeId)
                            }
                            ThemeButton {
                                width: 76
                                text: qsTr("Remove")
                                enabled: !builtIn
                                danger: true
                                onClicked: themeManager.uninstallTheme(themeId)
                            }
                        }
                    }

                    Column {
                        id: compactContent
                        visible: compactCard
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: theme.sp3
                        spacing: theme.sp3

                        Row {
                            width: parent.width
                            spacing: theme.sp3

                            Rectangle {
                                width: 88
                                height: 56
                                radius: theme.rSm
                                color: theme.surfaceAlt
                                clip: true
                                Image {
                                    anchors.fill: parent
                                    source: previewPath
                                    visible: previewPath.length > 0
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                            }

                            Column {
                                width: parent.width - 88 - theme.sp3
                                spacing: theme.sp1
                                Text {
                                    width: parent.width
                                    text: name + (version.length > 0 ? ("  " + version) : "")
                                    color: theme.ink
                                    font.family: theme.fontBody
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: author
                                    color: theme.inkSoft
                                    font.family: theme.fontMono
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                                Text {
                                    width: parent.width
                                    text: description
                                    color: theme.inkSoft
                                    font.family: theme.fontBody
                                    font.pixelSize: 12
                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        Row {
                            width: parent.width
                            spacing: theme.sp2
                            ThemeButton {
                                width: (parent.width - parent.spacing) / 2
                                text: themeManager.currentThemeId === themeId ? qsTr("Active") : qsTr("Use")
                                enabled: themeManager.currentThemeId !== themeId
                                variant: themeManager.currentThemeId === themeId ? "secondary" : "primary"
                                onClicked: themeManager.switchTheme(themeId)
                            }
                            ThemeButton {
                                width: (parent.width - parent.spacing) / 2
                                text: qsTr("Remove")
                                enabled: !builtIn
                                danger: true
                                onClicked: themeManager.uninstallTheme(themeId)
                            }
                        }
                    }
                }
            }
        }
    }

    Flickable {
        visible: root.selectedTab === 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: theme.sp4
        contentHeight: skillCol.implicitHeight + theme.sp6
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: skillCol
            width: Math.min(parent.width - theme.sp6 * 2, 720)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: theme.sp4

            Text {
                text: "Skill"
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
            }

            Column {
                width: parent.width
                spacing: theme.sp2

                ThemeButton {
                    text: qsTr("Import Skill Package")
                    variant: "primary"
                    onClicked: {
                        const path = filePicker.pickSkillPackage()
                        if (path && path.length > 0)
                            skillInstallManager.installSkillPackage(path)
                    }
                }

                Text {
                    width: parent.width
                    text: root.skillUiError.length > 0
                          ? root.skillUiError
                          : qsTr("Install location: %1").arg(config.skillsPath())
                    color: root.skillUiError.length > 0 ? theme.clay : theme.inkSoft
                    font.family: theme.fontMono
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }

            Text {
                visible: skillInstallManager.count === 0
                width: parent.width
                text: qsTr("No skills installed.")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Repeater {
                model: skillInstallManager
                delegate: Rectangle {
                    required property string skillId
                    required property string name
                    required property string description
                    required property string path
                    required property int referenceCount
                    required property bool enabled

                    readonly property bool compactCard: width < 440

                    width: skillCol.width
                    height: compactCard ? Math.max(148, compactSkillContent.implicitHeight + theme.sp4)
                                        : Math.max(104, wideSkillContent.implicitHeight + theme.sp4)
                    radius: theme.rMd
                    color: theme.surface
                    border.width: 1
                    border.color: theme.line

                    Row {
                        id: wideSkillContent
                        visible: !compactCard
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: theme.sp3
                        spacing: theme.sp3

                        Column {
                            width: Math.max(120, parent.width - skillWideActions.width - theme.sp3)
                            spacing: theme.sp1

                            Text {
                                width: parent.width
                                text: name.length > 0 ? name : skillId
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: skillId
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: description.length > 0 ? description : qsTr("No description")
                                color: theme.inkSoft
                                font.family: theme.fontBody
                                font.pixelSize: 12
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: qsTr("%1 references").arg(referenceCount)
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: path
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideMiddle
                            }
                        }

                        Row {
                            id: skillWideActions
                            width: 136
                            spacing: theme.sp2
                            anchors.verticalCenter: parent.verticalCenter

                            Column {
                                width: 44
                                spacing: 0
                                Components.MiuixSwitch {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    checked: enabled
                                    onToggled: skillInstallManager.setSkillEnabled(skillId, checked)
                                }
                            }

                            ThemeButton {
                                width: 76
                                text: qsTr("Remove")
                                danger: true
                                onClicked: skillInstallManager.uninstallSkill(skillId)
                            }
                        }
                    }

                    Column {
                        id: compactSkillContent
                        visible: compactCard
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: theme.sp3
                        spacing: theme.sp2

                        Text {
                            width: parent.width
                            text: name.length > 0 ? name : skillId
                            color: theme.ink
                            font.family: theme.fontBody
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: skillId
                            color: theme.inkSoft
                            font.family: theme.fontMono
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: description.length > 0 ? description : qsTr("No description")
                            color: theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 12
                            maximumLineCount: 3
                            wrapMode: Text.Wrap
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: qsTr("%1 references").arg(referenceCount)
                            color: theme.inkSoft
                            font.family: theme.fontMono
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                        Text {
                            width: parent.width
                            text: path
                            color: theme.inkSoft
                            font.family: theme.fontMono
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                        Row {
                            width: parent.width
                            spacing: theme.sp2
                            Item {
                                width: 44
                                height: compactToggleCol.implicitHeight
                                Column {
                                    id: compactToggleCol
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    spacing: 0
                                    Components.MiuixSwitch {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        checked: enabled
                                        onToggled: skillInstallManager.setSkillEnabled(skillId, checked)
                                    }
                                }
                            }
                            ThemeButton {
                                width: parent.width - 44 - parent.spacing
                                text: qsTr("Remove")
                                danger: true
                                onClicked: skillInstallManager.uninstallSkill(skillId)
                            }
                        }
                    }
                }
            }
        }
    }
}

