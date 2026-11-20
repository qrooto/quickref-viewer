#include "backend.h"
#include "safeio.h"
#include <QDir>
#include <dirent.h>
#include <sys/stat.h>
#include <functional>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextCursor>
#include <QTextImageFormat>
#include <QImage>
#include <QFont>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <algorithm>

namespace {
// Parsing untrusted Markdown must not read local images or request resources,
// even before image objects are removed from the document for QML rendering.
class PassiveDocument final : public QTextDocument {
protected:
 QVariant loadResource(int, const QUrl &) override { return QVariant::fromValue(QImage()); }
};

void replaceImagesWithText(QTextDocument &document) {
 struct Replacement { int start; int length; QString text; };
 QList<Replacement> replacements;
 for(auto block=document.begin();block.isValid();block=block.next()) {
  for(auto it=block.begin();!it.atEnd();++it) {
   const auto fragment=it.fragment();
   if(fragment.isValid() && fragment.charFormat().isImageFormat()) {
    auto alt=fragment.charFormat().property(QTextFormat::ImageAltText).toString();
    replacements.append({fragment.position(),fragment.length(),alt.isEmpty() ? QStringLiteral("[Image omitted]") : alt});
   }
  }
 }
 // Work backwards so positions remain valid; insert plain characters, never
 // HTML or the image's original format/anchor. This covers every image syntax
 // understood by Qt's Markdown parser, including reference-style images.
 for(auto it=replacements.crbegin();it!=replacements.crend();++it) {
  QTextCursor cursor(&document);
  cursor.setPosition(it->start);cursor.setPosition(it->start+it->length,QTextCursor::KeepAnchor);
  cursor.insertText(it->text,QTextCharFormat());
 }
}
}

Backend::Backend(QString folder, QString statePath, QObject *parent):QObject(parent),m_folder(QDir(folder).absolutePath()),m_statePath(statePath) {
 m_debounce.setSingleShot(true); m_debounce.setInterval(160);
 m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(250);
 connect(&m_saveTimer,&QTimer::timeout,this,&Backend::save);
 connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{m_debounce.start();});
 connect(&m_watcher,&QFileSystemWatcher::fileChanged,this,[this]{m_debounce.start();});
 connect(&m_debounce,&QTimer::timeout,this,&Backend::refresh);
 if(!QDir().mkpath(m_folder)) fail(tr("Cannot create notes folder: %1").arg(m_folder));
 m_libraryFd=::open(QFile::encodeName(m_folder).constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
 if(m_libraryFd<0)fail(tr("Cannot read note: %1").arg(SafeIo::systemError()));
 QFile state(m_statePath);
 if(state.open(QIODevice::ReadOnly)) {
  auto obj=QJsonDocument::fromJson(state.readAll()).object();
  for(auto value:obj["tabs"].toArray()) if(valid(value.toString())&&!m_tabs.contains(value.toString())) m_tabs<<value.toString();
  m_current=obj["current"].toString();
  auto positions=obj["positions"].toObject();
  for(auto it=positions.begin();it!=positions.end();++it) m_positions[it.key()]=it.value().toDouble();
 }
 refresh();
 if(!m_tabs.contains(m_current)) m_current=m_tabs.value(0);
 if(m_tabs.isEmpty()&&!m_notes.isEmpty()) open(m_notes.first().toMap()["path"].toString());
 else loadDocument();
}
void Backend::fail(const QString &message) { m_error=message; emit errorChanged(); }
Backend::~Backend() { save(); if(m_libraryFd>=0)::close(m_libraryFd); }
bool Backend::valid(const QString &relative) const {
 if(relative.isEmpty() || relative.contains(QChar(0)) || QDir::isAbsolutePath(relative)
    || QFileInfo(relative).suffix().compare("md",Qt::CaseInsensitive)!=0)return false;
 const int fd=quickrefOpenBeneath(m_libraryFd,QFile::encodeName(relative).constData(),O_RDONLY|O_NONBLOCK);
 if(fd<0)return false;
 struct stat metadata{};const bool regular=::fstat(fd,&metadata)==0 && S_ISREG(metadata.st_mode);
 ::close(fd);return regular;
}
void Backend::refresh() {
 auto watched=m_watcher.files()+m_watcher.directories(); if(!watched.isEmpty())m_watcher.removePaths(watched);
 QStringList paths{m_folder}; m_contents.clear();
 // Enumerate pinned directory descriptors, never a checked pathname. Every
 // descendant is opened beneath the original root in one kernel operation.
 std::function<void(int,const QString &)> scan=[&](int fd,const QString &prefix) {
  DIR *stream=::fdopendir(fd);
  if(!stream){::close(fd);return;}
  while(auto *entry=::readdir(stream)) {
   const QByteArray name(entry->d_name);
   if(name=="." || name=="..")continue;
   const QString relative=prefix+QFile::decodeName(name);
   const int child=quickrefOpenBeneath(m_libraryFd,QFile::encodeName(relative).constData(),O_RDONLY|O_NONBLOCK);
   if(child<0) {
    if(errno!=ELOOP && errno!=EXDEV && errno!=ENOENT)fail(tr("Cannot read note: %1").arg(relative));
    continue;
   }
   struct stat metadata{};
   if(::fstat(child,&metadata)<0){::close(child);continue;}
   if(S_ISDIR(metadata.st_mode)) {
    paths<<m_folder+"/"+relative;scan(child,relative+"/");continue;
   }
   if(S_ISREG(metadata.st_mode) && QFileInfo(relative).suffix().compare("md",Qt::CaseInsensitive)==0) {
    QFile file;
    if(file.open(child,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle)) {
     paths<<m_folder+"/"+relative;m_contents[relative]=QString::fromUtf8(file.readAll());
     continue;
    }
    fail(tr("Cannot read note: %1").arg(relative));
   }
   ::close(child);
  }
  ::closedir(stream);
 };
 const int root=quickrefOpenBeneath(m_libraryFd,".",O_RDONLY|O_DIRECTORY);
 if(root>=0)scan(root,{});
 else fail(tr("Cannot read note: %1").arg(SafeIo::systemError()));
 if(!QFileInfo::exists(m_folder))paths<<QFileInfo(m_folder).absolutePath();
 for(auto &p:paths)if(QFileInfo::exists(p))m_watcher.addPath(p);
 search(m_query);
 // Closed or deleted files never leave ghost tabs behind.
 for(int i=m_tabs.size()-1;i>=0;--i)if(!m_contents.contains(m_tabs[i]))m_tabs.removeAt(i);
 if(!m_tabs.contains(m_current))m_current=m_tabs.value(0);
 emit tabsChanged(); loadDocument();
}
void Backend::search(const QString &query) {
 m_query=query;m_notes.clear();
 auto words=query.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
 for(auto it=m_contents.cbegin();it!=m_contents.cend();++it) {
  QString title=QFileInfo(it.key()).completeBaseName();
  auto heading=QRegularExpression("^#\\s+(.+)$",QRegularExpression::MultilineOption).match(it.value());
  if(heading.hasMatch())title=heading.captured(1).trimmed();
  bool matches=true;for(auto &word:words)if(!(QFileInfo(it.key()).completeBaseName()+" "+title).contains(word,Qt::CaseInsensitive))matches=false;
  if(matches)m_notes<<QVariantMap{{"path",it.key()},{"title",title}};
 }
 emit notesChanged();
}
QVariantList Backend::tabs() const {
 QVariantList result;
 for(auto &path:m_tabs) result<<QVariantMap{{"path",path},{"title",QFileInfo(path).completeBaseName()}};
 return result;
}
void Backend::open(const QString &path) {
 if(!valid(path)){fail(tr("Note is missing or outside the notes folder: %1").arg(path));return;}
 if(!m_tabs.contains(path))m_tabs<<path;
 m_current=path;emit tabsChanged();loadDocument();save();
}
void Backend::closeTab(int index) {
 if(index<0||index>=m_tabs.size())return;
 auto removed=m_tabs.takeAt(index);
 if(removed==m_current)m_current=m_tabs.value(qMin(index,int(m_tabs.size())-1));
 emit tabsChanged();loadDocument();save();
}
void Backend::cycle(int direction) {
 if(m_tabs.isEmpty())return;
 open(m_tabs[(active()+direction+m_tabs.size())%m_tabs.size()]);
}
void Backend::loadDocument() {
 m_blocks=parse(m_contents.value(m_current));emit documentChanged();
}
QVariantList Backend::parse(const QString &markdown) {
 QVariantList result;QStringList buffer;QString language;bool code=false;QString fence;
 auto flush=[&] {
  QString value=buffer.join('\n');buffer.clear();if(value.trimmed().isEmpty())return;
  QString html;
  if(code) {
   // Tokenize before escaping so notes can never inject rich text.
   QRegularExpression tokens(R"re((#[^\n]*|//[^\n]*|"[^"\n]*"|'[^'\n]*'|\b(?:FROM|RUN|COPY|ADD|WORKDIR|ENV|ARG|EXPOSE|CMD|ENTRYPOINT|USER|LABEL|VOLUME|HEALTHCHECK|sudo|docker|git|systemctl|journalctl|ip|ss|grep|find|chmod|chown|curl|kubectl)\b))re");
   auto matches=tokens.globalMatch(value); qsizetype offset=0;
   while(matches.hasNext()) {
    auto match=matches.next(); html+=value.mid(offset,match.capturedStart()-offset).toHtmlEscaped();
    auto token=match.captured();
    auto color=token.startsWith('#')||token.startsWith("//")?"#8193a8":token.startsWith('\"')||token.startsWith('\'')?"#b3d99c":"#82b7f1";
    html+="<span style=\"color:"+QString(color)+"\">"+token.toHtmlEscaped()+"</span>";offset=match.capturedEnd();
   }
   html+=value.mid(offset).toHtmlEscaped();html="<pre style=\"font-family:monospace; color:#c8d7e8; white-space:pre-wrap\">"+html+"</pre>";
  } else {
   PassiveDocument doc; doc.setDefaultFont(QFont("sans-serif",11)); doc.setDefaultStyleSheet("body { color: #d9e0eb; font-family: sans-serif; font-size: 15px; } h1 { color: #f1f5fb; font-size: 26px; } h2 { color: #f1f5fb; font-size: 20px; } h3 { color: #f1f5fb; } a { color: #80bfff; } th, td { padding: 7px; } code { color: #a8d8b9; }");
   doc.setMarkdown(value,QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)|QTextDocument::MarkdownNoHTML);
   replaceImagesWithText(doc);
   html=doc.toHtml();
  }
  result<<QVariantMap{{"code",code},{"text",value},{"html",html},{"language",language}};
 };
 for(const auto &line:markdown.split('\n')) {
  auto match=QRegularExpression("^ {0,3}(`{3,}|~{3,})(.*)$").match(line);
  if(match.hasMatch()&&(!code||(match.captured(1)[0]==fence[0]&&match.captured(1).size()>=fence.size()&&match.captured(2).trimmed().isEmpty()))) {
   flush();code=!code;fence=match.captured(1);language=code?match.captured(2).trimmed():QString();
  }else buffer<<line;
 }
 flush();return result;
}
void Backend::rememberPosition(double position) { if(m_current.isEmpty())return;m_positions[m_current]=qMax(0.0,position);m_saveTimer.start(); }
void Backend::save() {
 QJsonObject positions;for(auto it=m_positions.cbegin();it!=m_positions.cend();++it)positions[it.key()]=it.value();
 QJsonObject obj{{"tabs",QJsonArray::fromStringList(m_tabs)},{"current",m_current},{"positions",positions}};
 if(!QDir().mkpath(QFileInfo(m_statePath).absolutePath())) { fail(tr("Cannot save session"));return; }
 QSaveFile file(m_statePath);auto bytes=QJsonDocument(obj).toJson();
 if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())fail(tr("Cannot save session"));
}
void Backend::copy(const QString &text) { QGuiApplication::clipboard()->setText(text); }
void Backend::openFolder() { emit externalRequested(m_folder); }
void Backend::edit() {
 if(!valid(m_current)) { fail(tr("Select a note first")); return; }
 emit externalRequested(m_folder+"/"+m_current);
}
bool Backend::createNote(const QString &name, const QString &text) {
 QString filename=name.trimmed();
 if(filename.isEmpty() || filename=="." || filename==".." || filename.contains('/') || filename.contains('\\') || filename.contains(QRegularExpression("[\\x00-\\x1f]"))) {
  fail(tr("Enter a filename without slashes")); return false;
 }
 if(!filename.endsWith(".md",Qt::CaseInsensitive))filename+=".md";
 QString error;
 if(!SafeIo::publishNew(m_libraryFd,QFile::encodeName(filename),text.toUtf8(),error)) {
  fail(tr("Cannot save note: %1").arg(error));return false;
 }
 fail({});m_query.clear();refresh();open(filename);return true;
}
