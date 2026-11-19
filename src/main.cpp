#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QProcess>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include "safeio.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <cstdio>
#include "backend.h"
#include "theme.h"
#include "translator.h"
int main(int argc,char **argv) {
 bool cli=false;
 if(argc>1)cli=QByteArray(argv[1])=="create"||QByteArray(argv[1])=="skill";
 if(cli){qputenv("QT_QPA_PLATFORM","offscreen");qputenv("QT_QPA_PLATFORMTHEME","basic");}
 QGuiApplication app(argc,argv);
 app.setApplicationName("quickref-viewer"); app.setApplicationVersion("0.1.0");app.setDesktopFileName("quickref-viewer");
 QQuickStyle::setStyle("Material");
 QString folder=QDir::homePath()+"/Notes/quickref-viewer";
 QString state=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/session.json";
 QString screenshot, name, text;int quitAfter=0; bool json=false, stdinText=false, skillInstall=false;
 auto args=app.arguments();
 for(int i=1;i<args.size();++i) {
  if(args[i]=="--notes"&&i+1<args.size())folder=args[++i];
  else if(args[i]=="--state"&&i+1<args.size())state=args[++i];
  else if(args[i]=="--screenshot"&&i+1<args.size())screenshot=args[++i];
  else if(args[i]=="--quit-after"&&i+1<args.size())quitAfter=args[++i].toInt();
  else if(args[i]=="--name"&&i+1<args.size())name=args[++i];
  else if(args[i]=="--text"&&i+1<args.size())text=args[++i];
  else if(args[i]=="--stdin")stdinText=true;
  else if(args[i]=="--json")json=true;
  else if(args[i]=="skill")skillInstall=true;
  else if(args[i]=="install"&&skillInstall){}
  else if(args[i]=="create"){}
  else if(args[i]=="--help") {qInfo("Quickref Viewer — Markdown reference\nUsage: quickref [open] [--notes FOLDER] [--state FILE]\n       quickref create --name NAME [--text TEXT | --stdin] [--json]\n       quickref skill install [--json]");return 0;}
  else if(args[i]!="open"){qCritical("Unknown argument");return 1;}
 }
 auto result=[&](bool ok,const QString &message) {
  if(json) { auto data=QJsonDocument(QJsonObject{{"ok",ok},{"message",message}}).toJson(QJsonDocument::Compact);fprintf(ok?stdout:stderr,"%s\n",data.constData()); }
  else fprintf(ok?stdout:stderr,"%s\n",message.toUtf8().constData());
  return ok?0:1;
 };
 if(skillInstall) {
  auto home=qEnvironmentVariable("CODEX_HOME",QDir::homePath()+"/.codex");
  const auto target=QDir(home).absoluteFilePath("skills/quickref-viewer/SKILL.md");
  QString error;const bool ok=SafeIo::installSkill(home,error);
  return result(ok,ok?target:QString("Cannot install Quickref Viewer skill: ")+error);
 }
 Translator translator;app.installTranslator(&translator);
 Backend backend(folder,state);
 if(cli) {
  if(stdinText){QFile input;if(!input.open(stdin,QIODevice::ReadOnly))return result(false,"Cannot read stdin");text=QString::fromUtf8(input.readAll());}
  bool ok=backend.createNote(name,text);return result(ok,ok?folder+"/"+backend.current():backend.error());
 }
 Theme theme;
 QObject::connect(&backend,&Backend::externalRequested,&backend,[&](const QString &path){
  if(qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
   if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))backend.launchError(QObject::tr("Cannot open editor"));
   return;
  }
  // Lua long strings preserve filenames without shell or Lua interpolation.
  QString delimiter="=";while(path.contains("]"+delimiter+"]"))delimiter+="=";
  QString literal="["+delimiter+"["+path+"]"+delimiter+"]";
  auto process=new QProcess(&backend);
  QObject::connect(process,&QProcess::errorOccurred,&backend,[&,process]{backend.launchError(QObject::tr("Cannot open editor")+": "+process->errorString());process->deleteLater();});
  QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&backend,[&,process](int code,QProcess::ExitStatus status){
   auto output=process->readAllStandardOutput()+process->readAllStandardError();
   if(code!=0||status!=QProcess::NormalExit||output.contains("error:"))backend.launchError(QObject::tr("Cannot open editor")+": "+QString::fromUtf8(output));
   process->deleteLater();
  });
  process->start("hyprctl",{"eval","require(\"hypr.quickref_viewer\").open_external("+literal+")"});
 });
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
