import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100; height: 650
    minimumWidth: 680; minimumHeight: 340
    visible: true
    title: "Quickref Viewer"
    color: "#10151d"
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius
    property bool restoring: false
    property bool helpVisible: false
    property real pendingPosition: 0
    property bool findVisible: false
    property var matches: []
    property int matchIndex: -1
    function openFind() {
        findVisible = true
        noteFind.forceActiveFocus()
        noteFind.selectAll()
    }
    function clearMatch() {
        for (let i = 0; i < blocksRepeater.count; ++i) {
            const section = blocksRepeater.itemAt(i)
            if (section) section.searchEditor.deselect()
        }
    }
    function findMatches() {
        clearMatch()
        const found = []
        const query = noteFind.text.toLowerCase()
        if (findVisible && query.length) {
            for (let i = 0; i < blocksRepeater.count; ++i) {
                const section = blocksRepeater.itemAt(i)
                if (!section) continue
                const editor = section.searchEditor
                const plain = editor.getText(0, editor.length).toLowerCase()
                let offset = 0
                while ((offset = plain.indexOf(query, offset)) !== -1) {
                    found.push({editor: editor, start: offset, end: offset + query.length})
                    offset += query.length
                }
            }
        }
        matches = found
        matchIndex = found.length ? 0 : -1
        showMatch()
    }
    function showMatch() {
        clearMatch()
        if (matchIndex < 0 || matchIndex >= matches.length) return
        const match = matches[matchIndex]
        match.editor.select(match.start, match.end)
        const rect = match.editor.positionToRectangle(match.start)
        const point = match.editor.mapToItem(reader.contentItem, rect.x, rect.y)
        reader.contentY = Math.max(0, Math.min(reader.contentHeight - reader.height,
                                             point.y - reader.height / 3))
    }
    function nextMatch(direction) {
        if (!matches.length) return
        matchIndex = (matchIndex + direction + matches.length) % matches.length
        showMatch()
    }
    function closeFind() {
        findVisible = false
        clearMatch()
        matches = []
        matchIndex = -1
        reader.forceActiveFocus()
    }
    Shortcut { sequence: "Ctrl+F"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: win.openFind() }
    Shortcut { sequence: "F3"; enabled: win.findVisible && !composer.visible; context: Qt.ApplicationShortcut; onActivated: win.nextMatch(1) }
    Shortcut { sequence: "Shift+F3"; enabled: win.findVisible && !composer.visible; context: Qt.ApplicationShortcut; onActivated: win.nextMatch(-1) }
    Shortcut { sequence: "Escape"; enabled: win.findVisible && !composer.visible; context: Qt.ApplicationShortcut; onActivated: win.closeFind() }
    function restorePosition() {
        restoring = true
        pendingPosition = backend.position()
        restoreTimer.restart()
    }
    Timer {
        id: restoreTimer
        interval: 60
        onTriggered: {
            documentColumn.forceLayout()
            reader.contentY = Math.min(win.pendingPosition, Math.max(0, reader.contentHeight-reader.height))
            win.restoring = false
            if (win.findVisible) win.findMatches()
        }
    }
    Shortcut { sequence: "Ctrl+K"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: { search.forceActiveFocus(); search.selectAll() } }
    Shortcut { sequence: "Ctrl+Tab"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: backend.cycle(1) }
    Shortcut { sequence: "Ctrl+Shift+Tab"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: backend.cycle(-1) }
    Shortcut { sequence: "Ctrl+W"; context: Qt.ApplicationShortcut; enabled: !composer.visible; onActivated: backend.edit() }
    Shortcut { sequence: "Ctrl+E"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: backend.edit() }
    Shortcut { sequence: "Ctrl+O"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: backend.openFolder() }
    Shortcut { sequence: "F5"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: backend.refresh() }
    Shortcut { sequence: "?"; enabled: !search.activeFocus && !noteFind.activeFocus && !composer.visible; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; enabled: !search.activeFocus && !noteFind.activeFocus && !composer.visible; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequence: "Space"; enabled: !search.activeFocus && !noteFind.activeFocus && !composer.visible; context: Qt.ApplicationShortcut; onActivated: reader.contentY = Math.min(Math.max(0,reader.contentHeight-reader.height),reader.contentY+reader.height*0.8) }
    Connections { target: backend; function onTabsChanged() { win.restoring = true } function onDocumentChanged() { win.restorePosition() } }
    Component.onCompleted: restorePosition()
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true
        ColumnLayout {
            anchors.fill: parent; spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 64
                color: "#141b25"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 16; spacing: 12
                    Rectangle { width: 30; height: 30; radius: Math.min(4,theme.radius); color: theme.accent
                        Text { textFormat: Text.PlainText; anchors.centerIn: parent; text: ">_"; color: theme.accentForeground; font.bold: true; font.pixelSize: 16 }
                    }
                    Column {
                        Text { textFormat: Text.PlainText; text: "Quickref Viewer"; color: "#f2f5fa"; font.pixelSize: 19; font.bold: true }
                        Text { textFormat: Text.PlainText; text: qsTr("Your Markdown notes, a shortcut away"); color: "#8a98ad"; font.pixelSize: 11 }
                    }
                    Item { Layout.fillWidth: true }
                    Button { text: qsTr("New note"); highlighted: true; onClicked: composer.open() }
                    Button { text: qsTr("Notes folder"); flat: true; onClicked: backend.openFolder() }
                    Button { text: "?"; flat: true; onClicked: win.helpVisible = true }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#263141" }
            RowLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                Rectangle {
                    Layout.preferredWidth: Math.min(280,win.width*0.27); Layout.fillHeight: true; color: "#131a23"
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 12
                        TextField {
                            id: search; objectName: "search"
                            Layout.fillWidth: true; placeholderText: qsTr("Search note titles · Ctrl+K")
                            font.pixelSize: 13; selectByMouse: true
                            onTextChanged: backend.search(text)
                            Keys.onDownPressed: { notesList.forceActiveFocus(); notesList.currentIndex = 0 }
                            Keys.onReturnPressed: {
                                if(backend.notes.length>0) { backend.open(backend.notes[0].path); reader.forceActiveFocus() }
                            }
                            Keys.onEscapePressed: { text = ""; reader.forceActiveFocus() }
                        }
                        RowLayout {
                            Text { textFormat: Text.PlainText; text: qsTr("LIBRARY"); color: "#8796ab"; font.pixelSize: 11; font.letterSpacing: 1.5 }
                            Item { Layout.fillWidth: true }
                            Text { textFormat: Text.PlainText; text: backend.notes.length; color: theme.accent; font.pixelSize: 12 }
                        }
                        ListView {
                            id: notesList; objectName: "notesList"
                            Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                            model: backend.notes; currentIndex: -1
                            ScrollBar.vertical: ScrollBar {}
                            Keys.onReturnPressed: if(currentIndex>=0) { backend.open(backend.notes[currentIndex].path); reader.forceActiveFocus() }
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: notesList.width; height: 66
                                radius: theme.radius
                                color: backend.current===modelData.path ? "#253244" : noteHover.hovered || (notesList.activeFocus && notesList.currentIndex===index) ? "#1b2635" : "transparent"
                                Rectangle { width: 3; height: 24; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; color: backend.current===modelData.path ? theme.accent : "transparent" }
                                Column {
                                    anchors.fill: parent; anchors.margins: 12; spacing: 6
                                    Text { textFormat: Text.PlainText; width: parent.width; text: modelData.title; elide: Text.ElideRight; color: "#e0e6ef"; font.pixelSize: 14 }
                                    Text { textFormat: Text.PlainText; width: parent.width; text: modelData.path; elide: Text.ElideMiddle; color: "#8494aa"; font.pixelSize: 10 }
                                }
                                HoverHandler { id: noteHover }
                                TapHandler { onTapped: { backend.open(modelData.path); reader.forceActiveFocus() } }
                            }
                            Label { textFormat: Text.PlainText; anchors.centerIn: parent; width: parent.width-12; visible: backend.notes.length===0; text: search.text ? qsTr("No matching notes") : qsTr("Add .md files to your notes folder"); wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: "#8a98ad" }
                        }
                        Label {
                            textFormat: Text.PlainText
                            Layout.fillWidth: true
                            text: backend.folder
                            elide: Text.ElideMiddle; color: "#62738a"; font.pixelSize: 10
                            ToolTip {
                                id: folderTip
                                objectName: "notesFolderTooltip"
                                visible: folderHover.hovered
                                contentItem: Text { textFormat: Text.PlainText; text: backend.folder; color: "#e0e6ef"; font: folderTip.font }
                            }
                            HoverHandler { id: folderHover }
                        }
                    }
                }
                Rectangle { Layout.fillHeight: true; width: 1; color: "#263141" }
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 46; color: "#111821"
                        ListView {
                            id: tabList; objectName: "tabList"; anchors.fill: parent; orientation: ListView.Horizontal; clip: true
                            model: backend.tabs; spacing: 1
                            onCountChanged: Qt.callLater(function(){ tabList.positionViewAtIndex(backend.active,ListView.Contain) })
                            Connections { target: backend; function onTabsChanged(){ tabList.positionViewAtIndex(backend.active,ListView.Contain) } }
                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: Math.min(230,Math.max(140,tabLabel.implicitWidth+56)); height: tabList.height
                                color: backend.active===index ? "#1c2735" : "#141c26"
                                TapHandler { onTapped: backend.open(modelData.path) }
                                Text { textFormat: Text.PlainText; id: tabLabel; anchors.left: parent.left; anchors.leftMargin: 14; anchors.verticalCenter: parent.verticalCenter; width: parent.width-50; text: modelData.title; elide: Text.ElideRight; font.pixelSize: 12; color: backend.active===index ? "#edf3fc" : "#8898ad" }
                                ToolButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 32; height: 32; text: "×"; onClicked: backend.closeTab(index) }
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: backend.active===index ? theme.accent : "transparent" }
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#263141" }
                    RowLayout {
                        objectName: "noteToolbar"
                        Layout.minimumHeight: implicitHeight
                        Layout.fillWidth: true; Layout.leftMargin: 24; Layout.rightMargin: 16; spacing: 8
                        Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: backend.current; elide: Text.ElideMiddle; color: "#8191a7"; font.pixelSize: 11 }
                        Button { text: qsTr("Find in note · Ctrl+F"); flat: true; enabled: backend.active>=0; onClicked: win.openFind() }
                        Button { text: qsTr("Open in editor"); flat: true; enabled: backend.active>=0; onClicked: backend.edit() }
                    }
                    RowLayout {
                        objectName: "noteFindBar"
                        visible: win.findVisible
                        Layout.minimumHeight: implicitHeight
                        Layout.fillWidth: true; Layout.leftMargin: 24; Layout.rightMargin: 16
                        TextField {
                            id: noteFind; objectName: "noteFind"
                            Layout.fillWidth: true
                            placeholderText: qsTr("Find in note · Ctrl+F")
                            selectByMouse: true
                            onTextChanged: win.findMatches()
                            Keys.onReturnPressed: function(event) { win.nextMatch(event.modifiers & Qt.ShiftModifier ? -1 : 1) }
                        }
                        Label { textFormat: Text.PlainText; objectName: "matchCount"; text: noteFind.text.length ? (win.matches.length ? (win.matchIndex + 1) + " / " + win.matches.length : qsTr("No matches")) : ""; color: "#90a9c6" }
                        ToolButton { text: "↑"; enabled: win.matches.length > 0; onClicked: win.nextMatch(-1); ToolTip.text: qsTr("Previous match · Shift+Enter"); ToolTip.visible: hovered }
                        ToolButton { text: "↓"; enabled: win.matches.length > 0; onClicked: win.nextMatch(1); ToolTip.text: qsTr("Next match · Enter"); ToolTip.visible: hovered }
                        ToolButton { text: "×"; onClicked: win.closeFind() }
                    }
                    Flickable {
                        id: reader; objectName: "reader"
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                        Layout.minimumHeight: 0
                        contentWidth: width; contentHeight: documentColumn.y+documentColumn.height+40
                        boundsBehavior: Flickable.StopAtBounds
                        WheelHandler {
                            target: null
                            onWheel: function(event) {
                                // Keep precise touchpad motion and give mouse wheels a useful step.
                                const distance = event.pixelDelta.y !== 0
                                    ? event.pixelDelta.y * 6
                                    : event.angleDelta.y / 120 * 240
                                reader.contentY = Math.max(0, Math.min(
                                    Math.max(0, reader.contentHeight - reader.height),
                                    reader.contentY - distance))
                                event.accepted = true
                            }
                        }
                        ScrollBar.vertical: ScrollBar {}
                        onContentYChanged: if(!win.restoring)backend.rememberPosition(contentY)
                        Keys.onDownPressed: contentY=Math.min(Math.max(0,contentHeight-height),contentY+40)
                        Keys.onUpPressed: contentY=Math.max(0,contentY-40)
                        Keys.onPressed: function(event) {
                            if(event.key===Qt.Key_PageDown) { contentY=Math.min(Math.max(0,contentHeight-height),contentY+height*0.8); event.accepted=true }
                            if(event.key===Qt.Key_PageUp) { contentY=Math.max(0,contentY-height*0.8); event.accepted=true }
                        }
                        Column {
                            id: documentColumn
                            objectName: "documentColumn"
                            y: 16
                            x: 28; width: Math.max(100,reader.width-56); spacing: 16
                            Repeater {
                                id: blocksRepeater
                                model: backend.blocks
                                delegate: Rectangle {
                                    id: section
                                    required property var modelData
                                    property var searchEditor: modelData.code ? codeText : prose
                                    width: documentColumn.width
                                    height: modelData.code ? codeColumn.height+24 : prose.height
                                    radius: theme.radius
                                    color: modelData.code ? "#192330" : "transparent"
                                    border.width: modelData.code ? 1 : 0; border.color: "#2b3b50"
                                    TextArea {
                                        persistentSelection: true
                                        selectionColor: theme.accent; selectedTextColor: theme.accentForeground
                                        id: prose; visible: !section.modelData.code
                                        width: parent.width; padding: 0
                                        text: section.modelData.html; textFormat: TextEdit.RichText
                                        readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap
                                        color: "#d9e0eb"; font.pixelSize: 15
                                        background: null
                                        onLinkActivated: function(link) { if(link.startsWith("https://")||link.startsWith("http://"))Qt.openUrlExternally(link) }
                                    }
                                    Column {
                                        id: codeColumn; visible: section.modelData.code
                                        x: 14; y: 12; width: parent.width-28; spacing: 10
                                        RowLayout {
                                            width: parent.width; height: 26
                                            Text { textFormat: Text.PlainText; text: section.modelData.language || qsTr("text"); color: "#90a9c6"; font.family: "monospace"; font.pixelSize: 11 }
                                            Item { Layout.fillWidth: true }
                                            Button {
                                                id: copyButton; text: copied ? qsTr("Copied") : qsTr("Copy"); flat: true; property bool copied: false
                                                implicitHeight: 30; font.pixelSize: 11
                                                onClicked: { backend.copy(section.modelData.text); copied=true; copiedTimer.restart() }
                                                Timer { id: copiedTimer; interval: 1400; onTriggered: copyButton.copied=false }
                                            }
                                        }
                                        TextArea {
                                            id: codeText
                                            persistentSelection: true
                                            selectionColor: theme.accent; selectedTextColor: theme.accentForeground
                                            width: parent.width; padding: 0
                                            text: section.modelData.html; textFormat: TextEdit.RichText
                                            color: "#b7d4ef"; font.family: "monospace"; font.pixelSize: 13
                                            readOnly: true; selectByMouse: true; wrapMode: TextEdit.WrapAnywhere
                                            background: null
                                        }
                                    }
                                }
                            }
                            Label { textFormat: Text.PlainText; width: parent.width; visible: backend.active<0; text: qsTr("Your answers, a keystroke away.\nChoose a note on the left."); color: "#8798b0"; font.pixelSize: 20; wrapMode: Text.WordWrap; topPadding: 50 }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 28; color: "#141b25"
                        Text { textFormat: Text.PlainText; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 16; text: qsTr("Ctrl+N  New note     Ctrl+K  Search     Ctrl+F  Find in note     Ctrl+W  Open in editor     ?  Shortcuts"); color: "#7e90a7"; font.pixelSize: 10 }
                    }
                }
            }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: backend.error!==""; text: backend.error; color: "#ffadad"; padding: 10; wrapMode: Text.WordWrap }
        }
    }
    Popup {
        id: help; anchors.centerIn: Overlay.overlay; width: Math.min(win.width-40,560)
        modal: true; visible: win.helpVisible; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: win.helpVisible=false
        contentItem: Column {
            spacing: 18
            Label { textFormat: Text.PlainText; text: qsTr("Keyboard shortcuts"); font.pixelSize: 22; font.bold: true }
            Label { textFormat: Text.PlainText; width: parent.width; text: qsTr("Super+Alt+grave — Show / hide Quickref\nCtrl+K — Search note titles\nCtrl+F — Find in current note\nEnter / Shift+Enter — Next / previous match\nEscape — Close find\nEnter — Open the first result\nCtrl+Tab / Ctrl+Shift+Tab — Switch tabs\nCtrl+N — Create a note\nCtrl+W / Ctrl+E — Open note in tiling editor\n× on tab — Close tab\nCtrl+O — Open notes folder\nSpace / PageDown — Scroll down\nF5 — Reload notes\nQ — Quit\n? — This help"); wrapMode: Text.WordWrap; lineHeight: 1.5 }
            Button { text: qsTr("Got it"); onClicked: win.helpVisible=false }
        }
    }
    function saveNote() {
        if (backend.createNote(noteName.text, noteBody.text)) {
            search.text = ""
            composer.close()
            noteName.text = ""; noteBody.text = ""
            reader.forceActiveFocus()
        }
    }
    function cancelNote() {
        if (noteName.text.length || noteBody.text.length) discardDialog.open()
        else composer.close()
    }
    property bool quitAfterDiscard: false
    onClosing: function(close) {
        if(composer.visible && (noteName.text.length || noteBody.text.length)) {
            close.accepted = false
            quitAfterDiscard = true
            discardDialog.open()
        }
    }
    Shortcut { sequence: "Ctrl+N"; enabled: !composer.visible; context: Qt.ApplicationShortcut; onActivated: composer.open() }
    Shortcut { sequence: "Ctrl+S"; enabled: composer.visible && !discardDialog.visible; context: Qt.ApplicationShortcut; onActivated: win.saveNote() }
    Shortcut { sequence: "Escape"; enabled: composer.visible && !discardDialog.visible; context: Qt.ApplicationShortcut; onActivated: win.cancelNote() }
    Dialog {
        id: composer; objectName: "composer"
        anchors.centerIn: Overlay.overlay
        width: Math.min(win.width-40,900); height: Math.min(win.height-24,690)
        modal: true; closePolicy: Popup.NoAutoClose
        title: qsTr("Create a note")
        onOpened: noteName.forceActiveFocus()
        contentItem: ColumnLayout {
            spacing: 12
            TextField {
                id: noteName; objectName: "noteName"; Layout.fillWidth: true
                placeholderText: qsTr("Filename, for example Docker commands")
                selectByMouse: true
                Keys.onReturnPressed: noteBody.forceActiveFocus()
            }
            Label { textFormat: Text.PlainText; text: qsTr("Markdown content"); color: "#96a6bb" }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                TextArea {
                    id: noteBody; objectName: "noteBody"
                    placeholderText: qsTr("# Topic\n\nYour explanation and commands…")
                    font.family: "monospace"; font.pixelSize: 14
                    textFormat: TextEdit.PlainText; wrapMode: TextEdit.Wrap
                    selectByMouse: true
                }
            }
            Label { textFormat: Text.PlainText; Layout.fillWidth: true; visible: backend.error!==""; text: backend.error; color: "#ffadad"; wrapMode: Text.WordWrap }
            RowLayout {
                Label { textFormat: Text.PlainText; Layout.fillWidth: true; text: qsTr("Saved as .md in your notes folder"); color: "#8698ae"; font.pixelSize: 11 }
                Button { text: qsTr("Cancel"); onClicked: win.cancelNote() }
                Button { text: qsTr("Save note · Ctrl+S"); enabled: noteName.text.trim().length>0; highlighted: true; onClicked: win.saveNote() }
            }
        }
    }
    Dialog {
        id: discardDialog; objectName: "discardDialog"
        width: Math.min(win.width-40,500)
        anchors.centerIn: Overlay.overlay; modal: true
        title: qsTr("Discard this draft?")
        onAccepted: {
            noteName.text=""; noteBody.text=""; composer.close()
            if(win.quitAfterDiscard)Qt.quit()
        }
        onRejected: win.quitAfterDiscard=false
        contentItem: ColumnLayout {
            Label { textFormat: Text.PlainText; text: qsTr("The note has not been saved."); padding: 12 }
            RowLayout {
                Button { text: qsTr("Keep editing"); onClicked: discardDialog.reject() }
                Button { text: qsTr("Discard"); onClicked: discardDialog.accept() }
            }
        }
    }

}
