import QtQuick
import QtQuick.Controls
import MediaCatalog

// Панель массового тегирования: появляется при выделении файлов.
Item {
    id: root

    implicitHeight: App.hasSelection ? 46 : 0
    visible: implicitHeight > 0

    /// Запрос на открытие менеджера тегов: попап живёт в главном окне, иначе
    /// он обрезается границами этой панели.
    signal tagManagerRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spacing * 1.5
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacing

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "Выбрано: " + App.selectedCount
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeNormal
        }

        Button {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: "Снять выделение"
            flat: true
            onClicked: App.mediaModel.clearSelection()
        }

        Button {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: "Выделить все"
            flat: true
            onClicked: App.mediaModel.selectAll()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spacing * 1.5
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacing

        // Поле нового тега с автодополнением по существующим тегам.
        ComboBox {
            id: tagInput
            anchors.verticalCenter: parent.verticalCenter
            width: 220
            height: 28
            editable: true
            model: App.tagModel
            textRole: "name"
            displayText: editText

            ToolTip.text: "Введите имя тега и нажмите «Применить»"
            ToolTip.visible: hovered

            onAccepted: applyButton.apply()
        }

        Button {
            id: applyButton
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: "Применить"
            enabled: tagInput.editText.trim().length > 0
            onClicked: apply()
        }

        Button {
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            text: "Менеджер тегов…"
            flat: true
            onClicked: root.tagManagerRequested()
        }
    }

    function apply() {
        const name = tagInput.editText.trim();
        if (name.length === 0)
            return;
        App.createTagAndApplyToSelection(name);
        tagInput.editText = "";
    }

}
