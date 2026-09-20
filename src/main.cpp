#include "geom.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QColor>
#include <QCursor>
#include <QDBusInterface>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegion>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>
#include <QWindow>

#include <cstdio>
#include <limits>

class Sys : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
  Q_PROPERTY(int remaining READ remaining WRITE setRemaining NOTIFY remainingChanged)
  Q_PROPERTY(QString text READ text CONSTANT)
public:
  using QObject::QObject;
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

private:
  bool m_paused = false;
  bool m_notify = false;
  int m_remaining = 0;
  QString m_text;
};

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("cclock");
  app.setDesktopFileName("cclock");

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
  const QCommandLineOption text({"t", "text"},
                                "Text revealed on hover", "text");
  const QCommandLineOption stop({"S", "stop"}, "End the running timer and exit");
  const QCommandLineOption timeGet({"T", "time-get"},
                                   "Print the running timer value");
  const QCommandLineOption textGet({"g", "text-get"},
                                   "Print the running timer text");
  const QCommandLineOption color({"c", "color"}, "Running blob color",
                                 "color", "#0b0b0d");
  const QCommandLineOption pauseColor({"C", "pause-color"},
                                      "Paused blob color", "color", "#ffd60a");
  const QCommandLineOption notify("notify", "Notify when the timer is stopped");
  // No short forms: -p is --picker and -S is --stop.
  const QCommandLineOption pause("pause", "Pause the running timer");
  const QCommandLineOption resume("resume", "Resume the running timer");
  const QCommandLineOption toggle("toggle", "Pause or resume the running timer");
  p.addOptions({picker, seconds, minutes, hours, text, stop, timeGet, textGet,
                color, pauseColor, notify, pause, resume, toggle});
  p.process(app);

  const QColor runColor(p.value(color));
  const QColor pausedColor(p.value(pauseColor));
  if (!runColor.isValid() || runColor.alpha() != 255) {
    std::fprintf(stderr, "cclock: invalid opaque color for --color: %s\n",
                 p.value(color).toLocal8Bit().constData());
    return 2;
  }
  if (!pausedColor.isValid() || pausedColor.alpha() != 255) {
    std::fprintf(stderr, "cclock: invalid opaque color for --pause-color: %s\n",
                 p.value(pauseColor).toLocal8Bit().constData());
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
      {"cfgDarkText", geom::useDarkText(runColor.red(), runColor.green(),
                                        runColor.blue())},
      {"cfgPauseDarkText",
       geom::useDarkText(pausedColor.red(), pausedColor.green(),
                         pausedColor.blue())},
      {"cfgPicker", p.isSet(picker) || t <= 0},
      {"cfgLastMinutes", sys.get("lastMinutes", 90).toInt()},
      {"cfgEdge", sys.get("edge", "top").toString()},
      {"cfgOffset", sys.get("offset", 0.5).toReal()},
      {"cfgScreen", sys.screenAtCursor()},
      {"cfgScreens", sys.screenNames()},
  });
  engine.load(QUrl(QStringLiteral("qrc:/qt/qml/CClock/Main.qml")));
  if (engine.rootObjects().isEmpty())
    return 1;
  return app.exec();
}

#include "main.moc"
