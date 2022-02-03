import QtQuick 1.1

Item {
    id: button
    property bool hovered: false
    property bool showTooltip: false
    property alias imageSource: icon.source
    property alias icoLeftOffset: icon.x
    property color tint: "transparent"
    property int brdr: 0
    property string tooltipText: ""

    signal clicked(variant mouse)

    Image {
        id: shadow
        source: 'qrc:/circle-shadow'; smooth: true
        property int negMarging: (parent.width/Math.log(parent.width/3))
        anchors {
            fill: parent
            leftMargin: -negMarging; topMargin: -negMarging; rightMargin: -negMarging; bottomMargin: -negMarging
        }        
    }


    Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "white" }
            GradientStop { position: 1.0; color: "#CCC" }
        }
        border.width: 0; radius: width*0.5
        anchors.fill: parent;
        color: button.tint; visible: button.tint != ""
        //opacity: 0.75;
        smooth: true
    }

    Image {
        id: icon;
        smooth: true
        anchors.centerIn: parent
    }

    Timer {
        id: timer
        interval: 500; running: false; repeat: false
        onTriggered: {
             if(!tooltip.pressDismiss)
                button.showTooltip = true;
            timer.running = false;
            tooltip.x = ma.mouseX+4;
            tooltip.y = ma.mouseY;
        }
    }

    TooltipLoader {
        id: tooltipLoader;
        anchors.fill: parent;
        Tooltip {
            id: tooltip;
            //anchors.horizontalCenter: parent.horizontalCenter
            //anchors.top: parent.top
            //anchors.topMargin: -5
            property bool pressDismiss: false;
            shown: button.showTooltip && !tooltip.pressDismiss;
            text: button.tooltipText;
        }
    }
    MouseArea {
        id: ma
        anchors { fill: parent; leftMargin: -brdr; topMargin: -brdr; rightMargin: -brdr; bottomMargin: -brdr }
        hoverEnabled: true
        onClicked: {
            console.log("X:"+mouseX+ " Y:" +mouseY);
            tooltip.pressDismiss = true;
            button.clicked(mouse); }
        onPressed: tooltip.pressDismiss = true;
        onEntered: {
            tooltip.pressDismiss = false;
            timer.running = true;
            button.hovered = true;
        }
        onExited: {
            timer.stop();
            button.hovered = false;
            button.showTooltip = false;
        }
    }
}
