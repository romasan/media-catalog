import QtQuick
import QtQuick.Controls
import MediaCatalog

// Полоса прогресса генерации превью.
Rectangle {
    id: root

    property real progress: 0
    property bool active: false

    visible: active
    height: 3
    color: "transparent"

    Rectangle {
        height: parent.height
        width: parent.width * Math.max(0, Math.min(1, root.progress))
        color: Theme.accent

        Behavior on width {
            NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
        }
    }
}
