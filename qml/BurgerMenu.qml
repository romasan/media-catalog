import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import MediaCatalog

// Круглая кнопка-меню: каталоги, теги, экспорт/импорт, повторное сканирование.
Item {
    id: root
    width: 40
    height: 40

    signal openCatalogManager()
    signal openTagManager()

    Rectangle {
        id: button
        anchors.fill: parent
        radius: width / 2
        color: hoverArea.containsMouse ? Theme.hover : Theme.surface
        border.color: Theme.border

        Text {
            anchors.centerIn: parent
            text: "☰"
            color: Theme.text
            font.pixelSize: 18
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: menu.popup()
        }
    }

    Menu {
        id: menu

        MenuItem {
            text: "Каталоги…"
            onTriggered: root.openCatalogManager()
        }
        MenuItem {
            text: "Теги…"
            onTriggered: root.openTagManager()
        }
        MenuSeparator {}
        MenuItem {
            text: "Сканировать заново"
            onTriggered: Library.requestInitialScan()
        }
        MenuSeparator {}
        MenuItem {
            text: "Экспорт данных…"
            onTriggered: exportDialog.open()
        }
        MenuItem {
            text: "Импорт данных…"
            onTriggered: importDialog.open()
        }
    }

    FileDialog {
        id: exportDialog
        title: "Экспорт данных"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON (*.json)"]
        onAccepted: {
            if (App.transferData(true, selectedFile))
                App.notifyExportDirectory(selectedFile);
        }
    }

    FileDialog {
        id: importDialog
        title: "Импорт данных"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: App.transferData(false, selectedFile)
    }
}
