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

        // Заглушка, пока превью не готово: значок типа файла.
        Loader {
            anchors.centerIn: parent
            visible: preview.status !== Image.Ready
            width: Math.max(20, Math.min(root.width, root.height) * 0.3)
            height: width

            sourceComponent: root.video ? videoGlyph : photoGlyph

            Component {
                id: videoGlyph
                VideoGlyph { color: Theme.textDim }
            }
            Component {
                id: photoGlyph
                PhotoGlyph { color: Theme.textDim }
            }
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

            PlayGlyph {
                anchors.centerIn: parent
                width: 10
                height: 10
                color: "#ffffff"
            }
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onClicked: function (mouse) {
                if (mouse.button === Qt.RightButton) {
                    // На macOS Ctrl+клик приходит как правый клик, но означает
                    // «добавить к выделению». Обычный правый клик ничего не
                    // делает — в версии 1.x меню было только в просмотре.
                    if (mouse.modifiers & Qt.ControlModifier
                            || mouse.modifiers & Qt.MetaModifier) {
                        root.clicked(mouse);
                    } else {
                        root.rightClicked(mouse);
                    }
                    return;
                }

                // Ctrl/Cmd+клик и Shift+клик всегда означают работу с выделением.
                root.clicked(mouse);
            }
        }
    }

    AppToolTip {
        target: hoverArea
        text: root.name
        delay: 700
        // Подсказка только для видимой карточки: элемент под другим окном
        // (просмотром или попапом) не должен её показывать.
        visible: target.containsMouse && root.name.length > 0 && root.visible
                 && root.Window.active
    }
}
