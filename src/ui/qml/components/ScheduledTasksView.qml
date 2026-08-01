import QtQuick
import QtQuick.Controls

Item {
    id: root

    Rectangle { anchors.fill: parent; color: theme.pageOverlay }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: theme.sp4
        contentHeight: content.implicitHeight + theme.sp6
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: Math.min(parent.width - theme.sp6 * 2, 720)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: theme.sp3

            Text {
                width: parent.width
                text: qsTr("任务会在应用后台保持运行，完成后会通知你，并把工具调用和回复留在对应会话中。")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                visible: Qt.platform.os === "android"
                width: parent.width
                text: qsTr("Android 设备还需要允许后台运行、电池优化例外，部分厂商还会额外限制自启动。去 设置 > 常规 > Background Runtime 里申请标准权限；如果仍然无效，请参照 Don’t Kill My App。")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            ToggleRow {
                title: qsTr("全局定时任务")
                description: qsTr("开关关闭时，所有定时任务暂停；开启后按计划继续运行。")
                checked: scheduledTasks.globalEnabled
                onToggled: scheduledTasks.globalEnabled = checked
            }

            Text {
                text: qsTr("任务")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 10
                font.letterSpacing: 1
                font.capitalization: Font.AllUppercase
                topPadding: theme.sp2
            }

            Repeater {
                model: scheduledTasks
                delegate: Rectangle {
                    required property string taskId
                    required property string prompt
                    required property string scope
                    required property string conversationTitle
                    required property string nextRun
                    required property int intervalMinutes
                    required property string recurrence
                    required property bool temp
                    required property bool enabled
                    required property string status
                    width: content.width
                    height: taskColumn.implicitHeight + theme.sp3 * 2
                    radius: theme.rSm
                    color: theme.surface
                    border.color: theme.line
                    border.width: 1

                    Column {
                        id: taskColumn
                        anchors.left: parent.left; anchors.right: parent.right
                        anchors.top: parent.top; anchors.margins: theme.sp3
                        spacing: theme.sp1

                        Row {
                            width: parent.width
                            spacing: theme.sp2

                            Text {
                                width: parent.width - taskSwitch.width - parent.spacing
                                text: conversationTitle.length > 0 ? conversationTitle : qsTr("已删除的会话")
                                color: theme.ink
                                font.family: theme.fontBody
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }

                            MiuixSwitch {
                                id: taskSwitch
                                anchors.verticalCenter: parent.verticalCenter
                                checked: enabled
                                onToggled: scheduledTasks.setTaskEnabled(taskId, checked)
                            }
                        }
                        Text {
                            width: parent.width
                            text: prompt
                            color: theme.inkSoft
                            font.family: theme.fontBody
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Row {
                            width: parent.width
                            spacing: theme.sp2
                            Text {
                                width: parent.width - deleteButton.width - parent.spacing
                                text: (scope === "global" ? qsTr("全局") : qsTr("此会话")) + "  |  "
                                      + qsTr("下次 %1").arg(nextRun.replace("T", " "))
                                      + "  |  "
                                      + (temp ? qsTr("临时单次")
                                                : recurrence === "daily" ? qsTr("每天")
                                                : recurrence === "weekly" ? qsTr("每周")
                                                : recurrence === "monthly" ? qsTr("每月")
                                                : recurrence === "interval" ? qsTr("每 %1 分钟").arg(intervalMinutes)
                                                : qsTr("单次"))
                                color: theme.inkSoft
                                font.family: theme.fontMono
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                            ThemeButton {
                                id: deleteButton
                                compact: true
                                danger: true
                                width: 32
                                height: 28
                                text: "\u00d7"
                                onClicked: scheduledTasks.deleteTask(taskId)
                            }
                        }
                    }
                }
            }

            Text {
                visible: scheduledTasks.count === 0
                width: parent.width
                topPadding: theme.sp5
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("还没有定时任务")
                color: theme.inkSoft
                font.family: theme.fontBody
                font.pixelSize: 13
            }
        }
    }
}
