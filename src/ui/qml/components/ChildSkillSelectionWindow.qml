import QtQuick
import QtQuick.Controls
import StarryAgent 1.0

StarryWindow {
    id: root

    width: 560
    height: 620
    minimumWidth: 420
    minimumHeight: 360
    visible: false
    modality: Qt.ApplicationModal
    flags: Qt.Dialog
    color: theme.paper
    title: qsTr("Choose child skill")

    property bool dark: false

    function openWindow() {
        visible = true
        raise()
        requestActivate()
    }

    function closeWindow() {
        visible = false
    }

    onVisibleChanged: {
        if (!visible)
            skillInstallManager.clearPendingChildSelection()
    }

    Rectangle {
        anchors.fill: parent
        color: theme.paper
    }

    Column {
        anchors.fill: parent
        anchors.margins: theme.sp4
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
            height: parent.height - closeRow.height - y - theme.sp3
            radius: theme.rLg
            color: theme.surface
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
                    color: theme.surfaceAlt
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
            id: closeRow
            width: parent.width
            layoutDirection: Qt.RightToLeft
            spacing: theme.sp2

            ThemeButton {
                text: qsTr("Done")
                onClicked: root.closeWindow()
            }
        }
    }
}
