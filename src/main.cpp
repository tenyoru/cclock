#include "geom.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QColor>
#include <QCursor>
#include <QDBusInterface>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegion>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVariantMap>
#include <QWindow>

#include <cstdio>
#include <limits>

struct Config {
  QString runningColor = "#0b0b0d";
  QString pausedColor = "#ffd60a";
  QString overtimeColor = "#d32f2f";
  QString screen;
  QString oled = "none";
  int oledInterval = 60;
  int oledShift = 5;
  int oledTimeout = 10;
  bool keyboardMotion = false;
  int keyboardStep = 20;
  QString beforeStart;
  QString onZero;
  QString onStop;
};

static QString unquote(QString value) {
  value = value.trimmed();
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value = value.mid(1, value.size() - 2);
    value.replace("\\\"", "\"");
    value.replace("\\\\", "\\");
  }
  return value;
}

static Config loadConfig() {
  Config cfg;
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
      "/cclock";
  const QString path = dir + "/config.toml";
  if (!QFile::exists(path)) {
    QDir().mkpath(dir);
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream out(&file);
      out << "running_color = \"#0b0b0d\"\n"
             "paused_color = \"#ffd60a\"\n"
             "overtime_color = \"#d32f2f\"\n"
             "screen = \"\"\n\n"
             "# OLED protection: \"none\", \"all\", or an output name.\n"
             "oled = \"none\"\n"
             "oled_interval = 60\n"
             "oled_shift = 5\n"
             "oled_timeout = 10\n\n"
             "# Vim keys move along the current edge after clicking the timer.\n"
             "keyboard_motion = false\n"
             "keyboard_step = 20\n\n"
             "before_start = \"\"\n"
             "on_zero = \"\"\n"
             "on_stop = \"\"\n";
      file.commit();
    }
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return cfg;
  QTextStream in(&file);
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#'))
      continue;
    const qsizetype equals = line.indexOf('=');
    if (equals < 1)
      continue;
    const QString key = line.left(equals).trimmed();
    const QString value = unquote(line.mid(equals + 1));
    bool ok = false;
    const int number = value.toInt(&ok);
    if (key == "running_color")
      cfg.runningColor = value;
    else if (key == "paused_color")
      cfg.pausedColor = value;
    else if (key == "overtime_color")
      cfg.overtimeColor = value;
    else if (key == "screen")
      cfg.screen = value;
    else if (key == "oled")
      cfg.oled = value;
    else if (key == "oled_interval" && ok && number >= 10)
      cfg.oledInterval = number;
    else if (key == "oled_shift" && ok && number >= 0 && number <= 20)
      cfg.oledShift = number;
    else if (key == "oled_timeout" && ok && number >= 1)
      cfg.oledTimeout = number;
    else if (key == "keyboard_motion")
      cfg.keyboardMotion = value == "true";
    else if (key == "keyboard_step" && ok && number > 0)
      cfg.keyboardStep = number;
    else if (key == "before_start")
      cfg.beforeStart = value;
    else if (key == "on_zero")
      cfg.onZero = value;
    else if (key == "on_stop")
      cfg.onStop = value;
  }
  return cfg;
}

class Sys : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
  Q_PROPERTY(int remaining READ remaining WRITE setRemaining NOTIFY remainingChanged)
  Q_PROPERTY(QString text READ text CONSTANT)
public:
  explicit Sys(QObject *parent = nullptr) : QObject(parent) {
    connect(qApp, &QGuiApplication::screenAdded, this,
            [this] { emit screensChanged(); });
    connect(qApp, &QGuiApplication::screenRemoved, this,
            [this] { QTimer::singleShot(0, this, &Sys::screensChanged); });
  }
  bool paused() const { return m_paused; }
  void setPaused(bool on) {
    if (m_paused == on)
      return;
    m_paused = on;
    emit pausedChanged();
  }
  int remaining() const { return m_remaining; }
  void setRemaining(int seconds) {
    if (m_remaining == seconds)
      return;
    m_remaining = seconds;
    emit remainingChanged();
  }
  QString text() const { return m_text; }
  void setText(const QString &text) { m_text = text; }
  void setNotify(bool on) { m_notify = on; }
  void setCommands(const QString &beforeStart, const QString &onZero,
                   const QString &onStop) {
    m_beforeStart = beforeStart;
    m_onZero = onZero;
    m_onStop = onStop;
  }
  Q_INVOKABLE void timerStarted() {
    if (m_started)
      return;
    m_started = true;
    runCommand(m_beforeStart);
  }
  Q_INVOKABLE void reachedZero() {
    if (m_reachedZero)
      return;
    m_reachedZero = true;
    runCommand(m_onZero);
  }
  Q_INVOKABLE void finish() {
    if (m_finished)
      return;
    m_finished = true;
    notifyStopped();
    runCommand(m_onStop);
  }
  Q_INVOKABLE void notifyStopped() const {
    if (!m_notify)
      return;
    QDBusInterface notifications("org.freedesktop.Notifications",
                                 "/org/freedesktop/Notifications",
                                 "org.freedesktop.Notifications");
    notifications.call(QDBus::NoBlock, "Notify", "cclock", uint(0), QString(),
                       "CClock",
                       "Stopped at " + formatTime(m_remaining, ":"),
                       QStringList(), QVariantMap(), 5000);
  }
  Q_INVOKABLE void beep() const { QApplication::beep(); }
  Q_INVOKABLE QVariant get(const QString &key, const QVariant &def) const {
    return QSettings("cclock", "cclock").value(key, def);
  }
  Q_INVOKABLE void set(const QString &key, const QVariant &value) {
    QSettings("cclock", "cclock").setValue(key, value);
  }
  Q_INVOKABLE QString formatTime(int remaining, const QString &sep) const {
    const char s = sep.isEmpty() ? ':' : sep[0].toLatin1();
    return QString::fromStdString(geom::formatTime(remaining, s));
  }
  QString consoleTime() const {
    return QString::fromStdString(geom::formatTime(m_remaining, ':', false));
  }
  Q_INVOKABLE QVariantMap placeOnRim(double px, double py, int bw, int bh,
                                     int sw, int sh, const QString &edge) const {
    const auto r =
        geom::placeOnRim(px, py, bw, bh, sw, sh, edge.toStdString());
    return {{"x", r.x}, {"y", r.y}, {"edge", QString::fromStdString(r.edge)}};
  }
  Q_INVOKABLE void setInputMask(QObject *obj, int x, int y, int w, int h) const {
    auto *win = qobject_cast<QWindow *>(obj);
    if (!win || w <= 0 || h <= 0)
      return;
    win->setMask(QRegion(x, y, w, h));
  }
  Q_INVOKABLE void clearInputMask(QObject *obj) const {
    if (auto *win = qobject_cast<QWindow *>(obj))
      win->setMask(QRegion());
  }
  Q_INVOKABLE void blockInput(QObject *obj) const {
    auto *win = qobject_cast<QWindow *>(obj);
    if (win)
      win->setMask(QRegion(0, 0, 1, 1));
  }
  Q_INVOKABLE QString screenAtCursor() const {
    const QPoint p = QCursor::pos();
    QScreen *at = QGuiApplication::screenAt(p);
    if (at && (p != QPoint(0, 0) || QGuiApplication::screens().size() == 1))
      return at->name();
    QProcess proc;
    proc.start("niri", {"msg", "--json", "focused-output"});
    if (proc.waitForFinished(300) && proc.exitCode() == 0) {
      const QString name =
          QJsonDocument::fromJson(proc.readAllStandardOutput()).object().value("name").toString();
      if (!name.isEmpty())
        return name;
    }
    if (at)
      return at->name();
    if (QScreen *s = QGuiApplication::primaryScreen())
      return s->name();
    return {};
  }
  Q_INVOKABLE QStringList screenNames() const {
    QStringList names;
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens)
      names << s->name();
    return names;
  }
  Q_INVOKABLE QVariantMap screenGeometry(const QString &name) const {
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens)
      if (s->name() == name) {
        const QRect g = s->geometry();
        return {{"x", g.x()}, {"y", g.y()}, {"w", g.width()}, {"h", g.height()}};
      }
    return {{"x", 0}, {"y", 0}, {"w", 1920}, {"h", 1080}};
  }
  Q_INVOKABLE QString screenAtGlobal(int x, int y) const {
    if (QScreen *s = QGuiApplication::screenAt(QPoint(x, y)))
      return s->name();
    return {};
  }
  Q_INVOKABLE QScreen *screenByName(const QString &name) const {
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens)
      if (s->name() == name) {
        // QScreen has no QObject parent, so QML would take JavaScript
        // ownership and free it on the next GC.
        QQmlEngine::setObjectOwnership(s, QQmlEngine::CppOwnership);
        return s;
      }
    return nullptr;
  }
signals:
  void stopRequested();
  void pausedChanged();
  void remainingChanged();
  void screensChanged();

private:
  static void runCommand(const QString &command) {
    if (!command.isEmpty())
      QProcess::startDetached("/bin/sh", {"-c", command});
  }
  bool m_paused = false;
  bool m_notify = false;
  bool m_started = false;
  bool m_reachedZero = false;
  bool m_finished = false;
  int m_remaining = 0;
  QString m_text;
  QString m_beforeStart;
  QString m_onZero;
  QString m_onStop;
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("cclock");
  app.setApplicationVersion("0.3.0");
  app.setDesktopFileName("cclock");
  app.setQuitOnLastWindowClosed(false);
  app.setWindowIcon(QIcon("qrc:/qt/qml/CClock/cclock.svg"));

  const Config cfg = loadConfig();

  QCommandLineParser p;
  p.setApplicationDescription(
      "Simple countdown timer with overlay window.\n"
      "If no time is provided, a custom time picker is shown.");
  p.addHelpOption();
  p.addVersionOption();
  const QCommandLineOption picker({"p", "picker"},
                                  "Force opening the custom time picker");
  const QCommandLineOption seconds({"s", "seconds"}, "Set countdown seconds",
                                   "num");
  const QCommandLineOption minutes({"m", "minutes"}, "Set countdown minutes",
                                   "num");
  const QCommandLineOption hours({"H", "hours"}, "Set countdown hours", "num");
  const QCommandLineOption text({"t", "text"},
                                "Text revealed on hover", "text");
  const QCommandLineOption stop({"S", "stop"}, "End the running timer and exit");
  const QCommandLineOption timeGet({"T", "time-get"},
                                   "Print the running timer value");
  const QCommandLineOption textGet({"g", "text-get"},
                                   "Print the running timer text");
  const QCommandLineOption color({"c", "running-color", "color"},
                                  "Running blob color (name or #RRGGBB)",
                                  "color", cfg.runningColor);
  const QCommandLineOption pauseColor({"C", "paused-color", "pause-color"},
                                      "Paused blob color (name or #RRGGBB)",
                                      "color", cfg.pausedColor);
  const QCommandLineOption overtimeColor(
      {"O", "overtime-color"}, "Overtime blob color (name or #RRGGBB)",
      "color", cfg.overtimeColor);
  const QCommandLineOption screen(
      "screen", "Prefer this output; fall back while disconnected", "name",
      cfg.screen);
  const QCommandLineOption oled(
      "oled", "OLED protection: none, all, or an output name", "output",
      cfg.oled);
  const QCommandLineOption notify("notify", "Notify when the timer is stopped");
  // No short forms: -p is --picker and -S is --stop.
  const QCommandLineOption pause("pause", "Pause the running timer");
  const QCommandLineOption resume("resume", "Resume the running timer");
  const QCommandLineOption toggle("toggle", "Pause or resume the running timer");
  p.addOptions({picker, seconds, minutes, hours, text, stop, timeGet, textGet,
                 color, pauseColor, overtimeColor, screen, oled, notify, pause,
                 resume, toggle});
  p.process(app);

  const QColor runColor(p.value(color));
  const QColor pausedColor(p.value(pauseColor));
  const QColor overtime(p.value(overtimeColor));
  if (!runColor.isValid() || runColor.alpha() != 255) {
    std::fprintf(stderr,
                 "cclock: invalid opaque color for --running-color: %s\n",
                 p.value(color).toLocal8Bit().constData());
    return 2;
  }
  if (!pausedColor.isValid() || pausedColor.alpha() != 255) {
    std::fprintf(stderr, "cclock: invalid opaque color for --paused-color: %s\n",
                 p.value(pauseColor).toLocal8Bit().constData());
    return 2;
  }
  if (!overtime.isValid() || overtime.alpha() != 255) {
    std::fprintf(stderr,
                 "cclock: invalid opaque color for --overtime-color: %s\n",
                 p.value(overtimeColor).toLocal8Bit().constData());
    return 2;
  }
  if (p.value(oled).isEmpty()) {
    std::fputs("cclock: --oled must be none, all, or an output name\n", stderr);
    return 2;
  }

  const int stateActions = int(p.isSet(pause)) + int(p.isSet(resume)) +
                           int(p.isSet(toggle));
  if ((p.isSet(timeGet) && p.isSet(textGet)) || stateActions > 1 ||
      (stateActions &&
       (p.isSet(stop) || p.isSet(timeGet) || p.isSet(textGet)))) {
    std::fputs("cclock: conflicting control options\n", stderr);
    return 2;
  }

  // An absolute path, because QLocalServer otherwise derives one from TMPDIR,
  // which a compositor keybind need not share with the running timer.
  const QString sockPath =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) +
      "/cclock.sock";

  const auto request = [&sockPath](const char *cmd, bool readReply = false) {
    QLocalSocket sock;
    sock.connectToServer(sockPath);
    if (!sock.waitForConnected(200))
      return 1;
    sock.write(cmd);
    if (!sock.waitForBytesWritten(200))
      return 1;
    if (!readReply)
      return 0;
    if (!sock.waitForReadyRead(200))
      return 1;
    const QByteArray reply = sock.readAll();
    return std::fwrite(reply.constData(), 1, reply.size(), stdout) ==
                   size_t(reply.size())
               ? 0
               : 1;
  };
  if (p.isSet(stop)) {
    const bool get = p.isSet(timeGet) || p.isSet(textGet);
    const char *cmd = p.isSet(timeGet) ? "stop-time"
                      : p.isSet(textGet) ? "stop-text"
                                         : "stop";
    return request(cmd, get);
  }
  if (p.isSet(timeGet))
    return request("time", true);
  if (p.isSet(textGet))
    return request("text", true);
  if (p.isSet(pause))
    return request("pause");
  if (p.isSet(resume))
    return request("resume");
  if (p.isSet(toggle))
    return request("toggle");

  qint64 total = 0;
  const auto addDuration = [&p, &total](const QCommandLineOption &option,
                                        qint64 scale) {
    if (!p.isSet(option))
      return true;
    bool ok = false;
    const qint64 value = p.value(option).toLongLong(&ok);
    if (!ok || value < 0 ||
        value > (std::numeric_limits<int>::max() - total) / scale)
      return false;
    total += value * scale;
    return true;
  };
  if (!addDuration(seconds, 1) || !addDuration(minutes, 60) ||
      !addDuration(hours, 3600)) {
    std::fputs("cclock: duration must be a non-negative integer within range\n",
               stderr);
    return 2;
  }
  const int t = int(total);

  Sys sys;
  sys.setRemaining(t);
  sys.setText(p.value(text));
  sys.setNotify(p.isSet(notify));
  sys.setCommands(cfg.beforeStart, cfg.onZero, cfg.onStop);
  QLocalServer server;
  if (!server.listen(sockPath)) {
    QLocalSocket existing;
    existing.connectToServer(sockPath);
    if (existing.waitForConnected(200)) {
      std::fputs("cclock: a timer is already running\n", stderr);
      return 1;
    }
    QLocalServer::removeServer(sockPath);
    if (!server.listen(sockPath)) {
      std::fprintf(stderr, "cclock: cannot listen on control socket: %s\n",
                   server.errorString().toLocal8Bit().constData());
      return 1;
    }
  }
  QObject::connect(&server, &QLocalServer::newConnection, &sys, [&server, &sys] {
    QLocalSocket *c = server.nextPendingConnection();
    QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
    QObject::connect(c, &QLocalSocket::readyRead, &sys, [c, &sys] {
      const QString cmd = QString::fromUtf8(c->readAll()).trimmed();
      if (cmd == "time" || cmd == "stop-time") {
        c->write((sys.consoleTime() + "\n").toUtf8());
        c->flush();
      } else if (cmd == "text" || cmd == "stop-text") {
        c->write((sys.text() + "\n").toUtf8());
        c->flush();
      }
      if (cmd == "stop" || cmd == "stop-time" || cmd == "stop-text")
        emit sys.stopRequested();
      else if (cmd == "pause")
        sys.setPaused(true);
      else if (cmd == "resume")
        sys.setPaused(false);
      else if (cmd == "toggle")
        sys.setPaused(!sys.paused());
    });
  });

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("sys", &sys);
  engine.setInitialProperties({
      {"cfgColor", runColor},
      {"cfgPauseColor", pausedColor},
      {"cfgOvertimeColor", overtime},
      {"cfgDarkText", geom::useDarkText(runColor.red(), runColor.green(),
                                        runColor.blue())},
      {"cfgPauseDarkText",
       geom::useDarkText(pausedColor.red(), pausedColor.green(),
                          pausedColor.blue())},
      {"cfgPicker", p.isSet(picker) || t <= 0},
      {"cfgLastMinutes", sys.get("lastMinutes", 90).toInt()},
      {"cfgEdge", sys.get("edge", "top").toString()},
      {"cfgOffset", sys.get("offset", 0.5).toReal()},
      {"cfgScreen", p.value(screen).isEmpty()
                        ? sys.get("screen", sys.screenAtCursor()).toString()
                        : p.value(screen)},
      {"cfgOled", p.value(oled)},
      {"cfgOledInterval", cfg.oledInterval},
      {"cfgOledShift", cfg.oledShift},
      {"cfgOledTimeout", cfg.oledTimeout},
      {"cfgKeyboardMotion", cfg.keyboardMotion},
      {"cfgKeyboardStep", cfg.keyboardStep},
  });
  engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CClock/Main.qml")));
  if (engine.rootObjects().isEmpty())
    return 1;
  return app.exec();
}

#include "main.moc"
