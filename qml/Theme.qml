pragma Singleton
import QtQuick

// Единая палитра и метрики интерфейса.
QtObject {
    // Фон и поверхности
    readonly property color background: "#14161a"
    readonly property color surface: "#1d2026"
    readonly property color surfaceAlt: "#262a31"
    readonly property color overlay: "#e6000000"

    // Текст
    readonly property color text: "#e8eaed"
    readonly property color textMuted: "#9aa0a6"
    readonly property color textDim: "#6b7075"

    // Акценты
    readonly property color accent: "#4c8dff"
    readonly property color accentSoft: "#334c8dff"
    readonly property color success: "#3fbf7f"
    readonly property color warning: "#e0a33e"
    readonly property color danger: "#e05c5c"

    // Границы и выделение
    readonly property color border: "#33383f"
    readonly property color selectedBorder: "#4c8dff"
    readonly property color hover: "#2b3037"

    // Метрики
    readonly property int spacing: 8
    readonly property int radius: 6
    readonly property int minCardSize: 100
    readonly property int maxCardSize: 200
    readonly property int fontSizeSmall: 11
    readonly property int fontSizeNormal: 13
    readonly property int fontSizeLarge: 16

    // Пустая строка означает системный шрифт по умолчанию: конкретные
// семейства вроде "SF Pro Text" могут отсутствовать и вызывать подстановку.
    readonly property string fontFamily: ""
}
