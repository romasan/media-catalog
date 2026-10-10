import QtQuick

// Треугольник воспроизведения — значок видео на карточке.
Canvas {
    id: root

    property color color: "#ffffff"

    implicitWidth: 24
    implicitHeight: 24
    antialiasing: true

    onPaint: draw()

    function draw() {
        const s = Math.min(width, height) / 24;
        const ctx = getContext("2d");
        ctx.reset();
        ctx.scale(s, s);
        ctx.fillStyle = color;

        ctx.beginPath();
        ctx.moveTo(8, 6);
        ctx.lineTo(18, 12);
        ctx.lineTo(8, 18);
        ctx.closePath();
        ctx.fill();
    }
}
