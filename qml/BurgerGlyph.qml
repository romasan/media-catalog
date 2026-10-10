import QtQuick

// Три полосы кнопки меню.
Canvas {
    id: root

    property color color: "#ffffff"
    property real lineWidth: 2

    implicitWidth: 24
    implicitHeight: 24
    antialiasing: true

    onPaint: draw()

    function draw() {
        const s = Math.min(width, height) / 24;
        const ctx = getContext("2d");
        ctx.reset();
        ctx.scale(s, s);
        ctx.lineWidth = lineWidth;
        ctx.strokeStyle = color;
        ctx.lineCap = "round";

        const lines = [7, 12, 17];
        for (let i = 0; i < lines.length; ++i) {
            ctx.beginPath();
            ctx.moveTo(5, lines[i]);
            ctx.lineTo(19, lines[i]);
            ctx.stroke();
        }
    }
}
