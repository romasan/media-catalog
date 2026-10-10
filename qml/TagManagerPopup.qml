import QtQuick
import QtQuick.Controls
import MediaCatalog

// Менеджер тегов.
//
// Два режима:
//   - обычный: все теги с числом применений, создание и удаление, применение
//     к открытому файлу;
//   - multiSelectMode: выбранный тег применяется ко всем выделенным файлам.
Item {
    id: root

    /// Идентификатор медиафайла для однофайлового режима.
    property string mediaId: ""
    /// Массовый режим: тег применяется к выделенным файлам.
    property bool multiSelectMode: false
    /// Теги открытого файла — чтобы отметить уже применённые.
    property var appliedIds: []
    /// Тег, выбранный в списке.
    property string selectedTagId: ""

    visible: false
    focus: true

    signal closed()

    function openFor(mediaIdValue) {
        multiSelectMode = false;
        mediaId = mediaIdValue;
        appliedIds = mediaId.length > 0 ? Library.mediaUserTagIds(mediaId) : [];
        show();
    }

    function openForSelection() {
        multiSelectMode = true;
        mediaId = "";
        appliedIds = [];
        show();
    }

    function show() {
        selectedTagId = "";
        visible = true;
        window.centerInParent();
        window.forceActiveFocus();
        searchField.forceActiveFocus();
    }

    function close() {
        visible = false;
        closed();
    }

    Keys.onEscapePressed: root.close()

    // Подложка: перехватывает клики по окну под попапом и закрывает его.
    Rectangle {
        anchors.fill: parent
        color: "#4d000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    function isApplied(tagId) {
        return appliedIds.indexOf(tagId) >= 0;
    }

    function createOrApply() {
        const name = searchField.text.trim();
        if (name.length === 0)
            return;

        if (multiSelectMode) {
            App.createTagAndApplyToSelection(name);
            close();
            return;
        }

        const tagId = Library.createTag(name);
        searchField.text = "";
        App.tagModel.query = "";
        if (mediaId.length > 0)
            toggleTag(tagId);
    }

    function toggleTag(tagId) {
        if (mediaId.length === 0)
            return;
        if (isApplied(tagId))
            Library.removeTagFromMedia(mediaId, tagId);
        else
            Library.applyTag(mediaId, tagId);
        appliedIds = Library.mediaUserTagIds(mediaId);
    }

    DraggableResizable {
        id: window
        width: 420
        height: 480
        title: root.multiSelectMode
               ? "Тег для " + App.selectedCount + " файлов"
               : "Менеджер тегов"
        minContentWidth: 320
        minContentHeight: 300
        onClosed: root.close()

        Column {
            anchors.fill: parent
            spacing: Theme.spacing

            TextField {
                id: searchField
                width: parent.width
                placeholderText: "Поиск или новый тег"

                onTextChanged: App.tagModel.query = text
                onAccepted: createOrApply()
            }

            Row {
                width: parent.width
                spacing: Theme.spacing

                Button {
                    text: "Создать"
                    enabled: searchField.text.trim().length > 0
                    onClicked: createOrApply()
                }

                Button {
                    text: "Удалить тег"
                    visible: root.selectedTagId.length > 0 && !root.multiSelectMode
                    onClicked: confirmDelete.open()
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Всего тегов: " + App.tagModel.count
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            ListView {
                id: tagList
                width: parent.width
                height: parent.height - y - Theme.spacing
                clip: true
                model: App.tagModel

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    id: tagRow
                    width: tagList.width
                    height: 32
                    color: rowMouse.containsMouse ? Theme.hover
                         : root.selectedTagId === tagRow.tagId ? Theme.accentSoft
                         : "transparent"

                    required property string tagId
                    required property string name
                    required property int count

                    readonly property bool applied: root.isApplied(tagRow.tagId)

                    // Отметка «тег уже применён к файлу».
                    Canvas {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                        height: 14
                        visible: tagRow.applied

                        onPaint: draw()

                        function draw() {
                            const ctx = getContext("2d");
                            ctx.reset();
                            ctx.strokeStyle = Theme.accent;
                            ctx.lineWidth = 2;
                            ctx.lineCap = "round";
                            ctx.beginPath();
                            ctx.moveTo(2, 7);
                            ctx.lineTo(6, 11);
                            ctx.lineTo(12, 3);
                            ctx.stroke();
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing * 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 80
                        text: tagRow.name
                        elide: Text.ElideRight
                        color: Theme.text
                        font.pixelSize: Theme.fontSizeNormal
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        text: tagRow.count
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.selectedTagId = tagRow.tagId;
                            if (root.multiSelectMode) {
                                // Массовый режим: клик сразу применяет тег.
                                App.applyTagToSelection(tagRow.tagId);
                                root.close();
                            }
                        }
                        onDoubleClicked: {
                            if (!root.multiSelectMode)
                                root.toggleTag(tagRow.tagId);
                        }
                    }
                }
            }

            Text {
                visible: App.tagModel.count === 0
                width: parent.width
                text: searchField.text.length > 0 ? "Ничего не найдено" : "Тегов пока нет"
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeNormal
            }
        }

        Dialog {
            id: confirmDelete
            anchors.centerIn: parent
            title: "Удаление тега"
            modal: true
            standardButtons: Dialog.Ok | Dialog.Cancel

            Text {
                text: "Удалить тег «" + App.library.tagName(root.selectedTagId) + "»?\n"
                      + "Тег будет снят со всех файлов."
                color: Theme.text
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeNormal
            }

            onAccepted: {
                App.library.deleteTag(root.selectedTagId);
                root.selectedTagId = "";
            }
        }
    }
}
