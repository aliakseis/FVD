import QtQuick 2.0
//import QtComponents 1.0

Item {
    id: placeholder;
    default property alias data: topLevelItem.data;

    // If true, the toplevel item will be constrained to be inside the
    // topLevelParent.
    //property alias keepInside: topLevelItem.keepInside;

    //TopLevelItemHelper { id: topLevelItem; }
}
