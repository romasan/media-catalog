import QtQuick
import QtQuick.Controls
import QtMultimedia
import MediaCatalog

// Полноэкранный просмотр фото и видео.
//
// Навигация: ←/→ или клик по левой/правой половине экрана, Esc — выход.
// ПКМ открывает системное контекстное меню «Показать в проводнике».
Item {
    id: root

    /// Индекс файла в текущем списке, с которого начинается просмотр.
    property int startIndex: 0
    property int currentIndex: startIndex

    /// Дата съёмки из метаданных (0 — ещё не определена).
    property double capturedAt: 0

    readonly property var current: App.mediaModel.get(currentIndex)
    readonly property bool isVideo: current !== undefined && current.isVideo === true

    /// Обычные теги и метатеги текущего файла — для панели информации.
    readonly property var currentTags: {
        if (current === undefined || current.mediaId === undefined)
            return [];
        return Library.mediaTags(current.mediaId).concat(Library.mediaMetaTags(current.mediaId));
    }

    signal closed()

    function open() {
        currentIndex = startIndex;
        forceActiveFocus();
        loadCaptureDate();
    }

    function close() {
        mediaPlayer.stop();
        closed();
    }

    function step(delta) {
        const next = currentIndex + delta;
        if (next < 0 || next >= App.mediaModel.count)
            return;
        mediaPlayer.stop();
        currentIndex = next;
        loadCaptureDate();
    }

    function loadCaptureDate() {
        capturedAt = 0;
        if (current === undefined || current.mediaId === undefined)
            return;
        // Уже известное значение берём из модели, иначе запрашиваем у ядра.
        if (current.hasCapturedAt && current.capturedAt > 0) {
            capturedAt = current.capturedAt;
            return;
        }
        App.requestCaptureDate(current.mediaId);
    }

    // Значение, пришедшее от ядра после чтения метаданных.
    Connections {
        target: Library
        function onCaptureDateReady(mediaId, value, found) {
            if (!found || current === undefined || current.mediaId !== mediaId)
                return;
            capturedAt = value;
        }
    }

    anchors.fill: parent
    visible: false
    focus: true
    z: 100

    Rectangle {
        anchors.fill: parent
        color: "#0b0c0e"
    }

    // ==== Содержимое: фото или видео ====
    Image {
        id: photo
        anchors.fill: parent
        anchors.margins: 48
        visible: !root.isVideo
        source: root.isVideo || root.current === undefined ? "" : root.current.fileUrl
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        cache: false
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        anchors.margins: 48
        visible: root.isVideo
        fillMode: VideoOutput.PreserveAspectFit
    }

    MediaPlayer {
        id: mediaPlayer
        videoOutput: videoOutput
        audioOutput: AudioOutput { volume: 0.8 }
        source: root.isVideo && root.current !== undefined ? root.current.fileUrl : ""
        loops: MediaPlayer.Infinite

        onSourceChanged: {
            if (root.isVideo) {
                position = 0;
                play();
            }
        }
    }

    // Клик по половинам экрана — листание; ПКМ — контекстное меню.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                mediaMenu.popup();
                return;
            }
            if (mouse.x < width / 2)
                root.step(-1);
            else
                root.step(1);
        }
        onDoubleClicked: root.close()
    }

    // ==== Информация о файле ====
    Rectangle {
        id: infoPanel
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Theme.spacing * 2
        width: Math.min(infoColumn.implicitWidth + 2 * Theme.spacing, root.width - 200)
        height: infoColumn.implicitHeight + 2 * Theme.spacing
        color: "#cc1d2026"
        radius: Theme.radius
        border.color: Theme.border

        Column {
            id: infoColumn
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Theme.spacing
            spacing: 3

            Text {
                text: root.current !== undefined ? root.current.name : ""
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLarge
                font.bold: true
            }

            Text {
                width: infoPanel.width - 2 * Theme.spacing
                text: root.current !== undefined ? root.current.path : ""
                color: Theme.textMuted
                elide: Text.ElideMiddle
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            Text {
                text: {
                    if (root.current === undefined)
                        return "";
                    let line = App.formatDateTime(root.current.modifiedAt);
                    if (root.capturedAt > 0
                            && Math.abs(root.capturedAt - root.current.modifiedAt) > 60000) {
                        line += "   •   съёмка: " + App.formatDateTime(root.capturedAt);
                    }
                    line += "   •   " + root.current.sizeText;
                    return line;
                }
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            // Теги файла: обычные и метатеги.
            Row {
                spacing: 4
                visible: tagRepeater.count > 0

                Repeater {
                    id: tagRepeater
                    model: root.currentTags

                    delegate: Rectangle {
                        required property var modelData
                        width: tagLabel.implicitWidth + Theme.spacing
                        height: 18
                        radius: 9
                        color: modelData.isMeta ? Theme.surfaceAlt : Theme.accentSoft

                        Text {
                            id: tagLabel
                            anchors.centerIn: parent
                            text: modelData.name
                            color: modelData.isMeta ? Theme.textMuted : Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 10
                        }
                    }
                }
            }
        }
    }

    // Счётчик позиции в списке.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spacing * 2
        width: counterText.implicitWidth + 2 * Theme.spacing
        height: 26
        radius: 13
        color: "#cc1d2026"

        Text {
            id: counterText
            anchors.centerIn: parent
            text: (root.currentIndex + 1) + " / " + App.mediaModel.count
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    // Контекстное меню с системным пунктом.
    Menu {
        id: mediaMenu

        MenuItem {
            text: "Показать в проводнике"
            enabled: root.current !== undefined
            onTriggered: App.revealInFileManager(root.current.path)
        }
        MenuItem {
            text: "Добавить тег…"
            enabled: root.current !== undefined
            onTriggered: tagManager.openFor(root.current.mediaId)
        }
        MenuItem {
            text: "Добавить метатег в фильтр"
            enabled: root.current !== undefined
            onTriggered: {
                const tags = Library.mediaMetaTags(root.current.mediaId);
                for (let i = 0; i < tags.length; ++i)
                    Library.addTagToFilter(tags[i].id);
            }
        }
    }

    TagManagerPopup {
        id: tagManager
        anchors.fill: parent
    }

    // ==== Клавиатура ====
    Keys.onPressed: function (event) {
        switch (event.key) {
        case Qt.Key_Escape:
        case Qt.Key_Backspace:
            root.close();
            event.accepted = true;
            break;
        case Qt.Key_Left:
            root.step(-1);
            event.accepted = true;
            break;
        case Qt.Key_Right:
        case Qt.Key_Space:
            root.step(1);
            event.accepted = true;
            break;
        case Qt.Key_Home:
            mediaPlayer.stop();
            root.currentIndex = 0;
            root.loadCaptureDate();
            event.accepted = true;
            break;
        case Qt.Key_End:
            mediaPlayer.stop();
            root.currentIndex = App.mediaModel.count - 1;
            root.loadCaptureDate();
            event.accepted = true;
            break;
        default:
            break;
        }
    }

    Component.onCompleted: visible = true
}
