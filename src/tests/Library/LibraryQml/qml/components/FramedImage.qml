import QtQuick 1.1

Item {
    id: photoWrapper
    anchors.fill: parent

    property alias imageSource: originalImage.source
    property alias cache: originalImage.cache
    property alias smooth: originalImage.smooth
    property alias asynchronous: originalImage.asynchronous
    property alias isImageHovered: imageMouseArea.containsMouse

    signal imageClicked
    signal imageEntered
    signal imageExited

    rotation: delegate.randomAngle
    Behavior on rotation { PropertyAnimation {easing.type: Easing.InOutBack; easing.amplitude: 5.0; easing.period: 5.5; easing.overshoot: 5.0} }

    BorderImage {
        visible: originalImage.status == Image.Ready
        anchors {
            fill: border
            leftMargin: -6; topMargin: -6; rightMargin: -8; bottomMargin: -8
        }

        source: 'qrc:/box-shadow'; smooth: true
        border.left: 10; border.top: 10; border.right: 10; border.bottom: 10
    }

    Rectangle {
        id: border; color: 'white'; anchors.centerIn: parent; smooth: true
        width: originalImage.paintedWidth + 6; height: originalImage.paintedHeight + 6
        visible: originalImage.status == Image.Ready
    }

    Image {
        id: originalImage

        anchors.fill: parent
        fillMode: Image.PreserveAspectFit;

        MouseArea {
            id: imageMouseArea
            anchors.centerIn:  originalImage;
            width: originalImage.paintedWidth
            height: originalImage.paintedHeight
            hoverEnabled: true
            onClicked: {
                photoWrapper.imageClicked();
            }
            onEntered:  {
                photoWrapper.imageEntered();
                photoWrapper.rotation = 0
            }
            onExited:  {
                photoWrapper.imageExited();
                photoWrapper.rotation = delegate.randomAngle
            }
        }
    }
    BusyIndicator { anchors.centerIn: parent; on: originalImage.status != Image.Ready }

} // photoWrapper
