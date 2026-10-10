import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import MediaCatalog

// Круглая кнопка меню в правом нижнем углу — как в версии 1.x.
Item {
    id: root

    implicitWidth: 56
    implicitHeight: 56

    signal openCatalogManager()
    signal openTagManager()

    // Затемнение фона под открытым меню: клик по нему закрывает меню.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -root.parent.width
        visible: menu.opened
        color: "#4d000000"
        z: -1

        MouseArea {
            anchors.fill: parent
            onClicked: menu.close()
        }
    }

    Rectangle {
        id: buttonCircle
        anchors.fill: parent
        radius: width / 2
        color: hoverArea.containsMouse ? "#4682b4" : Theme.accent
        border.color: "#00000033"

        BurgerGlyph {
            anchors.centerIn: parent
            width: 24
            height: 24
            color: "#ffffff"
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: menu.opened ? menu.close() : menu.open()
        }

        AppToolTip {
            target: hoverArea
            text: "Меню"
            delay: 600
        }
    }

    Menu {
        id: menu
        // Раскрываемся вверх и влево: кнопка стоит в правом нижнем углу.
        y: -height - 12
        x: buttonCircle.width - menu.width

        MenuItem {
            text: "Управление каталогами"
            onTriggered: root.openCatalogManager()
        }
        MenuItem {
            text: "Управление тегами"
            onTriggered: root.openTagManager()
        }
        MenuSeparator {}
        MenuItem {
            text: "Сканировать заново"
            onTriggered: Library.requestInitialScan()
        }
        MenuSeparator {}
        MenuItem {
            text: "Экспорт"
            onTriggered: exportDialog.open()
        }
        MenuItem {
            text: "Импорт"
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
            // selectedFile — URL, а не путь.
            const path = urlToLocalFile(selectedFile);
            if (App.transferData(true, path))
                App.notifyExportDirectory(path);
        }
    }

    FileDialog {
        id: importDialog
        title: "Импорт данных"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: App.transferData(false, urlToLocalFile(selectedFile))
    }
}
