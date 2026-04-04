import QtQuick
import QtQuick.Window

Window {
    visible: true
    width: 400
    height: 800
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property var notifications: []

    Connections {
        target: notifServer
        function onNewNotification(summary, body) {
            notifications.push({summary: summary, body: body})
        }
    }

    Column {
        anchors.right: parent.right
        spacing: 10

        Repeater {
            model: notifications

            Rectangle {
                width: 300
                height: 80
                radius: 10
                color: "#1e1e2e"

                property bool visibleState: false

                x: parent.width

                Behavior on x {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }

                Component.onCompleted: {
                    visibleState = true
                    x = parent.width - width - 10

                    // auto remove
                    Qt.callLater(() => {
                        visibleState = false
                        x = parent.width
                    })
                }

                Text {
                    anchors.centerIn: parent
                    text: modelData.summary + ": " + modelData.body
                    color: "white"
                }
            }
        }
    }
}
