import QtQuick 2.0

//TopLevelItem {
Item {
    id: tooltip;
    property string text;
    property bool shown: false;
    property int maxWidth: 200;

    width: displayText.width + 22;
    height: displayText.height + 16;

    transformOrigin: Item.Center;
    scale: 0;
    visible: false;

    // Visible items bellow should anchor / be sized in relation to
    // 'parent'. They'll be reparented to a proper positioned and
    // resized toplevel item.
    //keepInside: true;

    BorderImage {
        id: background;
        anchors.fill: parent;
        source: "qrc:/tooltip-background.png";
        border.top: 4;
        border.left: 11;
        border.bottom: 12;
        border.right: 11;

        states: State {
            name: "shown";
            when: tooltip.shown && (tooltip.text !== "");
            PropertyChanges { target: tooltip; scale: 1; visible: true }
        }

        transitions: [
            Transition {
                from: "";
                to: "shown";
                SequentialAnimation {
                    PropertyAction {
                        target: tooltip;
                        property: "visible";
                    }
                    NumberAnimation {
                        duration: 500;
                        target: tooltip;
                        easing.type: Easing.OutQuart
                        //easing.period: 0.75;
                        property: "scale";
                    }
                }
            },
            Transition {
                from: "shown";
                to: "";
                SequentialAnimation {
                    NumberAnimation {
                        duration: 150;
                        target: tooltip;
                        easing.type: "InSine";
                        property: "scale";
                    }
                    PropertyAction {
                        target: tooltip;
                        property: "visible";
                    }
                }
            }
        ]
    }

    // ### This Text is used to get the "preferred size" information, that
    // will be considered when calculating the toplevel item geometry. This
    // could be replaced by having this information available in a regular Text
    // item. Similar issue of trying to know the "real image size" inside an
    // Image item.
    Text {
        id: model;
        text: tooltip.text;
        visible: false;
    }

    TextEdit {
        id: displayText
        readOnly: true
        anchors.centerIn: parent;
        anchors.verticalCenterOffset: -4;
        horizontalAlignment: Text.AlignHCenter;
        wrapMode: TextEdit.WrapAnywhere
        width: (model.width > maxWidth) ? maxWidth : model.width;
        text: tooltip.text;

        color: "#ffffff";
    }
}
