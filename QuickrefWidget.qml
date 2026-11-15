import QtQuick
import Quickshell
import qs.Ui as Ui

Ui.BarWidget {
  id: root
  moduleName: "io.github.qrooto.quickref-viewer"
  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  function launch() {
    // Use an argument array: repository paths may contain spaces.
    var script = decodeURIComponent(Qt.resolvedUrl("bin/launch").toString().replace(/^file:\/\//, ""))
    Quickshell.execDetached(["sh", script])
  }

  Ui.BarIconButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: "󰈙"
    tooltipText: "Quickref Viewer — Markdown notes"
    onPressed: function(mouseButton) {
      if (mouseButton === Qt.LeftButton) root.launch()
    }
  }
}
