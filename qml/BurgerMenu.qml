import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import MediaCatalog

// Круглая кнопка меню в правом нижнем углу — как в версии 1.x.
Item {
    id: root
    objectName: "burgerMenu"

    implicitWidth: 56
    implicitHeight: 56

    signal openCatalogManager()
    signal openTagManager()

    /// Открыть меню кнопки (используется кнопкой и проверками).
    function openMenu() {
        menu.open();
    }

    function closeMenu() {
        menu.close();
    }

    function isMenuOpen() {
        return menu.opened;
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
            objectName: "burgerButtonArea"
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
            onTriggered: {
                // Закрываем меню явно: иначе оно остаётся модальным и
                // перехватывает клики до перезапуска приложения.
                menu.close();
                root.openCatalogManager()
            }
        }
        MenuItem {
            text: "Управление тегами"
            onTriggered: {
                // Закрываем меню явно: иначе оно остаётся модальным и
                // перехватывает клики до перезапуска приложения.
                menu.close();
                root.openTagManager()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Сканировать заново"
            onTriggered: {
                // Закрываем меню явно: иначе оно остаётся модальным и
                // перехватывает клики до перезапуска приложения.
                menu.close();
                Library.requestInitialScan()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: "Экспорт"
            onTriggered: {
                // Закрываем меню явно: иначе оно остаётся модальным и
                // перехватывает клики до перезапуска приложения.
                menu.close();
                exportDialog.open()
            }
        }
        MenuItem {
            text: "Импорт"
            onTriggered: {
                // Закрываем меню явно: иначе оно остаётся модальным и
                // перехватывает клики до перезапуска приложения.
                menu.close();
                importDialog.open()
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: "Экспорт данных"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON (*.json)"]
        onAccepted: {
            // selectedFile — URL, а не путь: преобразуем через ядро.
            const path = App.toLocalPath(selectedFile);
            if (App.transferData(true, path))
                App.notifyExportDirectory(path);
        }
    }

    FileDialog {
        id: importDialog
        title: "Импорт данных"
        fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: App.transferData(false, App.toLocalPath(selectedFile))
    }
}
