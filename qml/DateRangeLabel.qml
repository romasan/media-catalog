import QtQuick
import QtQuick.Controls
import MediaCatalog

// Подпись с диапазоном дат видимых карточек.
// Детализация меняется по мере сужения диапазона: год → месяц → день.
Rectangle {
    id: root

    property double minTimestamp: 0
    property double maxTimestamp: 0

    readonly property bool valid: minTimestamp > 0 && maxTimestamp >= minTimestamp

    /// Подбирает самый компактный формат, который однозначно описывает диапазон.
    readonly property string label: {
        if (!valid)
            return "";

        const minDate = new Date(minTimestamp);
        const maxDate = new Date(maxTimestamp);

        const sameYear = minDate.getFullYear() === maxDate.getFullYear();
        const sameMonth = sameYear && minDate.getMonth() === maxDate.getMonth();
        const sameDay = sameMonth && minDate.getDate() === maxDate.getDate();

        const months = ["янв", "фев", "мар", "апр", "мая", "июн",
                        "июл", "авг", "сен", "окт", "ноя", "дек"];

        if (sameDay) {
            // Один день — показываем число, месяц и год.
            return minDate.getDate() + " " + months[minDate.getMonth()] + " " + minDate.getFullYear();
        }
        if (sameMonth) {
            // Один месяц — диапазон чисел внутри месяца.
            return minDate.getDate() + "–" + maxDate.getDate() + " "
                 + months[minDate.getMonth()] + " " + minDate.getFullYear();
        }
        if (sameYear) {
            // Один год — диапазон по месяцам.
            return months[minDate.getMonth()] + " – " + months[maxDate.getMonth()]
                 + " " + minDate.getFullYear();
        }
        // Разные годы — показываем только годы.
        return minDate.getFullYear() + " – " + maxDate.getFullYear();
    }

    visible: valid
    color: Theme.surface
    radius: Theme.radius
    implicitWidth: text.implicitWidth + 2 * Theme.spacing
    implicitHeight: 26
    opacity: 0.92

    Text {
        id: text
        anchors.centerIn: parent
        text: root.label
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSmall
    }
}
