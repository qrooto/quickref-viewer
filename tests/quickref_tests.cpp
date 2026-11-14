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
  b.search("CONTAINER logs");QCOMPARE(b.notes().size(),1);QCOMPARE(b.notes()[0].toMap()["title"].toString(),QString("Docker"));
  b.search("missing");QCOMPARE(b.notes().size(),0);b.search("");
  write(notes+"/new.md","# New note");QTRY_COMPARE(b.notes().size(),3);
  write(notes+"/a.md","# Changed\nwatcher sees this");b.search("watcher");QTRY_COMPARE(b.notes().size(),1);
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
 void languageAndTheme(){
  QTemporaryDir dir;auto lang=dir.path()+"/language";write(lang,"ru\n");
  Translator translator(lang);QCOMPARE(translator.translate("Main","Notes folder"),QString::fromUtf8("Папка заметок"));
  write(lang,"da\n");QTRY_COMPARE(translator.language(),QString("da"));QCOMPARE(translator.translate("Main","Notes folder"),QString("Notemappe"));
  QDir(dir.path()).mkdir("theme");write(dir.path()+"/theme/colors.toml","accent = \"#112233\"\n");
  Theme theme(dir.path());QCOMPARE(theme.accent(),QString("#112233"));
  write(dir.path()+"/theme/colors.toml","accent = \"#aabbcc\"\n");QTRY_COMPARE(theme.accent(),QString("#aabbcc"));
 }
 void qmlKeyboardAndScroll(){
  QTemporaryDir dir;auto notes=dir.path()+"/notes";QDir().mkpath(notes);
  write(notes+"/a.md","# Alpha\n"+QByteArray("long content\n\n").repeated(100));write(notes+"/b.md","# Bravo\nUnique query\n");
  Backend b(notes,dir.path()+"/session");Theme theme(dir.path());Translator translator(dir.path()+"/language");
  QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("backend",&b);engine.rootContext()->setContextProperty("theme",&theme);engine.rootContext()->setContextProperty("translator",&translator);
  QStringList warnings;connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError> &errors){for(auto &e:errors)warnings<<e.toString();});
  engine.load(QUrl("qrc:/Main.qml"));QVERIFY(!engine.rootObjects().isEmpty());
  auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());QVERIFY(window);QTest::qWait(150);
  auto reader=window->findChild<QQuickItem*>("reader");auto search=window->findChild<QQuickItem*>("search");QVERIFY(reader);QVERIFY(search);
  QTest::keyClick(window,Qt::Key_K,Qt::ControlModifier);QTRY_VERIFY(search->hasActiveFocus());
  for(auto c:QString("Unique")) QTest::keyClick(window,c.toLatin1());QCOMPARE(b.notes().size(),1);QTest::keyClick(window,Qt::Key_Return);QCOMPARE(b.current(),QString("b.md"));
  QTest::keyClick(window,Qt::Key_Tab,Qt::ControlModifier);QCOMPARE(b.current(),QString("a.md"));QTest::qWait(80);
  reader->setProperty("contentY",350.0);QTest::qWait(80);QCOMPARE(b.position(),350.0);
  QTest::keyClick(window,Qt::Key_Tab,Qt::ControlModifier);QCOMPARE(b.current(),QString("b.md"));QTest::qWait(80);
  QTest::keyClick(window,Qt::Key_Tab,Qt::ControlModifier);QTest::qWait(80);QCOMPARE(reader->property("contentY").toDouble(),350.0);
  QTest::keyClick(window,Qt::Key_W,Qt::ControlModifier);QCOMPARE(b.tabs().size(),1);
  QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
 }
};
QTEST_MAIN(QuickrefTests)
#include "quickref_tests.moc"
