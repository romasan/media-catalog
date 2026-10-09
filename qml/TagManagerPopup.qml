import QtQuick
import QtQuick.Controls
import MediaCatalog

// Менеджер тегов.
//
// Два режима:
//   - обычный: показать все теги с числом применений, создать или удалить тег;
//   - multiSelectMode: добавить тег к выделенным файлам одним кликом.
Item {
    id: root

    /// Идентификатор медиафайла для обычного (однофайлового) режима.
    property string mediaId: ""
    /// Режим массового тегирования выделенных файлов.
    property bool multiSelectMode: false

    visible: false

    /// Теги текущего файла — чтобы пометить уже применённые.
    property var appliedIds: []
    /// Тег, выбранный в списке (для удаления).
    property string selectedTagId: ""

    function openFor(mediaIdValue) {
        mediaId = mediaIdValue;
        appliedIds = mediaId.length > 0 ? Library.mediaUserTagIds(mediaId) : [];
        visible = true;
        window.centerInParent();
        searchField.forceActiveFocus();
    }

    function openForSelection() {
        multiSelectMode = true;
        mediaId = "";
        appliedIds = [];
        visible = true;
        window.centerInParent();
        searchField.forceActiveFocus();
    }

    function isApplied(tagId) {
        return appliedIds.indexOf(tagId) >= 0;
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
                    font.family: Theme.fontFamily
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
                    color: rowMouse.containsMouse ? Theme.hover : "transparent"

                    required property string tagId
                    required property string name
                    required property int count

                    readonly property bool applied: root.isApplied(tagRow.tagId)
                    readonly property bool selected: root.selectedTagId === tagRow.tagId

                    Rectangle {
                        anchors.fill: parent
                        visible: tagRow.selected
                        color: Theme.accentSoft
                    }

                    // Отметка «тег уже применён к файлу».
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        width: 16
                        text: tagRow.applied ? "✓" : ""
                        color: Theme.accent
                        font.pixelSize: 13
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.spacing * 3
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 80
                        text: tagRow.name
                        elide: Text.ElideRight
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeNormal
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        text: tagRow.count
                        color: Theme.textDim
                        font.family: Theme.fontFamily
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
                                root.visible = false;
                            }
                        }
                        onDoubleClicked: {
                            if (!root.multiSelectMode)
                                toggleTag(tagRow.tagId);
                        }
                    }
                }
            }

            Text {
                visible: App.tagModel.count === 0
                width: parent.width
                text: searchField.text.length > 0
                      ? "Ничего не найдено"
                      : "Тегов пока нет"
                color: Theme.textMuted
                horizontalAlignment: Text.AlignHCenter
                font.family: Theme.fontFamily
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
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeNormal
            }

            onAccepted: {
                App.library.deleteTag(root.selectedTagId);
                root.selectedTagId = "";
            }
        }
    }

    function createOrApply() {
        const name = searchField.text.trim();
        if (name.length === 0)
            return;

        if (multiSelectMode) {
            App.createTagAndApplyToSelection(name);
            visible = false;
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
}
