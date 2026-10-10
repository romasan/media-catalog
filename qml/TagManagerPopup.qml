import QtQuick
import QtQuick.Controls
import MediaCatalog

// Управление тегами.
//
// Повторяет вид и поведение версии 1.x:
//   - сверху метатеги, сгруппированные по виду (годы, времена года, тип файла,
//     «без тега»): клик по метатегу добавляет его в фильтр;
//   - поиск с кнопкой «Создать» (кнопка появляется, если такого тега ещё нет);
//   - список тегов: имя, число файлов и кнопка удаления;
//   - клик по строке тега добавляет тег в фильтр;
//   - тег, присвоенный файлам, удаляется только после подтверждения.
PopupFrame {
    id: root

    /// Идентификатор медиафайла для однофайлового режима.
    property string mediaId: ""
    /// Массовый режим: тег применяется к выделенным файлам.
    property bool multiSelectMode: false
    /// Теги открытого файла — чтобы отметить уже применённые.
    property var appliedIds: []
    /// Тег, выбранный в списке.
    property string selectedTagId: ""
    /// Тег, ожидающий подтверждения удаления.
    property string pendingDeleteId: ""
    property int pendingDeleteCount: 0

    objectName: "tagManager"

    title: multiSelectMode ? "Тег для " + App.selectedCount + " файлов" : "Управление тегами"
    frameWidth: 460
    frameHeight: 520

    /// Общая точка входа: открыть менеджер тегов для выделенных файлов.
    function open() {
        openForSelection();
    }

    function openFor(mediaIdValue) {
        multiSelectMode = false;
        mediaId = mediaIdValue;
        appliedIds = mediaId.length > 0 ? Library.mediaUserTagIds(mediaId) : [];
        selectedTagId = "";
        baseOpen();
        searchField.forceActiveFocus();
    }

    function openForSelection() {
        multiSelectMode = true;
        mediaId = "";
        appliedIds = [];
        selectedTagId = "";
        baseOpen();
        searchField.forceActiveFocus();
    }

    function isApplied(tagId) {
        return appliedIds.indexOf(tagId) >= 0;
    }

    /// Клик по тегу или метатегу — добавить условие в фильтр.
    function clickTag(tagId) {
        Library.addTagToFilter(tagId);
    }

    function createTag() {
        const name = searchField.text.trim();
        if (name.length === 0)
            return;

        if (multiSelectMode) {
            // Массовый режим: создаём тег и сразу применяем к выделенным файлам.
            App.createTagAndApplyToSelection(name);
            searchField.text = "";
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

    /// Нажать кнопку удаления в строке списка.
    /// Нужно там, где строку нельзя достать напрямую: делегаты Repeater
    /// не видны в дереве объектов QML из C++.
    function deleteTagAt(row) {
        const item = tagList.itemAtIndex(row);
        if (!item)
            return false;
        item.deleteRequested();
        return true;
    }

    /// Подтвердить удаление — то же, что нажать «Удалить» в диалоге.
    function confirmDeleteNow() {
        confirmDelete.accept();
    }

    /// Удаление тега: если он присвоен файлам — сначала подтверждение.
    function requestDelete(tagId, count) {
        if (count > 0) {
            pendingDeleteId = tagId;
            pendingDeleteCount = count;
            confirmDelete.open();
            return;
        }
        Library.deleteTag(tagId);
    }

    /// Метатеги, сгруппированные по виду.
    readonly property var metaGroups: {
        App.tagRevision;
        const order = ["Годы", "Времена года", "Тип файла", "Прочее"];
        const all = Library.metaTags();
        const groups = [];
        for (let i = 0; i < order.length; ++i) {
            const items = [];
            for (let j = 0; j < all.length; ++j) {
                if (all[j].group === order[i])
                    items.push(all[j]);
            }
            if (items.length > 0)
                groups.push({ "group": order[i], "items": items });
        }
        return groups;
    }

    /// Есть ли уже тег с введённым именем.
    readonly property bool tagExists: {
        App.tagRevision;
        const name = searchField.text.trim().toLowerCase();
        if (name.length === 0)
            return false;
        const all = Library.allTags();
        for (let i = 0; i < all.length; ++i) {
            if (all[i].name.toLowerCase() === name)
                return true;
        }
        return false;
    }

    Column {
        anchors.fill: parent
        spacing: Theme.spacing

        // ==== Метатеги по группам ====
        Column {
            id: metaSection
            width: parent.width
            spacing: 8
            visible: root.metaGroups.length > 0

            Repeater {
                model: root.metaGroups

                delegate: Column {
                    required property var modelData
                    width: metaSection.width
                    spacing: 4

                    Text {
                        text: modelData.group.toUpperCase()
                        color: Theme.textDim
                        font.pixelSize: 11
                        font.letterSpacing: 0.5
                    }

                    Flow {
                        width: metaSection.width - 20
                        spacing: 4

                        Repeater {
                            model: modelData.items

                            delegate: Rectangle {
                                required property var modelData

                                width: metaChipLabel.implicitWidth + 20
                                height: 22
                                radius: 12
                                color: metaChipArea.containsMouse ? "#4a4a4a" : "#3a3a3a"

                                Text {
                                    id: metaChipLabel
                                    anchors.centerIn: parent
                                    text: modelData.name
                                    color: metaChipArea.containsMouse ? "#dddddd" : "#aaaaaa"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    id: metaChipArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.clickTag(modelData.id)
                                }

                                AppToolTip {
                                    text: "Добавить «" + modelData.name + "» в фильтр"
                                    visible: metaChipArea.containsMouse
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==== Поиск и создание ====
        Row {
            width: parent.width
            spacing: Theme.spacing

            TextField {
                id: searchField
                width: parent.width - (createButton.visible ? createButton.width + Theme.spacing : 0)
                placeholderText: "Поиск или создание тега…"

                onTextChanged: App.tagModel.query = text
                onAccepted: root.createTag()
            }

            Button {
                id: createButton
                // Кнопка создания появляется, только если такого тега ещё нет.
                visible: searchField.text.trim().length > 0 && !root.tagExists
                text: "Создать"
                onClicked: root.createTag()
            }
        }

        // ==== Список тегов ====
        ListView {
            id: tagList
            width: parent.width
            height: Math.max(60, parent.height - y - totalLabel.height - Theme.spacing)
            clip: true
            model: App.tagModel
            spacing: 4

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: tagRow
                width: tagList.width
                height: 32
                radius: 6
                color: rowMouse.containsMouse ? "#333333" : "#2c2c2c"

                required property string tagId
                required property string name
                required property int count

                readonly property bool applied: root.isApplied(tagRow.tagId)

                /// Нажатие кнопки удаления в этой строке.
                function deleteRequested() {
                    root.requestDelete(tagRow.tagId, tagRow.count);
                }

                // Отметка «тег уже применён к открытому файлу».
                Canvas {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
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
                    anchors.leftMargin: tagRow.applied ? 28 : 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 140
                    text: tagRow.name
                    elide: Text.ElideRight
                    color: "#d6e6f7"
                    font.pixelSize: Theme.fontSizeNormal
                }

                Text {
                    anchors.right: deleteButton.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: tagRow.count + " файлов"
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSizeSmall
                }

                // Кнопка удаления.
                Item {
                    id: deleteButton
                    objectName: "deleteTagButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24

                    Rectangle {
                        anchors.fill: parent
                        radius: 4
                        color: deleteArea.containsMouse ? "#3a2a2a" : "transparent"
                    }

                    DeleteGlyph {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        color: deleteArea.containsMouse ? Theme.danger : Theme.textDim
                    }

                    MouseArea {
                        id: deleteArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.requestDelete(tagRow.tagId, tagRow.count)
                    }

                    AppToolTip {
                        text: "Удалить тег"
                        visible: deleteArea.containsMouse
                    }
                }

                // Клик по строке добавляет тег в фильтр; в однофайловом режиме
                // дополнительно применяет его к открытому файлу.
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.selectedTagId = tagRow.tagId;
                        root.clickTag(tagRow.tagId);
                        if (!root.multiSelectMode && root.mediaId.length > 0
                                && !root.isApplied(tagRow.tagId)) {
                            root.toggleTag(tagRow.tagId);
                        }
                    }
                }

                AppToolTip {
                    text: "Добавить «" + tagRow.name + "» в фильтр"
                    visible: rowMouse.containsMouse && !deleteArea.containsMouse
                }
            }
        }

        Text {
            id: totalLabel
            width: parent.width
            text: "Всего тегов: " + App.tagModel.count
            color: Theme.textDim
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSizeSmall
        }

        Text {
            visible: App.tagModel.count === 0
            width: parent.width
            text: searchField.text.length > 0
                  ? "Ничего не найдено. Нажмите Enter, чтобы создать тег."
                  : "Тегов пока нет. Создайте первый тег."
            color: Theme.textDim
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeNormal
        }
    }

    // ==== Подтверждение удаления ====
    Dialog {
        id: confirmDelete
        anchors.centerIn: parent
        modal: true
        title: "Удалить тег?"
        // Стандартные кнопки диалога: раскладку берёт на себя Dialog.
        standardButtons: Dialog.Cancel | Dialog.Ok

        onAccepted: Library.deleteTag(root.pendingDeleteId)

        Text {
            width: 320
            text: "Тег «" + App.library.tagName(root.pendingDeleteId) + "» присвоен файлам: "
                  + root.pendingDeleteCount + ".\nСвязи с файлами будут удалены."
            color: Theme.text
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSizeNormal
        }
    }
}
