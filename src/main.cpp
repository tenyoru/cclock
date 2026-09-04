#include <LayerShellQt/Shell>
#include <QApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>

class Sys : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  Q_INVOKABLE void beep() const { QApplication::beep(); }
  Q_INVOKABLE int lastMinutes() const {
    return QSettings("cclock", "cclock").value("lastMinutes", 90).toInt();
  }
  Q_INVOKABLE void saveMinutes(int m) {
    QSettings("cclock", "cclock").setValue("lastMinutes", m);
  }
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("cclock");
  app.setDesktopFileName("cclock");
  LayerShellQt::Shell::useLayerShell();

  QCommandLineParser p;
  p.setApplicationDescription(
      "Simple countdown timer with overlay window.\n"
      "If no time is provided, a custom time picker is shown.");
  p.addHelpOption();
  const QCommandLineOption picker({"p", "picker"},
                                  "Force opening the custom time picker");
  const QCommandLineOption seconds({"s", "seconds"}, "Set countdown seconds",
                                   "num");
  const QCommandLineOption minutes({"m", "minutes"}, "Set countdown minutes",
                                   "num");
  const QCommandLineOption hours({"H", "hours"}, "Set countdown hours", "num");
  const QCommandLineOption task({"t", "task"},
                                "Show task label below the timer", "text");
  p.addOptions({picker, seconds, minutes, hours, task});
  p.process(app);

  int t = 0;
  if (p.isSet(seconds))
    t += p.value(seconds).toInt();
  if (p.isSet(minutes))
    t += p.value(minutes).toInt() * 60;
  if (p.isSet(hours))
    t += p.value(hours).toInt() * 3600;

  Sys sys;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("sys", &sys);
  engine.setInitialProperties({
      {"cfgSeconds", t},
      {"cfgTask", p.value(task)},
      {"cfgPicker", p.isSet(picker) || t <= 0},
      {"cfgLastMinutes", sys.lastMinutes()},
  });
  engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CClock/Main.qml")));
  if (engine.rootObjects().isEmpty())
    return 1;
  return app.exec();
}

#include "main.moc"
