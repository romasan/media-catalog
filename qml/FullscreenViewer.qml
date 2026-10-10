import QtQuick
import QtQuick.Controls
import QtMultimedia
import MediaCatalog

// Полноэкранный просмотр фото и видео.
//
// Повторяет вид и поведение версии 1.x:
//   - содержимое занимает весь экран, поверх него лежат элементы управления;
//   - кнопки ◀ и ▶ по бокам, крестик закрытия в правом верхнем углу;
//   - нижняя панель: путь, дата, размер и теги файла;
//   - клик по тегу добавляет его в глобальный фильтр, крестик на теге снимает
//     тег с файла, «+» открывает ввод нового тега с автодополнением;
//   - ПКМ показывает системное меню «Показать в проводнике» (выделяет файл).
Item {
    id: root

    /// Индекс файла в текущем списке, с которого начинается просмотр.
    property int startIndex: 0
    property int currentIndex: startIndex

    /// Дата съёмки из метаданных (0 — ещё не определена).
    property double capturedAt: 0

    readonly property var current: App.mediaModel.get(currentIndex)
    readonly property bool isVideo: current !== undefined && current.isVideo === true
    readonly property string currentId: current !== undefined && current.mediaId !== undefined
                                           ? current.mediaId : ""

    /// Пользовательские теги и метатеги текущего файла.
    ///
    /// App.tagRevision читается намеренно: методы ядра — обычные функции, и без
    /// такой зависимости QML не пересчитает биндинг после применения тега,
    /// поэтому чип появлялся только при повторном открытии файла.
    readonly property var userTags: {
        App.tagRevision;
        return currentId.length > 0 ? Library.userTags(currentId) : [];
    }
    readonly property var metaTags: {
        App.tagRevision;
        return currentId.length > 0 ? Library.mediaMetaTags(currentId) : [];
    }

    signal closed()

    function open() {
        currentIndex = startIndex;
        visible = true;
        forceActiveFocus();
        loadCaptureDate();
        App.tagModel.query = "";
    }

    function close() {
        mediaPlayer.stop();
        visible = false;
        closed();
    }

    function step(delta) {
        const next = currentIndex + delta;
        if (next < 0 || next >= App.mediaModel.count)
            return;
        mediaPlayer.stop();
        currentIndex = next;
        tagInput.closeEditor();
        loadCaptureDate();
    }

    function loadCaptureDate() {
        capturedAt = 0;
        if (currentId.length === 0)
            return;
        // Уже известное значение берём из модели, иначе запрашиваем у ядра.
        if (current.hasCapturedAt && current.capturedAt > 0) {
            capturedAt = current.capturedAt;
            return;
        }
        App.requestCaptureDate(currentId);
    }

    // Значение, пришедшее от ядра после чтения метаданных.
    Connections {
        target: Library
        function onCaptureDateReady(mediaId, value, found) {
            if (!found || mediaId !== root.currentId)
                return;
            root.capturedAt = value;
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

    // ==== Слой медиа ====
    Item {
        id: mediaLayer
        anchors.fill: parent
        anchors.bottomMargin: infoBar.visible ? infoBar.height : 0

        Image {
            id: photo
            anchors.fill: parent
            anchors.margins: 24
            visible: !root.isVideo
            source: root.isVideo || root.current === undefined ? "" : root.current.fileUrl
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: false
        }

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            anchors.margins: 24
            visible: root.isVideo
            fillMode: VideoOutput.PreserveAspectFit
        }

        // Клик по медиа: половины экрана листают, ПКМ открывает меню.
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

    // ==== Кнопки навигации ====
    NavButton {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 12
        z: 10
        direction: "left"
        enabled: root.currentIndex > 0
        onClicked: root.step(-1)
    }

    NavButton {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 12
        z: 10
        direction: "right"
        enabled: root.currentIndex < App.mediaModel.count - 1
        onClicked: root.step(1)
    }

    // ==== Кнопка закрытия ====
    Rectangle {
        id: closeButton
        objectName: "viewerCloseButton"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        z: 20
        width: 40
        height: 40
        radius: 20
        color: closeArea.containsMouse ? Theme.danger : "#cc1d2026"
        border.color: Theme.border

        CloseGlyph {
            anchors.centerIn: parent
            width: 16
            height: 16
            color: "#ffffff"
        }

        MouseArea {
            id: closeArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.close()
        }

        ToolTip.text: "Закрыть (Esc)"
        ToolTip.visible: closeArea.containsMouse
        ToolTip.delay: 500
    }

    // Счётчик позиции в списке.
    Rectangle {
        anchors.right: closeButton.left
        anchors.top: parent.top
        anchors.topMargin: 16
        anchors.rightMargin: 8
        z: 20
        width: counterText.implicitWidth + 2 * Theme.spacing
        height: 40
        radius: 20
        color: "#cc1d2026"

        Text {
            id: counterText
            anchors.centerIn: parent
            text: (root.currentIndex + 1) + " / " + App.mediaModel.count
            color: Theme.textMuted
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    // ==== Нижняя панель: путь, метаданные и теги ====
    Rectangle {
        id: infoBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 10
        height: infoColumn.implicitHeight + 2 * Theme.spacing
        color: "#e61d2026"

        Column {
            id: infoColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Theme.spacing
            spacing: 4

            Text {
                width: parent.width
                text: root.current !== undefined ? root.current.path : ""
                color: Theme.textMuted
                elide: Text.ElideMiddle
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
                font.pixelSize: Theme.fontSizeSmall
            }

            // Теги файла: метатеги, пользовательские теги, ввод нового тега.
            Flow {
                id: tagFlow
                width: parent.width
                spacing: 6

                Repeater {
                    model: root.metaTags

                    delegate: Rectangle {
                        required property var modelData

                        width: metaLabel.implicitWidth + 2 * Theme.spacing
                        height: 22
                        radius: 11
                        color: Theme.surfaceAlt

                        Text {
                            id: metaLabel
                            anchors.centerIn: parent
                            text: modelData.name
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Library.addTagToFilter(modelData.id)
                        }

                        ToolTip.text: "Добавить «" + modelData.name + "» в фильтр"
                        ToolTip.visible: metaMouse.containsMouse
                        ToolTip.delay: 600

                        MouseArea {
                            id: metaMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                Repeater {
                    model: root.userTags

                    delegate: Rectangle {
                        required property var modelData

                        width: tagRow.implicitWidth + Theme.spacing
                        height: 22
                        radius: 11
                        color: Theme.accentSoft

                        Row {
                            id: tagRow
                            anchors.centerIn: parent
                            spacing: 4

                            // Клик по имени добавляет тег в глобальный фильтр.
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fontSizeSmall

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Library.addTagToFilter(modelData.id)
                                }
                            }

                            // Крестик снимает тег с файла.
                            CloseGlyph {
                                id: removeGlyph
                                anchors.verticalCenter: parent.verticalCenter
                                width: 9
                                height: 9
                                color: removeArea.containsMouse ? Theme.danger : "#99ffffff"

                                MouseArea {
                                    id: removeArea
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    hoverEnabled: true
                                    onClicked: Library.removeTagFromMedia(root.currentId, modelData.id)
                                }
                            }
                        }

                        ToolTip.text: "Добавить «" + modelData.name + "» в фильтр"
                        ToolTip.visible: tagHover.containsMouse
                        ToolTip.delay: 600

                        MouseArea {
                            id: tagHover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }

                // Кнопка «+» и поле ввода нового тега.
                Item {
                    id: tagInput
                    width: editor.visible ? editor.width : addButton.width
                    height: 22

                    /// Открыт ли ввод нового тега.
                    readonly property bool editorVisible: editor.visible

                    function openEditor() {
                        editor.visible = true;
                        App.tagModel.query = "";
                        input.forceActiveFocus();
                    }

                    function closeEditor() {
                        editor.visible = false;
                        input.text = "";
                        App.tagModel.query = "";
                    }

                    function applyName() {
                        const name = input.text.trim();
                        if (name.length === 0)
                            return;
                        // Существующий тег применяем, новый — создаём и применяем.
                        const tagId = Library.createTag(name);
                        Library.applyTag(root.currentId, tagId);
                        closeEditor();
                    }

                    // Кнопка «+».
                    Rectangle {
                        id: addButton
                        visible: !editor.visible
                        width: 22
                        height: 22
                        radius: 11
                        color: addArea.containsMouse ? Theme.accent : Theme.surfaceAlt

                        Text {
                            anchors.centerIn: parent
                            text: "+"
                            color: addArea.containsMouse ? "#ffffff" : Theme.text
                            font.pixelSize: 14
                        }

                        MouseArea {
                            id: addArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: tagInput.openEditor()
                        }

                        ToolTip.text: "Добавить тег"
                        ToolTip.visible: addArea.containsMouse
                        ToolTip.delay: 500
                    }

                    // Поле ввода с подсказками.
                    Item {
                        id: editor
                        visible: false
                        width: 220
                        height: 22

                        TextField {
                            id: input
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: parent.height
                            placeholderText: "Новый тег…"
                            font.pixelSize: Theme.fontSizeSmall
                            leftPadding: Theme.spacing
                            rightPadding: Theme.spacing

                            onTextChanged: App.tagModel.query = text
                            onAccepted: tagInput.applyName()

                            Keys.onDownPressed: {
                                if (App.tagModel.count > 0)
                                    suggestionList.currentIndex = Math.min(
                                        suggestionList.currentIndex + 1, App.tagModel.count - 1);
                            }
                            Keys.onUpPressed: {
                                suggestionList.currentIndex = Math.max(
                                    suggestionList.currentIndex - 1, 0);
                            }
                        }

                        // Подсказки появляются, когда введён хотя бы символ.
                        Rectangle {
                            id: suggestions
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.topMargin: -Math.min(suggestionList.contentHeight, 132) - 6
                            width: parent.width
                            height: visible ? Math.min(suggestionList.contentHeight, 132) : 0
                            visible: input.text.trim().length > 0 && App.tagModel.count > 0
                            color: Theme.surface
                            radius: Theme.radius
                            border.color: Theme.border
                            clip: true

                            ListView {
                                id: suggestionList
                                anchors.fill: parent
                                model: App.tagModel
                                clip: true

                                delegate: Rectangle {
                                    required property string tagId
                                    required property string name
                                    required property int index

                                    width: suggestionList.width
                                    height: 24
                                    color: index === suggestionList.currentIndex
                                           ? Theme.accentSoft
                                           : "transparent"

                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: Theme.spacing
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: parent.name
                                        color: Theme.text
                                        font.pixelSize: Theme.fontSizeSmall
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            // Существующий тег просто применяем.
                                            Library.applyTag(root.currentId, parent.tagId);
                                            tagInput.closeEditor();
                                        }
                                    }
                                }
                            }
                        }

                        Keys.onEscapePressed: tagInput.closeEditor()
                    }
                }
            }
        }
    }

    // ==== Контекстное меню файла ====
    Menu {
        id: mediaMenu

        MenuItem {
            text: "Показать в проводнике"
            enabled: root.current !== undefined
            onTriggered: App.revealInFileManager(root.current.path)
        }
    }

    // ==== Клавиатура ====
    Keys.onPressed: function (event) {
        if (tagInput.editorVisible)
            return;
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

    /// Пока открыт ввод тега, стрелки и Esc принадлежат полю ввода.
    function editorHasFocus() {
        return tagInput.editorVisible;
    }

    ToastContainer {
        id: viewerToasts
        anchors.fill: parent
        z: 40
    }

    Connections {
        target: Library
        function onToast(message, type) {
            viewerToasts.show(message, type);
        }
    }
}
