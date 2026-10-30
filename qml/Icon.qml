import ZaloNative 1.0
import QtQuick

// Small, theme-aware line icons drawn with Canvas so the shell does not depend
// on an icon font or branded image assets.
Item {
    id: root

    property string name: "search"
    property color color: Theme.textMuted
    property int size: 20

    implicitWidth: size
    implicitHeight: size

    onNameChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d");
            if (!ctx)
                return;
            ctx.clearRect(0, 0, width, height);
            const scale = width / 24;
            ctx.save();
            ctx.scale(scale, scale);
            ctx.strokeStyle = root.color;
            ctx.fillStyle = root.color;
            ctx.lineWidth = 1.8;
            ctx.lineCap = "round";
            ctx.lineJoin = "round";

            switch (root.name) {
            case "search":
                ctx.beginPath();
                ctx.arc(10.5, 10.5, 6, 0, Math.PI * 2);
                ctx.moveTo(15, 15);
                ctx.lineTo(20, 20);
                ctx.stroke();
                break;
            case "send":
                ctx.beginPath();
                ctx.moveTo(4, 11.5);
                ctx.lineTo(20.5, 4);
                ctx.lineTo(14.5, 20.5);
                ctx.lineTo(11.5, 13.5);
                ctx.closePath();
                ctx.moveTo(11.5, 13.5);
                ctx.lineTo(20.5, 4);
                ctx.stroke();
                break;
            case "attach":
                ctx.beginPath();
                ctx.moveTo(8, 12.5);
                ctx.lineTo(14.5, 6);
                ctx.bezierCurveTo(17, 3.5, 21, 7.5, 18.5, 10);
                ctx.lineTo(11, 17.5);
                ctx.bezierCurveTo(9, 19.5, 6, 16.5, 8, 14.5);
                ctx.lineTo(15, 7.5);
                ctx.stroke();
                break;
            case "close":
                ctx.beginPath();
                ctx.moveTo(6, 6);
                ctx.lineTo(18, 18);
                ctx.moveTo(18, 6);
                ctx.lineTo(6, 18);
                ctx.stroke();
                break;
            case "more":
                for (let i = 0; i < 3; ++i) {
                    ctx.beginPath();
                    ctx.arc(6 + i * 6, 12, 1.6, 0, Math.PI * 2);
                    ctx.fill();
                }
                break;
            case "phone":
                ctx.beginPath();
                ctx.moveTo(6, 3.5);
                ctx.lineTo(9, 3.5);
                ctx.lineTo(10.6, 9);
                ctx.lineTo(8.4, 11.2);
                ctx.bezierCurveTo(10, 14.5, 12.5, 17, 15.8, 18.6);
                ctx.lineTo(18, 16.4);
                ctx.lineTo(20.5, 18);
                ctx.lineTo(20.5, 21);
                ctx.bezierCurveTo(20.5, 21.6, 20, 22, 19.4, 22);
                ctx.bezierCurveTo(10.4, 22, 3, 14.6, 3, 5.6);
                ctx.bezierCurveTo(3, 5, 3.4, 4.6, 4, 4.6);
                ctx.closePath();
                ctx.stroke();
                break;
            case "check":
                ctx.beginPath();
                ctx.moveTo(5, 12.5);
                ctx.lineTo(10, 17.5);
                ctx.lineTo(19, 7);
                ctx.stroke();
                break;
            case "chevron-down":
                ctx.beginPath();
                ctx.moveTo(6, 9);
                ctx.lineTo(12, 15);
                ctx.lineTo(18, 9);
                ctx.stroke();
                break;
            case "info":
                ctx.beginPath();
                ctx.arc(12, 12, 8.5, 0, Math.PI * 2);
                ctx.moveTo(12, 11);
                ctx.lineTo(12, 17);
                ctx.moveTo(12, 7.4);
                ctx.lineTo(12, 7.6);
                ctx.stroke();
                break;
            default:
                break;
            }

            ctx.restore();
        }
    }
}
