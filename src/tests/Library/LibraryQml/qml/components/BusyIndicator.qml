import QtQuick 1.1

Image {
    id: container
    property bool on: false

    source: "qrc:/busy"; visible: container.on
    NumberAnimation on rotation { running: container.on; from: 0; to: 360; loops: Animation.Infinite; duration: 1200 }
}

