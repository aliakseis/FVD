import QtQuick 1.1

Loader {
    id: tooltipLoader;

    property string text: "";
    property bool shown: false;
    //anchors.horizontalCenter: parent.horizontalCenter
    //            anchors.top: parent.top
    property Component realComponent: Tooltip {
        text: tooltip.text;
        shown: tooltip.shown;
    }

    sourceComponent: text == "" ? null : realComponent
}
