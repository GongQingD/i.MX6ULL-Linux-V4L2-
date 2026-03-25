#ifndef PERF_EVENT_H
#define PERF_EVENT_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

struct PerfField {
    QString key;
    QString value;
};

using PerfKv = PerfField;

inline QString buildPerfEventLine(const QString &app,
                                  const QString &event,
                                  const QList<PerfKv> &fields = QList<PerfKv>()) {
    QStringList parts;
    parts << "PERF_EVENT";
    parts << QString("ts_ms=%1").arg(QDateTime::currentMSecsSinceEpoch());
    parts << QString("app=%1").arg(app);
    parts << QString("event=%1").arg(event);

    for (const PerfKv &field : fields) {
        parts << QString("%1=%2").arg(field.key, field.value);
    }

    return parts.join(' ');
}

#endif  // PERF_EVENT_H
