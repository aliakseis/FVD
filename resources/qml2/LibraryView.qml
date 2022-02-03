import QtQuick 2.12
import QtQuick.Controls 2.12
import "components"

Rectangle {
    width: 100
    height: 100

    color: "#f5f5f5"; smooth: true

    //anchors.centerIn: parent

	function selectItem(index) {
		if (index && typeof index == "number") {
			albumView.currentIndex = index;
            albumView.positionViewAtIndex(index, GridView.Visible);
		}
    }

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

    ScrollView {
        anchors.fill: parent
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
    }

   /* ScrollBar {
        id: vertical
        flickableItem: albumView
        height: parent.height
        anchors { right: albumView.right; top: albumView.top;  bottom: albumView.Bottom }
    }
	*/
}
