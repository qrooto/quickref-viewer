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
#include "theme.h"
#include "translator.h"
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
