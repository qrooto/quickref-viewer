#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickStyle>
#include <QClipboard>
#include "backend.h"
#include "safeio.h"
#include "theme.h"
#include "translator.h"
#include <functional>
#include <fcntl.h>
#include <unistd.h>
#include <utility>
#include <sys/stat.h>
#include <cerrno>

static std::function<void()> beforePublication;
extern "C" int __real_linkat(int, const char *, int, const char *, int);
extern "C" int __wrap_linkat(int from, const char *source, int to, const char *target, int flags) {
 auto hook=std::exchange(beforePublication,{});
 if(hook)hook();
 return __real_linkat(from,source,to,target,flags);
}

static QByteArray racedReadPath;
static std::function<void()> beforeReadOpen, afterReadOpen;
static bool unavailableRead=false;
extern "C" int __real_quickrefOpenBeneath(int, const char *, int);
extern "C" int __wrap_quickrefOpenBeneath(int directory,const char *path,int flags) {
 if(unavailableRead){errno=ENOSYS;return -1;}
 const bool selected=racedReadPath==path;
 if(selected && beforeReadOpen){auto hook=std::exchange(beforeReadOpen,{});hook();}
 const int fd=__real_quickrefOpenBeneath(directory,path,flags);
 if(selected && afterReadOpen){auto hook=std::exchange(afterReadOpen,{});hook();}
 return fd;
}

class QuickrefTests:public QObject {
 Q_OBJECT
 static void write(const QString &path,const QByteArray &value){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(value),value.size());}
private slots:
 void initTestCase(){QQuickStyle::setStyle("Material");}
 void searchAndWatch(){
  QTemporaryDir dir; QDir(dir.path()).mkdir("notes");auto notes=dir.path()+"/notes";
  write(notes+"/a.md","# Docker\ncontainer logs");QDir(notes).mkdir("Linux");write(notes+"/Linux/b.md","# Memory\nfree -h");
  Backend b(notes,dir.path()+"/state.json");QCOMPARE(b.notes().size(),2);
  b.search("CONTAINER logs");QCOMPARE(b.notes().size(),0);
  b.search("DOCKER");QCOMPARE(b.notes().size(),1);QCOMPARE(b.notes()[0].toMap()["title"].toString(),QString("Docker"));
  b.search("missing");QCOMPARE(b.notes().size(),0);b.search("");
  write(notes+"/new.md","# New note");QTRY_COMPARE(b.notes().size(),3);
  write(notes+"/a.md","# Changed\nwatcher sees this");b.search("Changed");QTRY_COMPARE(b.notes().size(),1);
  b.search("watcher");QCOMPARE(b.notes().size(),0);b.search("Changed");
  QCOMPARE(b.notes()[0].toMap()["title"].toString(),QString("Changed"));
 }
 void sessionAndConfinement(){
  QTemporaryDir dir;QDir(dir.path()).mkdir("notes");auto notes=dir.path()+"/notes";
  write(notes+"/a.md","# A");write(notes+"/b.md","# B");write(dir.path()+"/secret.md","secret");QFile::link(dir.path()+"/secret.md",notes+"/escape.md");
  {Backend b(notes,dir.path()+"/session");QCOMPARE(b.notes().size(),2);b.open("b.md");b.rememberPosition(123);b.open("a.md");b.open("b.md");QCOMPARE(b.tabs().size(),2);QCOMPARE(b.position(),123.0);b.open("../secret.md");QCOMPARE(b.current(),QString("b.md"));QVERIFY(!b.error().isEmpty());}
  {Backend b(notes,dir.path()+"/session");QCOMPARE(b.current(),QString("b.md"));QCOMPARE(b.position(),123.0);QCOMPARE(b.tabs().size(),2);b.cycle(1);QCOMPARE(b.current(),QString("a.md"));b.closeTab(0);QCOMPARE(b.current(),QString("b.md"));QFile::remove(notes+"/b.md");QTRY_COMPARE(b.tabs().size(),0);QCOMPARE(b.active(),-1);}
 }
 void markdownAndClipboard(){
  auto blocks=Backend::parse("# Hello\n\n| A | B |\n|---|---|\n| C | D |\n\n```bash\nsudo echo '<tag>'\n```\nAfter\n");
  QCOMPARE(blocks.size(),3);QVERIFY(blocks[0].toMap()["html"].toString().contains("<table"));QCOMPARE(blocks[1].toMap()["text"].toString(),QString("sudo echo '<tag>'"));QVERIFY(blocks[1].toMap()["html"].toString().contains("&lt;tag&gt;"));
  auto nested=Backend::parse("````markdown\n```bash\nip a\n```\n````");QCOMPARE(nested.size(),1);QVERIFY(nested[0].toMap()["text"].toString().contains("```bash"));
  auto unsafe=Backend::parse("<img src='https://example.com/private'>");QVERIFY(!unsafe[0].toMap()["html"].toString().contains("<img"));
  QTemporaryDir dir;Backend b(dir.path(),dir.path()+"/session");b.copy("docker ps\n");QCOMPARE(QGuiApplication::clipboard()->text(),QString("docker ps\n"));
 }
 void createWithoutOverwrite(){
  QTemporaryDir dir;auto folder=dir.path()+"/notes";Backend b(folder,dir.path()+"/session");
  QVERIFY(b.createNote(QString::fromUtf8("Команды"),QString::fromUtf8("# Подсказка\n\n```bash\nip a\n```")));
  QCOMPARE(b.current(),QString::fromUtf8("Команды.md"));QCOMPARE(b.notes().size(),1);
  QFile original(folder+"/"+b.current());QVERIFY(original.open(QIODevice::ReadOnly));auto bytes=original.readAll();original.close();
  QVERIFY(!b.createNote(QString::fromUtf8("Команды.md"),"overwrite"));QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),bytes);
  QVERIFY(!b.createNote("../escape","no"));QVERIFY(!QFileInfo::exists(dir.path()+"/escape.md"));
  QVERIFY(!b.createNote("/absolute","no"));QVERIFY(!b.createNote(" ","no"));
  write(dir.path()+"/outside.md","outside");QFile::link(dir.path()+"/outside.md",folder+"/linked.md");QVERIFY(!b.createNote("linked.md","no"));
 }
 void creationPublicationRaces(){
  QTemporaryDir dir;const auto folder=dir.path()+"/notes";
  Backend backend(folder,dir.path()+"/session");
  const auto outside=dir.path()+"/outside.md";
  write(outside,"keep original");
  // Deterministic races, injected after writing/fsync and before linkat.
  for(const auto &kind: {QString("symlink"),QString("dangling"),QString("hardlink"),QString("file")}) {
   const auto target=folder+"/"+kind+".md";
   beforePublication=[&] {
    QVERIFY(!QFileInfo::exists(target));
    if(kind=="file")write(target,"another writer");
    else if(kind=="hardlink")QCOMPARE(::link(QFile::encodeName(outside).constData(),QFile::encodeName(target).constData()),0);
    else QVERIFY(QFile::link(kind=="dangling" ? dir.path()+"/missing.md" : outside,target));
   };
   QVERIFY(!backend.createNote(kind,"must not overwrite"));
   QVERIFY(!beforePublication);
   QFile original(outside);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("keep original"));
   QVERIFY(!QFileInfo::exists(dir.path()+"/missing.md"));
   if(kind=="file") {QFile other(target);QVERIFY(other.open(QIODevice::ReadOnly));QCOMPARE(other.readAll(),QByteArray("another writer"));}
  }
 }
 void creationPinsDirectoryAndPublishesCompleteContents(){
  QTemporaryDir dir;const auto folder=dir.path()+"/notes",moved=dir.path()+"/moved",outside=dir.path()+"/outside";
  Backend backend(folder,dir.path()+"/session");QVERIFY(QDir().mkdir(outside));
  const QByteArray content(128*1024,'x');
  beforePublication=[&] {
   QVERIFY(!QFileInfo::exists(folder+"/complete.md"));
   QVERIFY(QDir().rename(folder,moved));
   QVERIFY(QFile::link(outside,folder));
  };
  QVERIFY(backend.createNote("complete",QString::fromUtf8(content)));
  QVERIFY(!beforePublication);
  QVERIFY(!QFileInfo::exists(outside+"/complete.md"));
  QFile note(moved+"/complete.md");QVERIFY(note.open(QIODevice::ReadOnly));QCOMPARE(note.readAll(),content);
  const auto permissions=QFileInfo(note).permissions();
  QVERIFY(!(permissions & (QFileDevice::ReadGroup|QFileDevice::WriteGroup|QFileDevice::ReadOther|QFileDevice::WriteOther)));
 }
 void refreshRejectsRacedSymlinks(){
  for(bool ancestor : {false,true}) {
   QTemporaryDir dir;const auto folder=dir.path()+"/notes",outside=dir.path()+"/outside";
   QVERIFY(QDir().mkpath(folder+"/nested"));QVERIFY(QDir().mkdir(outside));
   const QString relative=ancestor ? "nested/note.md" : "note.md";
   write(folder+"/"+relative,"# Safe note");write(outside+"/note.md","# Outside secret");
   Backend backend(folder,dir.path()+"/session");QCOMPARE(backend.notes().size(),1);
   racedReadPath=QFile::encodeName(ancestor ? "nested" : relative);
   beforeReadOpen=[&] {
    if(ancestor) {QVERIFY(QDir().rename(folder+"/nested",folder+"/moved"));QVERIFY(QFile::link(outside,folder+"/nested"));}
    else {QVERIFY(QFile::remove(folder+"/note.md"));QVERIFY(QFile::link(outside+"/note.md",folder+"/note.md"));}
   };
   backend.refresh();QVERIFY(!beforeReadOpen);
   for(const auto &note:backend.notes())QVERIFY(note.toMap()["title"].toString()!="Outside secret");
   backend.search("Outside secret");QVERIFY(backend.notes().isEmpty());
  }
 }
 void refreshReadsPinnedDescriptors(){
  QTemporaryDir dir;const auto folder=dir.path()+"/notes",outside=dir.path()+"/outside";
  QVERIFY(QDir().mkpath(folder));QVERIFY(QDir().mkdir(outside));
  write(folder+"/note.md","# Safe note");write(outside+"/note.md","# Outside secret");
  Backend backend(folder,dir.path()+"/session");
  racedReadPath="note.md";
  afterReadOpen=[&] {QVERIFY(QFile::remove(folder+"/note.md"));QVERIFY(QFile::link(outside+"/note.md",folder+"/note.md"));};
  backend.refresh();QVERIFY(!afterReadOpen);QCOMPARE(backend.notes().size(),1);
  QCOMPARE(backend.notes()[0].toMap()["title"].toString(),QString("Safe note"));
  QVERIFY(QFile::remove(folder+"/note.md"));write(folder+"/note.md","# Original root");
  QVERIFY(QDir().rename(folder,dir.path()+"/moved"));QVERIFY(QFile::link(outside,folder));
  backend.refresh();QCOMPARE(backend.notes().size(),1);
  QCOMPARE(backend.notes()[0].toMap()["title"].toString(),QString("Original root"));
 }
 void refreshRejectsSpecialFilesAndUnavailableResolution(){
  QTemporaryDir dir;const auto folder=dir.path()+"/notes";Backend backend(folder,dir.path()+"/session");
  QCOMPARE(::mkfifo(QFile::encodeName(folder+"/pipe.md").constData(),0600),0);
  write(folder+"/safe.md","# Safe");backend.refresh();QCOMPARE(backend.notes().size(),1);
  unavailableRead=true;backend.refresh();unavailableRead=false;
  QVERIFY(backend.notes().isEmpty());QVERIFY(!backend.error().isEmpty());
 }
 void skillInstallNeverReplacesExistingFiles(){
  for(const auto &kind : {QString("file"),QString("symlink"),QString("dangling"),QString("hardlink"),QString("raced")}) {
   QTemporaryDir dir;const auto home=dir.path()+"/codex",target=home+"/skills/quickref-viewer/SKILL.md",outside=dir.path()+"/outside";
   QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));write(outside,"keep existing instructions");
   if(kind=="file")write(target,"keep existing instructions");
   else if(kind=="symlink")QVERIFY(QFile::link(outside,target));
   else if(kind=="dangling")QVERIFY(QFile::link(dir.path()+"/missing",target));
   else if(kind=="hardlink")QCOMPARE(::link(QFile::encodeName(outside).constData(),QFile::encodeName(target).constData()),0);
   else beforePublication=[&] {QVERIFY(QFile::link(outside,target));};
   QString error;QVERIFY(!SafeIo::installSkill(home,error));QVERIFY(!error.isEmpty());QVERIFY(!beforePublication);
   QFile original(outside);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("keep existing instructions"));
   QVERIFY(!QFileInfo::exists(dir.path()+"/missing"));
   if(kind=="file") {QFile existing(target);QVERIFY(existing.open(QIODevice::ReadOnly));QCOMPARE(existing.readAll(),QByteArray("keep existing instructions"));}
  }
 }
 void skillInstallRejectsUnsafeDirectories(){
  for(const auto &component : {QString("codex"),QString("skills"),QString("quickref-viewer")}) {
   QTemporaryDir dir;const auto home=dir.path()+"/codex",outside=dir.path()+"/outside";QVERIFY(QDir().mkdir(outside));
   const auto target=component=="codex" ? home : component=="skills" ? home+"/skills" : home+"/skills/quickref-viewer";
   QVERIFY(QDir().mkpath(QFileInfo(target).absolutePath()));QVERIFY(QFile::link(outside,target));
   QString error;QVERIFY(!SafeIo::installSkill(home,error));QVERIFY(!QFileInfo::exists(outside+"/SKILL.md"));
  }
  QTemporaryDir dir;const auto home=dir.path()+"/codex";QVERIFY(QDir().mkdir(home));
  QCOMPARE(::chmod(QFile::encodeName(home).constData(),0777),0);
  QString error;QVERIFY(!SafeIo::installSkill(home,error));QVERIFY(error.contains("owned by"));
 }
 void skillInstallCreatesCompletePrivateFile(){
  QTemporaryDir dir;const auto home=dir.path()+"/new/codex",target=home+"/skills/quickref-viewer/SKILL.md";
  QString error;QVERIFY2(SafeIo::installSkill(home,error),qPrintable(error));
  QFile installed(target),source(":/agent-skill.md");QVERIFY(installed.open(QIODevice::ReadOnly));QVERIFY(source.open(QIODevice::ReadOnly));
  QCOMPARE(installed.readAll(),source.readAll());
  struct stat metadata{};QCOMPARE(::stat(QFile::encodeName(target).constData(),&metadata),0);QCOMPARE(metadata.st_mode & 0777,mode_t(0600));
  QVERIFY(!SafeIo::installSkill(home,error));
 }
 void englishFallback(){
  QTemporaryDir dir;
  qunsetenv("QUICKREF_LANGUAGE");
  auto russian=dir.path()+"/russian"; write(russian,"ru\n");
  Translator english(russian); QCOMPARE(english.language(), QString("en"));
  QCOMPARE(Translator::readLanguage(dir.path()+"/missing"), QString("en"));
  auto path=dir.path()+"/language"; write(path,"invalid locale\n");
  QCOMPARE(Translator::readLanguage(path), QString("en"));
 }
 void languageAndTheme(){
  QTemporaryDir dir;auto lang=dir.path()+"/language";write(lang,"ru\n");
  qputenv("QUICKREF_LANGUAGE", "system");
  Translator translator(lang);QCOMPARE(translator.translate("Main","Notes folder"),QString::fromUtf8("Папка заметок"));
  write(lang,"da\n");QTRY_COMPARE(translator.language(),QString("da"));QCOMPARE(translator.translate("Main","Notes folder"),QString("Notemappe"));
  QDir(dir.path()).mkdir("theme");write(dir.path()+"/theme/colors.toml","accent = \"#112233\"\n");
  Theme theme(dir.path());QCOMPARE(theme.accent(),QString("#112233"));
  write(dir.path()+"/theme/colors.toml","accent = \"#aabbcc\"\n");QTRY_COMPARE(theme.accent(),QString("#aabbcc"));
 }
 void qmlKeyboardAndScroll(){
  qunsetenv("QUICKREF_LANGUAGE");
  QTemporaryDir dir;auto notes=dir.path()+"/notes";QDir().mkpath(notes);
  write(notes+"/a.md","# Alpha\n"+QByteArray("long content\n\n").repeated(100));write(notes+"/b.md","# Bravo\nUnique query\n");
  Backend b(notes,dir.path()+"/session");Theme theme(dir.path());Translator translator(dir.path()+"/language");
  QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);engine.rootContext()->setContextProperty("translator",&translator);
  QStringList warnings;connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors){for(auto &e:errors)warnings<<e.toString();});
  engine.load(QUrl("qrc:/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
  auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);QTest::qWait(150);
  auto reader=window->findChild<QQuickItem*>("reader");auto search=window->findChild<QQuickItem*>("search");QVERIFY(reader);QVERIFY(search);
  auto toolbar=window->findChild<QQuickItem*>("noteToolbar");
  auto findBar=window->findChild<QQuickItem*>("noteFindBar");
  auto document=window->findChild<QQuickItem*>("documentColumn");
  QVERIFY(toolbar);QVERIFY(findBar);QVERIFY(document);
  // The note must stay below both toolbars, including at the minimum window size.
  for (const QSize size : {QSize(1100,650),QSize(680,340)}) {
   window->resize(size);
   for (bool finding : {false,true}) {
    window->setProperty("findVisible",finding);QTest::qWait(80);
    auto lastBar=finding?findBar:toolbar;
    QVERIFY(reader->mapToScene(QPointF()).y()>=lastBar->mapToScene(QPointF(0,lastBar->height())).y());
    QVERIFY(toolbar->height()>=toolbar->implicitHeight());
    if(finding)QVERIFY(findBar->height()>=findBar->implicitHeight());
    QVERIFY(document->mapToScene(QPointF()).y()>reader->mapToScene(QPointF()).y());
    QVERIFY(reader->height()>0);
   }
  }
  window->setProperty("findVisible",false);window->resize(1100,650);QTest::qWait(80);
  QTest::keyClick(window,Qt::Key_K,Qt::ControlModifier);QTRY_VERIFY(search->hasActiveFocus());
  for(auto c:QString("Bravo")) QTest::keyClick(window,c.toLatin1());QCOMPARE(b.notes().size(),1);QTest::keyClick(window,Qt::Key_Return);QCOMPARE(b.current(),QString("b.md"));
  QTest::keyClick(window,Qt::Key_Tab,Qt::ControlModifier);QCOMPARE(b.current(),QString("a.md"));QTest::qWait(80);
  reader->setProperty("contentY",350.0);QTest::qWait(80);QCOMPARE(b.position(),350.0);
  QTest::keyClick(window,Qt::Key_Tab,Qt::ControlModifier);QCOMPARE(b.current(),QString("b.md"));QTest::qWait(80);
  QTest::keyClick(window,Qt::Key_Tab,Qt::ControlModifier);QTest::qWait(80);QCOMPARE(reader->property("contentY").toDouble(),350.0);
  QTest::keyClick(window,Qt::Key_F,Qt::ControlModifier);
  auto find=window->findChild<QQuickItem*>("noteFind");QVERIFY(find);QTRY_VERIFY(find->hasActiveFocus());
  find->setProperty("text",QString("LONG"));QCOMPARE(window->property("matchIndex").toInt(),0);
  auto count=window->findChild<QObject*>("matchCount");QVERIFY(count);QCOMPARE(count->property("text").toString(),QString("1 / 100"));
  QTest::keyClick(window,Qt::Key_Return);QCOMPARE(window->property("matchIndex").toInt(),1);
  QTest::keyClick(window,Qt::Key_Return,Qt::ShiftModifier);QCOMPARE(window->property("matchIndex").toInt(),0);
  QTest::keyClick(window,Qt::Key_Return,Qt::ShiftModifier);QCOMPARE(window->property("matchIndex").toInt(),99);QVERIFY(reader->property("contentY").toDouble()>350);
  find->setProperty("text",QString("missing"));QCOMPARE(window->property("matchIndex").toInt(),-1);
  QTest::keyClick(window,Qt::Key_Escape);QVERIFY(!window->property("findVisible").toBool());
  QSignalSpy external(&b,&Backend::externalRequested);
  QTest::keyClick(window,Qt::Key_W,Qt::ControlModifier);QCOMPARE(b.tabs().size(),2);QCOMPARE(external.size(),1);QCOMPARE(external[0][0].toString(),notes+"/a.md");
  QTest::keyClick(window,Qt::Key_N,Qt::ControlModifier);
  auto composer=window->findChild<QObject*>("composer");auto noteName=window->findChild<QObject*>("noteName");auto noteBody=window->findChild<QObject*>("noteBody");
  QVERIFY(composer);QVERIFY(noteName);QVERIFY(noteBody);QTRY_VERIFY(composer->property("visible").toBool());
  noteName->setProperty("text",QString("Created"));noteBody->setProperty("text",QString("# New note\n\n```bash\ndocker ps\n```"));
  QTest::keyClick(window,Qt::Key_W,Qt::ControlModifier);QCOMPARE(external.size(),1);QVERIFY(composer->property("visible").toBool());
  QTest::keyClick(window,Qt::Key_S,Qt::ControlModifier);QTRY_VERIFY(!composer->property("visible").toBool());QCOMPARE(b.current(),QString("Created.md"));QVERIFY(QFileInfo::exists(notes+"/Created.md"));
  QTest::keyClick(window,Qt::Key_F,Qt::ControlModifier);QTRY_VERIFY(find->hasActiveFocus());
  find->setProperty("text",QString("docker ps"));QCOMPARE(count->property("text").toString(),QString("1 / 1"));
  QTest::keyClick(window,Qt::Key_Escape);
  QTest::keyClick(window,Qt::Key_N,Qt::ControlModifier);QTRY_VERIFY(composer->property("visible").toBool());
  noteName->setProperty("text",QString("Created"));noteBody->setProperty("text",QString("Do not lose my draft"));
  QTest::keyClick(window,Qt::Key_S,Qt::ControlModifier);QVERIFY(composer->property("visible").toBool());QCOMPARE(noteBody->property("text").toString(),QString("Do not lose my draft"));
  auto discard=window->findChild<QObject*>("discardDialog");QVERIFY(discard);
  QTest::keyClick(window,Qt::Key_Escape);QTRY_VERIFY(discard->property("visible").toBool());QVERIFY(composer->property("visible").toBool());
  QMetaObject::invokeMethod(discard,"reject");QTRY_VERIFY(!discard->property("visible").toBool());
  window->close();QTRY_VERIFY(discard->property("visible").toBool());QVERIFY(window->isVisible());QCOMPARE(noteBody->property("text").toString(),QString("Do not lose my draft"));
  QMetaObject::invokeMethod(discard,"reject");noteName->setProperty("text",QString());noteBody->setProperty("text",QString());QMetaObject::invokeMethod(composer,"close");
  QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
 }
};
QTEST_MAIN(QuickrefTests)
#include "quickref_tests.moc"
