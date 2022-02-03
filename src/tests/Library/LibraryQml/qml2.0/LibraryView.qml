import QtQuick 2.0
import "components"
//import QtDesktop 1.0

Rectangle {
    width: 100
    height: 100

    color: "#f5f5f5"; smooth: true
    property bool blockUpdates: false

    Component {
        id: appHighlight
        Item {
            Rectangle {
                anchors.centerIn: parent;
                height: parent.height - 18;
                width:parent.width - 18;
                color: "lightsteelblue"
                radius: 5
            }
        }
    }

    GridView {
        id: albumView
        property int hoveredIndex: -1

        anchors.fill: parent
        width: parent.width; height: parent.height; cellWidth: 230; cellHeight: 200
        model: libraryModel
        boundsBehavior: Flickable.StopAtBounds
        delegate: LibraryItemDelegate{ }
        focus: true
        highlight: appHighlight
        highlightMoveDuration : 1
    }

//    ScrollBar {
//        id: vertical
//        /* QtDesktop 1.0 */
//        orientation: Qt.Vertical
//        maximumValue: albumView.contentHeight > albumView.height ? albumView.contentHeight - albumView.height : 0 // contentWidth > availableWidth ? root.contentWidth - availableWidth : 0
//        minimumValue: 0
//        visible: maximumValue > 0
//        onValueChanged: {
//            if (!blockUpdates) {
//                albumView.contentY = value
//               // verticalValue = value
//            }
//        }
//        /********************/
//        //flickableItem: albumView
//        height: parent.height
//        anchors { right: albumView.right; top: albumView.top;  bottom: albumView.Bottom }
//    }
}
