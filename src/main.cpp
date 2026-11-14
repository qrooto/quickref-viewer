#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include "backend.h"
#include "theme.h"
#include "translator.h"
int main(int argc,char **argv) {
 QGuiApplication app(argc,argv);
 app.setApplicationName("quickref"); app.setApplicationVersion("0.1.0");app.setDesktopFileName("quickref");
 QQuickStyle::setStyle("Material");
 QString folder=QDir::homePath()+"/Notes/cheatsheets";
 QString state=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/session.json";
 QString screenshot;int quitAfter=0;
 auto args=app.arguments();
 for(int i=1;i<args.size();++i) {
  if(args[i]=="--notes"&&i+1<args.size())folder=args[++i];
  else if(args[i]=="--state"&&i+1<args.size())state=args[++i];
  else if(args[i]=="--screenshot"&&i+1<args.size())screenshot=args[++i];
  else if(args[i]=="--quit-after"&&i+1<args.size())quitAfter=args[++i].toInt();
  else if(args[i]=="--help") {qInfo("Quickref — Markdown reference\nUsage: quickref [open] [--notes FOLDER] [--state FILE]");return 0;}
  else if(args[i]!="open"){qCritical("Unknown argument");return 1;}
 }
 Translator translator;app.installTranslator(&translator);Theme theme;Backend backend(folder,state);
 QQmlApplicationEngine engine;
 QObject::connect(&translator,&Translator::changed,&engine,&QQmlEngine::retranslate);
 engine.rootContext()->setContextProperty("translator",&translator);
 engine.rootContext()->setContextProperty("theme",&theme);
 engine.rootContext()->setContextProperty("backend",&backend);
 engine.load(QUrl("qrc:/Main.qml"));if(engine.rootObjects().isEmpty())return 1;
 if(!screenshot.isEmpty())QTimer::singleShot(900,&app,[&]{auto w=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!w->grabWindow().save(screenshot))app.exit(1);else app.quit();});
 if(quitAfter>0)QTimer::singleShot(quitAfter,&app,&QCoreApplication::quit);
 return app.exec();
}
