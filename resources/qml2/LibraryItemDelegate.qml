import QtQuick 2.9
import QtQuick.Controls 2.2
import "components"

Item {
    id: delegate

    width: albumView.cellWidth
    height: albumView.cellHeight

    property double randomAngle: (Math.random() > 0.5 ? -1 : 1)
                                 * (Math.random() * 4 + 2)

    property bool isListenerWorking: false
    property int textInfoHeight: 32
    property bool contextMenuHovered: false

    /*
     * Context menu for a library item.
     *
     * The menu belongs to the delegate, so it automatically operates
     * on the correct model index.
     */
    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Play")

            onHoveredChanged: {
                delegate.contextMenuHovered = hovered
                if (!hovered)
                    Qt.callLater(delegate.updateContextMenuHover)
            }

            onTriggered: {
                delegate.selectThisItem()
                qmllistener.onPlayClicked(index)
            }
        }

        MenuItem {
            text: qsTr("Play internally")

            onHoveredChanged: {
                delegate.contextMenuHovered = hovered
                if (!hovered)
                    Qt.callLater(delegate.updateContextMenuHover)
            }

            onTriggered: {
                delegate.selectThisItem()
                qmllistener.onPlayInternal(index)
            }
        }

        MenuSeparator {
        }

        MenuItem {
            text: qsTr("Delete")

            onHoveredChanged: {
                delegate.contextMenuHovered = hovered
                if (!hovered)
                    Qt.callLater(delegate.updateContextMenuHover)
            }

            onTriggered: {
                delegate.selectThisItem()
                delegate.deleteItemByIndex(index)
            }
        }

        onOpened: {
            delegate.contextMenuHovered = false
        }

        onClosed: {
            delegate.contextMenuHovered = false
        }
    }

    GridView.onRemove: SequentialAnimation {
        PropertyAction {
            target: delegate
            property: "GridView.delayRemove"
            value: true
        }

        NumberAnimation {
            target: delegate
            property: "scale"
            to: 0
            duration: 300
            easing.type: Easing.InOutQuad
        }

        PropertyAction {
            target: delegate
            property: "GridView.delayRemove"
            value: false
        }
    }

    function updateContextMenuHover() {
        if (!contextMenu.visible)
            return

        if (!frameMouseArea.containsMouse && !contextMenuHovered)
            contextMenu.close()
    }

    function selectThisItem() {
        /*
         * Make sure the GridView owns keyboard/mouse focus.
         * Otherwise currentIndex can change without the highlight
         * becoming visible.
         */
        albumView.forceActiveFocus()
        albumView.currentIndex = index
        albumView.positionViewAtIndex(index, GridView.Contain)
    }

    function deleteItemByIndex(idx) {
        albumView.positionViewAtIndex(idx, GridView.Contain)
        qmllistener.onDeleteClicked(idx)
    }

    function itemHoveredHandler() {
        albumView.hoveredIndex = index

        frameHoverTimer.stop()

        if (playLoader.status === Loader.Loading
                && deleteLoader.status === Loader.Loading)
            return

        if (playLoader.status === Loader.Ready)
            playLoader.item.visible = true
        else
            playLoader.sourceComponent = playButton

        if (deleteLoader.status === Loader.Ready)
            deleteLoader.item.visible = true
        else
            deleteLoader.sourceComponent = deleteButton
    }

    function looseHoverHandler() {
        frameHoverTimer.stop()

        if (playLoader.status === Loader.Ready)
            playLoader.item.visible = false
        else
            playLoader.sourceComponent = null

        if (deleteLoader.status === Loader.Ready)
            deleteLoader.item.visible = false
        else
            deleteLoader.sourceComponent = null
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Delete) {
            delegate.deleteItemByIndex(index)
        }
        else if (event.key === Qt.Key_Space
                 || event.key === Qt.Key_Enter
                 || (event.key === Qt.Key_Return
                     && event.modifiers === Qt.NoModifier)) {
            qmllistener.onPlayClicked(index)
        }
        else if (event.key === Qt.Key_PageDown
                 && event.modifiers === Qt.NoModifier) {
            vertical.addPageStep()
        }
        else if (event.key === Qt.Key_PageUp
                 && event.modifiers === Qt.NoModifier) {
            vertical.subPageStep()
        }
    }

    Connections {
        target: albumView

        function onHoveredIndexChanged(index) {
            if (albumView.hoveredIndex !== index)
                looseHoverHandler()
        }
    }

    // ---------------------------------------------------------------------
    // Background shadow
    // ---------------------------------------------------------------------

    BorderImage {
        id: frameShadow

        anchors {
            fill: frame
            leftMargin: -6
            topMargin: -6
            rightMargin: -8
            bottomMargin: -8
        }

        rotation: frame.rotation

        source: "qrc:/box-shadow"
        smooth: true

        border.left: 10
        border.top: 10
        border.right: 10
        border.bottom: 10
    }

    // ---------------------------------------------------------------------
    // Main cell frame
    // ---------------------------------------------------------------------

    Rectangle {
        id: frame

        smooth: true

        anchors.fill: parent
        anchors.margins: 20

        gradient: Gradient {
            GradientStop {
                position: 0.00
                color: "#ededed"
            }

            GradientStop {
                position: 1.00
                color: "#dbdbdb"
            }
        }

        MouseArea {
            id: frameMouseArea

            anchors.fill: parent

            hoverEnabled: true

            /*
             * IMPORTANT:
             *
             * MouseArea accepts LeftButton by default.
             * Explicitly accepting RightButton is required for
             * onPressed to receive a right mouse click.
             */
            acceptedButtons: Qt.LeftButton | Qt.RightButton

            onEntered: {
                delegate.itemHoveredHandler()
            }

            onExited: {
                frameHoverTimer.start()
                if (contextMenu.visible)
                    Qt.callLater(delegate.updateContextMenuHover)
            }

            onPressed: {
                if (mouse.button === Qt.RightButton) {
                    /*
                     * Right-click also selects the item.
                     */
                    delegate.selectThisItem()

                    /*
                     * Open the context menu.
                     */
                    contextMenu.popup()

                    /*
                     * Do not treat the right-click as a normal
                     * left-click selection/action.
                     */
                    mouse.accepted = true
                }
            }

            onClicked: {
                if (mouse.button === Qt.LeftButton) {
                    delegate.selectThisItem()
                }
            }

            Timer {
                id: frameHoverTimer

                interval: 200
                running: false
                repeat: true

                onTriggered: {
                    if (!frameMouseArea.containsMouse
                            && !photoWrapper.isImageHovered
                            && !labelTitle.hovered
                            && !labelSize.hovered
                            && !labelCreated.hovered
                            && !playLoader.hovered
                            && !deleteLoader.hovered
                            && !delegate.isListenerWorking) {

                        delegate.looseHoverHandler()
                    }
                }
            }
        }

        // -----------------------------------------------------------------
        // Image
        // -----------------------------------------------------------------

        FramedImage {
            id: photoWrapper

            anchors {
                fill: parent
                leftMargin: 10
                topMargin: 10
                rightMargin: 10
                bottomMargin: 40
            }

            imageSource: (thumb.indexOf("http") === 0)
                         ? thumb
                         : ("image://imageprovider/" + thumb)

            smooth: true
            cache: true
            asynchronous: true

            onImageClicked: {
                delegate.selectThisItem()
                qmllistener.onImageClicked(index)
            }

            onImageEntered: {
                delegate.itemHoveredHandler()
            }
        }

        // -----------------------------------------------------------------
        // Delete button
        // -----------------------------------------------------------------

        Loader {
            id: deleteLoader

            anchors.right: frame.right
            anchors.rightMargin: -9

            y: -10

            width: 19
            height: 19

            property bool hovered: item ? item.hovered : false
        }

        // -----------------------------------------------------------------
        // Play button
        // -----------------------------------------------------------------

        Loader {
            id: playLoader

            anchors.centerIn: photoWrapper

            y: 20

            width: 25
            height: 25

            property bool hovered:
                status === Loader.Ready ? item.hovered : false
        }

        Component {
            id: playButton

            Button {
                id: playButtonImpl

                imageSource: "qrc:/control/icoplay"

                tooltipText: qsTr("Hold Shift for embedded player")

                icoLeftOffset: 1

                onClicked: {
                    frameHoverTimer.stop()

                    delegate.selectThisItem()

                    if (mouse.modifiers & Qt.ShiftModifier)
                        qmllistener.onPlayInternal(index)
                    else
                        qmllistener.onPlayClicked(index)

                    frameHoverTimer.start()
                }

                onHoveredChanged: {
                    if (playButtonImpl.hovered)
                        photoWrapper.rotation = 0
                    else
                        photoWrapper.rotation = delegate.randomAngle
                }
            }
        }

        Component {
            id: deleteButton

            Button {
                id: deleteButtonImpl

                imageSource: "qrc:/control/icodelete"

                tooltipText: qsTr("Delete")

                onClicked: {
                    frameHoverTimer.stop()

                    delegate.deleteItemByIndex(index)

                    frameHoverTimer.start()
                }
            }
        }

        // -----------------------------------------------------------------
        // Text information
        // -----------------------------------------------------------------

        Item {
            id: textInfoFrame

            anchors.bottom: frame.bottom

            height: textInfoHeight
            width: frame.width

            Label {
                id: labelSize

                anchors {
                    top: textInfoFrame.top
                    topMargin: 15
                    right: textInfoFrame.right
                    rightMargin: 120
                    left: textInfoFrame.left
                    leftMargin: 10
                }

                height: 15

                color: "#333"
                font.pixelSize: 11

                text: fileSize
            }

            Label {
                id: labelCreated

                anchors {
                    top: textInfoFrame.top
                    topMargin: 15
                    right: textInfoFrame.right
                    rightMargin: 10
                    left: textInfoFrame.left
                    leftMargin: 122
                }

                height: 15

                font.pixelSize: 11
                color: "#333"

                text: fileCreated
            }

            LabelWithTooltip {
                id: labelTitle

                anchors {
                    top: textInfoFrame.top
                    right: textInfoFrame.right
                    rightMargin: 10
                    left: textInfoFrame.left
                    leftMargin: 10
                }

                height: 15

                fontPixelSize: 12

                text: title

                enableTooltip: true

                onClicked: {
                    delegate.selectThisItem()
                }
            }
        }
    }
}
