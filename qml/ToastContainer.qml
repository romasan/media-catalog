import QtQuick
import QtQuick.Controls
import MediaCatalog

// Всплывающие уведомления в правом нижнем углу.
// Показывает не больше трёх сообщений одновременно.
Item {
    id: root

    property var toasts: []

    /// Добавляет сообщение и снимает его через 4 секунды.
    function show(message, type) {
        const list = root.toasts.slice();
        list.push({
            "message": message,
            "type": type === undefined ? "info" : type,
            "key": Date.now() + Math.random()
        });
        root.toasts = list.slice(-3);

        toastRemoval.interval = 4000;
        toastRemoval.restart();
    }

    function dismiss(key) {
        root.toasts = root.toasts.filter(function (t) { return t.key !== key; });
    }

    Timer {
        id: toastRemoval
        repeat: true
        onTriggered: {
            if (root.toasts.length === 0) {
                stop();
                return;
            }
            root.toasts = root.toasts.slice(1);
        }
    }

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 2 * Theme.spacing
        spacing: Theme.spacing

        Repeater {
            model: root.toasts

            delegate: Rectangle {
                required property var modelData

                width: Math.min(toastText.implicitWidth + 3 * Theme.spacing, 420)
                height: toastText.implicitHeight + 2 * Theme.spacing
                radius: Theme.radius
                color: modelData.type === "error" ? Theme.danger
                     : modelData.type === "success" ? Theme.success
                     : Theme.surfaceAlt
                border.color: Theme.border

                Text {
                    id: toastText
                    anchors.centerIn: parent
                    width: parent.width - 2 * Theme.spacing
                    text: modelData.message
                    color: modelData.type === "info" ? Theme.text : "#ffffff"
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeNormal
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.dismiss(modelData.key)
                }
            }
        }
    }
}
