import QtQuick 2.0
import QtComponents 1.0

Item {
    id: scrollbar

    signal scrollStart
    signal scrollStop

    property alias value: model.value
    property alias inverted: model.inverted
    property alias minimum: model.minimumValue
    property alias maximum: model.maximumValue
    property alias singleStep: model.singleStep
    property alias pageStep: model.pageStep


    property Flickable flickableItem: null
    property bool vertical: true
    property int documentSize: 100
    property int viewSize: 100
    property real multiplier: 1
    property int bugWorkaround: 0
    visible: viewSize<documentSize

    height: 50
    width: 50

    onFlickableItemChanged: internal.initScollbar()
    Connections {
        target: flickableItem
        onHeightChanged: internal.initScollbar();
        onWidthChanged: internal.initScollbar();
       // onContentWidthChanged: internal.initScollbar();
        onContentHeightChanged: internal.initScollbar();
        onContentYChanged: internal.changeValue();
    }

    onScrollStart: internal.acceptChangeValue = false
    onScrollStop: internal.acceptChangeValue = true

    QtObject {
        id: internal
        property bool acceptChangeValue: true
        function initScollbar() {

            if(flickableItem.contentHeight>0)
            {
                scrollbar.viewSize = flickableItem.height;//flickableItem.visibleArea.heightRatio*flickableItem.contentHeight;
                scrollbar.documentSize = flickableItem.contentHeight;
                scrollbar.multiplier = scrollbar.documentSize/(scrollbar.documentSize-scrollbar.viewSize)+0.01;
                scrollbar.maximum = scrollbar.documentSize;
                scrollbar.value = flickableItem.contentY * scrollbar.multiplier;
                scrollbar.pageStep = scrollbar.viewSize;
                scrollbar.singleStep = scrollbar.viewSize/10;
            }

            //console.log("viewSize: " + scrollbar.viewSize +" docSize: "+ scrollbar.documentSize + " multplier: " + scrollbar.multiplier + " flickHeight: " + flickableItem.height);
        }
        function changeValue() {
            if(internal.acceptChangeValue)
            {
                if(flickableItem.contentY < 0) {
                    if(flickableItem.contentY < scrollbar.bugWorkaround) {
                        //console.log("sssmall contY: " + flickableItem.contentY);
                        scrollbar.bugWorkaround = flickableItem.contentY;
                    }
                }
                else if((flickableItem.contentY  + scrollbar.height) > flickableItem.contentHeight) {
                    var corrention = (flickableItem.contentY  + scrollbar.height) - flickableItem.contentHeight;
                    //console.log("correction: " + corrention + "  bugWorkaround: " + scrollbar.bugWorkaround)
                    if(corrention > scrollbar.bugWorkaround)
                        scrollbar.bugWorkaround = corrention;
                }

                scrollbar.value = (flickableItem.contentY - scrollbar.bugWorkaround)  * scrollbar.multiplier;
                //console.log("autoscrl: " + scrollbar.value + " " + flickableItem.contentY);
            }
        }
    }

    function addSingleStep() {
        // dont use the convenience API to trigger the
        // behavior on value specified on rangemodel
        if(model.value != model.maximumValue)
        {
            if (!inverted)
                model.value += model.singleStep;
            else
                model.value -= model.singleStep;
        }
    }

    function subSingleStep() {
        // dont use the convenience API to trigger the
        // behavior on value specified on rangemodel
        if(model.value != model.minimumValue)
        {
            if (!inverted)
                model.value -= model.singleStep;
            else
                model.value += model.singleStep;
        }
    }

    function addPageStep() {
        // dont use the convenience API to trigger the
        // behavior on value specified on rangemodel
        scrollbar.scrollStart();
        if (!inverted)
            model.value += model.pageStep;
        else
            model.value -= model.pageStep;
        scrollbar.scrollStop();
    }

    function subPageStep() {
        // dont use the convenience API to trigger the
        // behavior on value specified on rangemodel
        scrollbar.scrollStart();
        if (!inverted)
            model.value -= model.pageStep;
        else
            model.value += model.pageStep;        
        scrollbar.scrollStop();
    }

    Rectangle  {
        id: scrollbarPath

        color: '#dee2e5'
        property bool hold: false

        Timer {
            interval: 150
            repeat: true
            running: scrollbarPath.hold
            onTriggered: { scrollbarPathMouseRegion.handleRelease(); }
        }

        MouseArea {
            id: scrollbarPathMouseRegion
            anchors.fill: parent

            onPressed: {
                scrollbarPath.hold = true;
                handleRelease();
                scrollbar.scrollStart();
            }
            onReleased: {
                scrollbarPath.hold = false;
                scrollbar.scrollStop();
            }

            function handleRelease() {
                var pos;
                if (!scrollbar.vertical) {
                    if (mouseX > (handle.x + handle.width)) {
                        addPageStep();
                    } else {
                        subPageStep();
                    }
                } else {
                    if (mouseY > (handle.y + handle.height)) {
                        addPageStep();
                    } else {
                        subPageStep();
                    }
                }
            }
        }

        BorderImage {
            id: handle

            MouseArea {
                id: handleMouseRegion
                anchors.fill: parent
                hoverEnabled: true
                drag.target: handle

                onPressed: { scrollbar.scrollStart(); }
                onReleased: { scrollbar.scrollStop(); }
            }

            function handleSize() {
                var size1;
                if (!scrollbar.vertical)
                    size1 = scrollbarPath.width;
                else
                    size1 = scrollbarPath.height;

                var size = (size1 * viewSize) / documentSize;
                if (size < 0)
                    return 0;
                if (size > size1)
                    return size1;

                return size;
            }
        }

        state: "horizontal";
        states: [
            State {
                name: "horizontal"
                AnchorChanges {
                    target: scrollbarPath
                    anchors.left: button1.right
                    anchors.right: button2.left
                }
                AnchorChanges {
                    target: button1
                    anchors.left: scrollbar.left
                }
                AnchorChanges {
                    target: button2
                    anchors.right: scrollbar.right
                }
                PropertyChanges {
                    target: scrollbar;
                    height: button1.height;
                }
                PropertyChanges {
                    target: scrollbarPath
                    height: button1.height
                }
                PropertyChanges {
                    target: handleMouseRegion
                    drag.axis: "XAxis"
                    drag.minimumX: 0
                    drag.maximumX: scrollbarPath.width - handle.width
                }
                PropertyChanges {
                    target: model
                    position: handle.x
                    positionAtMaximum: scrollbarPath.width - handle.width
                }
                PropertyChanges {
                    target: handle
                    x: model.position
                    width: handleSize()
                    border.left: 5
                    border.right: 5
                    source: Qt.resolvedUrl("qrc:/style/scroll-hhandle"
                                           + (handleMouseRegion.containsMouse ? "-hover" : "") + ".png")
                }
                PropertyChanges {
                    target: button1;
                    source: Qt.resolvedUrl("qrc:/style/scroll-button-left"
                                           + (button1MouseRegion.containsMouse ? "-hover" : "") + ".png")
                }
                PropertyChanges {
                    target: button2;
                    source: Qt.resolvedUrl("qrc:/style/scroll-button-right"
                                           + (button2MouseRegion.containsMouse ? "-hover" : "") + ".png")
                }
            },
            State {
                name: "vertical"
                when: scrollbar.vertical
                AnchorChanges {
                    target: scrollbarPath
                    anchors.top: button1.bottom
                    anchors.bottom: button2.top
                }
                AnchorChanges {
                    target: button1
                    anchors.top: scrollbar.top
                }
                AnchorChanges {
                    target: button2
                    anchors.bottom: scrollbar.bottom
                }
                PropertyChanges {
                    target: scrollbar;
                    width: button1.width;
                }
                PropertyChanges {
                    target: scrollbarPath
                    width: button1.width
                }
                PropertyChanges {
                    target: handleMouseRegion
                    drag.axis: "YAxis"
                    drag.minimumY: 0
                    drag.maximumY: scrollbarPath.height - handle.height
                }
                PropertyChanges {
                    target: model
                    position: handle.y
                    positionAtMaximum: scrollbarPath.height - handle.height
                }
                PropertyChanges {
                    target: handle
                    y: model.position
                    height: handleSize()
                    border.top: 5
                    border.bottom: 5
                    source: Qt.resolvedUrl("qrc:/style/scroll-vhandle"
                                           + (handleMouseRegion.containsMouse ? "-hover" : "") + ".png")
                }
                PropertyChanges {
                    target: button1;
                    source: Qt.resolvedUrl("qrc:/style/scroll-button-up"
                                           + (button1MouseRegion.containsMouse ? "-hover" : "") + ".png")
                }
                PropertyChanges {
                    target: button2;
                    source: Qt.resolvedUrl("qrc:/style/scroll-button-down"
                                           + (button2MouseRegion.containsMouse ? "-hover" : "") + ".png")
                }
            }
        ]
    }

    /*
     * ### Yes, button1 and button2 have too much code repeated...
     */
    Image {
        id: button1
        property bool hold: false

        Timer {
            interval: 150
            repeat: true;
            running: button1.hold
            onTriggered: { scrollbar.scrollStart(); subSingleStep(); scrollbar.scrollStop(); }
        }

        MouseArea {
            id: button1MouseRegion
            hoverEnabled: true
            anchors.fill: parent
            onPressed: {
                button1.hold = true;
                scrollbar.scrollStart();
                subSingleStep();
                scrollbar.scrollStop();
            }

            onReleased: {
                button1.hold = false;
            }
        }
    }

    Image {
        id: button2
        property bool hold: false;

        Timer {
            interval: 150
            repeat: true
            running: button2.hold
            onTriggered: { scrollbar.scrollStart(); addSingleStep();scrollbar.scrollStop(); }
        }

        MouseArea {
            id: button2MouseRegion
            hoverEnabled: true
            anchors.fill: parent
            onPressed: {
                button2.hold = true;
                scrollbar.scrollStart();
                addSingleStep();
                scrollbar.scrollStop();
            }

            onReleased: {
                button2.hold = false;
            }
        }
    }

    RangeModel {
        id: model

//        Behavior on value {
//            PropertyAnimation {
//                id: prop1
//                easing.type: "OutCubic";
//                duration: 250;
//            }
//        }
        onValueChanged: {
            if( !internal.acceptChangeValue )
            {
                //console.log("1)" + scrollbar.value/scrollbar.multiplier);
                flickableItem.contentY = scrollbar.value/scrollbar.multiplier + scrollbar.bugWorkaround ; //- flickableItem.visibleArea.heightRatio * scrollbar.documentSize;
            }
        }
        singleStep: 1
        pageStep: 10
        minimumValue: 0
        maximumValue: 100
        positionAtMinimum: 0
    }
}
