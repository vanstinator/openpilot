#include <cassert>
#include <cmath>

#include <QDateTime>
#include <QGeoCoordinate>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickWidget>
#include <QQuickView>
#include <QStackedLayout>
#include <QVariant>

#include "common/params.h"
#include "common/util.h"
#include "map.hpp"

#define RAD2DEG(x) ((x) * 180.0 / M_PI)


QtMap::QtMap(QWidget *parent) : QFrame(parent) {
  QStackedLayout* layout = new QStackedLayout();

  QString mapboxAccessToken = QString::fromStdString(Params().get("MapboxToken"));

  // might have to use QQuickWidget for proper stacking?
  QQuickWidget *map = new QQuickWidget();
  map->rootContext()->setContextProperty("mapboxAccessToken", mapboxAccessToken);
  map->rootContext()->setContextProperty("isRHD", Params().getBool("IsRHD"));
  map->setSource(QUrl::fromLocalFile("qt/widgets/map.qml"));
  mapObject = map->rootObject();
  QSize size = map->size();

  QSizeF scaledSize = mapObject->size() * mapObject->scale();
  qDebug() << "size" << size;
  qDebug() << "scaledSize" << scaledSize;
  qDebug() << "mapObject->scale()" << mapObject->scale();
  map->setFixedSize(scaledSize.toSize());
  setFixedSize(scaledSize.toSize());

  layout->addWidget(map);
  setLayout(layout);

  // Start polling loop
  sm = new SubMaster({"liveLocationKalman"});
  timer.start(100, this); // 10Hz
}

void QtMap::timerEvent(QTimerEvent *event) {
  if (!event)
    return;

  if (event->timerId() == timer.timerId()) {
    if (isVisible())
      updatePosition();
  }
  else
    QObject::timerEvent(event);
}

void QtMap::updatePosition() {
  sm->update(0);
  if (sm->updated("liveLocationKalman")) {
    auto location = (*sm)["liveLocationKalman"].getLiveLocationKalman();
    auto pos = location.getPositionGeodetic();
    auto orientation = location.getOrientationNED();

    QVariantMap gpsLocation;
    gpsLocation["latitude"] = pos.getValue()[0];
    gpsLocation["longitude"] = pos.getValue()[1];
    gpsLocation["altitude"] = pos.getValue()[2];
    gpsLocation["bearingDeg"] = RAD2DEG(orientation.getValue()[2]);

    QQmlProperty::write(mapObject, "gpsLocation", QVariant::fromValue(gpsLocation));
  }
}
