import QtQuick

// -1 unknown, 0 good, 1 warning, 2 error, 3 information
Image {
    property int level: 0

    width: 16
    height: 16
    sourceSize: Qt.size(16, 16)
    source: level < 0 ? "icons/unknown.svg"
          : level >= 3 ? "icons/information.svg"
          : level === 2 ? "icons/error.svg"
          : level === 1 ? "icons/warning.svg"
          : "icons/check.svg"
}
