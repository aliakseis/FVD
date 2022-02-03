import QtQuick 2.0

Rectangle {
    id:main
    property string text: textRectangle.text
    property alias fontPixelSize: helpText.font.pixelSize
    property alias textColor: helpText.color
    property alias fontBold: helpText.font.bold

    property bool enableTooltip: false
    property bool hovered: false
    property bool showTooltip: false
    color: "transparent"

    Flickable {
        id: flickArea
        anchors.fill: parent
        contentWidth: helpText.width; contentHeight: helpText.height
        flickableDirection: Flickable.VerticalFlick
        clip: true

        TextEdit {
            id: helpText
            smooth: true
            wrapMode: TextEdit.WrapAnywhere
            width: main.width;
            readOnly: true

            text: main.text
            color:'black'
        }
    }

    Timer {
        id: timer
        interval: 500; running: false; repeat: false
        onTriggered: {
             if(!tooltip.pressDismiss)
                main.showTooltip = true;
            timer.running = false;
        }
    }

    TooltipLoader {
        id: tooltipLoader;
        anchors.fill: parent;
        Tooltip {
            id: tooltip;
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: -5
            maxWidth: main.width
            property bool pressDismiss: false;
            shown: main.showTooltip && !tooltip.pressDismiss;
            text: main.text;
        }
    }

    MouseArea {
        id: ma
        anchors { fill: parent; }
        hoverEnabled: true
        onClicked: {
            console.log("X:"+mouseX+ " Y:" +mouseY);
            tooltip.pressDismiss = true;
        }
        onPressed: tooltip.pressDismiss = true;
        onEntered: {
            if(main.enableTooltip)
            {
                tooltip.pressDismiss = false;
                timer.running = true;
                main.hovered = true;
            }
        }
        onExited: {
            timer.stop();
            main.hovered = false;
            main.showTooltip = false;
        }
    }
}
