import QtQuick
import QtQuick.Controls
import MediaCatalog

// Каркас попапа: затемнённый фон на всё окно и перемещаемое окно внутри.
//
// Отвечает за закрытие тремя способами, как в версии 1.x:
//   - крестик в заголовке;
//   - клавиша Esc;
//   - клик по затемнённому фону.
//
// Подложка перехватывает клики, поэтому они не проваливаются в сетку под
// попапом. Esc обрабатывается на уровне окна (`Window.onEscapePressed` у
// родителя), потому что фокус к этому моменту может быть у любого элемента.
Item {
    id: root

    /// Заголовок окна попапа.
    property string title: ""
    /// Содержимое кладётся внутрь окна.
    default property alias content: window.content
    /// Начальный размер окна.
    property int frameWidth: 420
    property int frameHeight: 460

    signal closed()

    function open() {
        baseOpen();
    }

    /// Открытие без переопределения: наследники вызывают его из своих
    /// вариантов open(), иначе получается рекурсия.
    function baseOpen() {
        visible = true;
        window.centerInParent();
    }

    function close() {
        visible = false;
        closed();
    }

    visible: false
    // Esc закрывает попап независимо от того, где сейчас фокус.
    focus: true
    Keys.onEscapePressed: root.close()

    // Подложка: закрывает попап по клику и не пропускает клики в сетку.
    Rectangle {
        anchors.fill: parent
        color: "#66000000"

        MouseArea {
            anchors.fill: parent
            // Наведение тоже перехватываем: иначе над открытым попапом
            // показывались подсказки карточек из сетки под ним.
            hoverEnabled: true
            onClicked: root.close()
        }
    }

    DraggableResizable {
        id: window
        width: root.frameWidth
        height: root.frameHeight
        title: root.title
        onClosed: root.close()
    }
}
