import QtQuick
import QtQuick.Controls
import MediaCatalog

// Карточка одного медиафайла в сетке.
Item {
    id: root

    property string mediaId: ""
    property string name: ""
    property bool video: false
    property bool selected: false
    property string thumbnailUrl: ""

    signal clicked(var mouse)
    signal rightClicked(var mouse)

    Rectangle {
        id: frame
        anchors.fill: parent
        anchors.margins: 2
        color: Theme.surfaceAlt
        radius: 3
        border.width: root.selected ? 2 : 1
        border.color: root.selected ? Theme.selectedBorder
                     : hoverArea.containsMouse ? Theme.textDim
                     : Theme.border

        // Превью грузится асинхронно и обрезается по центру квадрата.
        Image {
            id: preview
            anchors.fill: parent
            anchors.margins: frame.border.width
            source: root.thumbnailUrl
            asynchronous: true
            cache: true
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 400
            sourceSize.height: 400
            visible: status === Image.Ready
        }

        // Заглушка, пока превью не готово.
        Text {
            anchors.centerIn: parent
            visible: preview.status !== Image.Ready
            text: root.video ? "🎬" : "🖼️"
            font.pixelSize: Math.max(20, Math.min(root.width, root.height) * 0.3)
            opacity: 0.55
        }

        // Значок видео поверх превью.
        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 6
            visible: root.video && preview.status === Image.Ready
            width: 22
            height: 22
            radius: 11
            color: "#99000000"

            Text {
                anchors.centerIn: parent
                text: "▶"
                color: "#ffffff"
                font.pixelSize: 11
            }
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onClicked: function (mouse) {
                if (mouse.button === Qt.RightButton)
                    root.rightClicked(mouse);
                else
                    root.clicked(mouse);
            }
        }
    }

    ToolTip {
        text: root.name
        visible: hoverArea.containsMouse && root.name.length > 0
        delay: 700
    }
}
