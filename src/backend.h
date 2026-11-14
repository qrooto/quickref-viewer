#pragma once
#include <QObject>
#include <QVariantList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QMap>

class Backend : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList notes READ notes NOTIFY notesChanged)
 Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
 Q_PROPERTY(QVariantList blocks READ blocks NOTIFY documentChanged)
 Q_PROPERTY(QString folder READ folder CONSTANT)
 Q_PROPERTY(QString current READ current NOTIFY documentChanged)
 Q_PROPERTY(QString error READ error NOTIFY errorChanged)
 Q_PROPERTY(int active READ active NOTIFY tabsChanged)
public:
 explicit Backend(QString folder, QString statePath, QObject *parent=nullptr);
 ~Backend() override { save(); }
 QVariantList notes() const { return m_notes; }
 QVariantList tabs() const;
 QVariantList blocks() const { return m_blocks; }
 QString folder() const { return m_folder; }
 QString current() const { return m_current; }
 QString error() const { return m_error; }
 int active() const { return m_tabs.indexOf(m_current); }
 Q_INVOKABLE void search(const QString &query);
 Q_INVOKABLE void open(const QString &path);
 Q_INVOKABLE void closeTab(int index);
 Q_INVOKABLE void cycle(int direction);
 Q_INVOKABLE void refresh();
 Q_INVOKABLE void copy(const QString &text);
 Q_INVOKABLE void openFolder();
 Q_INVOKABLE void edit();
 Q_INVOKABLE bool createNote(const QString &name, const QString &text);
 void launchError(const QString &message) { fail(message); }
 Q_INVOKABLE void rememberPosition(double position);
 Q_INVOKABLE double position() const { return m_positions.value(m_current,0); }
 static QVariantList parse(const QString &markdown);
signals:
 void externalRequested(const QString &path);
 void notesChanged();
 void tabsChanged();
 void documentChanged();
 void errorChanged();
private:
 bool valid(const QString &relative) const;
 void loadDocument();
 void save();
 void fail(const QString &message);
 QString m_folder, m_statePath, m_current, m_query, m_error;
 QStringList m_tabs;
 QMap<QString,QString> m_contents;
 QMap<QString,double> m_positions;
 QVariantList m_notes, m_blocks;
 QFileSystemWatcher m_watcher;
 QTimer m_debounce, m_saveTimer;
};
