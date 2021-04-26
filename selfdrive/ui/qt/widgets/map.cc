#include <cmath>

#include "map.hpp"
#include "common/util.h"

#include <QDebug>
#include <QString>

#define RAD2DEG(x) ((x) * 180.0 / M_PI)

QMapbox::CoordinatesCollections coordinate_to_collection(QMapbox::Coordinate c){
  QMapbox::Coordinates coordinates;
  coordinates.push_back(c);

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections point;
  point.push_back(collection);
  return point;
}

MapWindow::MapWindow(const QMapboxGLSettings &settings) : m_settings(settings) {
  sm = new SubMaster({"liveLocationKalman"});

  timer = new QTimer(this);
  QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timerUpdate()));
}

MapWindow::~MapWindow() {
  makeCurrent();
}

void MapWindow::timerUpdate() {
  // This doesn't work from initializeGL
  if (!m_map->layerExists("circleLayer")){
    m_map->addImage("label-arrow", QImage("../assets/images/triangle.svg"));

    QVariantMap circle;
    circle["id"] = "circleLayer";
    circle["type"] = "symbol";
    circle["source"] = "circleSource";
    m_map->addLayer(circle);
    m_map->setLayoutProperty("circleLayer", "icon-image", "label-arrow");
    m_map->setLayoutProperty("circleLayer", "icon-ignore-placement", true);
  }

  sm->update(0);
  if (sm->updated("liveLocationKalman")) {
    auto location = (*sm)["liveLocationKalman"].getLiveLocationKalman();
    auto pos = location.getPositionGeodetic();
    auto orientation = location.getOrientationNED();

    float velocity = location.getVelocityCalibrated().getValue()[0];
    static FirstOrderFilter velocity_filter(velocity, 30, 0.1);

    auto coordinate = QMapbox::Coordinate(pos.getValue()[0], pos.getValue()[1]);

    if (location.getStatus() == cereal::LiveLocationKalman::Status::VALID){
      m_map->setCoordinate(coordinate);
      m_map->setBearing(RAD2DEG(orientation.getValue()[2]));

      // Scale zoom between 16 and 19 based on speed
      m_map->setZoom(19 - std::min(3.0f, velocity_filter.update(velocity) / 10));

      // Update current location marker
      auto point = coordinate_to_collection(coordinate);
      QMapbox::Feature feature(QMapbox::Feature::PointType, point, {}, {});
      QVariantMap circleSource;
      circleSource["type"] = "geojson";
      circleSource["data"] = QVariant::fromValue<QMapbox::Feature>(feature);
      m_map->updateSource("circleSource", circleSource);
    }
    update();
  }

}

void MapWindow::initializeGL() {
  m_map.reset(new QMapboxGL(nullptr, m_settings, size(), 1));

  // Get from last gps position param
  auto coordinate = QMapbox::Coordinate(37.7393118509158, -122.46471285025565);
  m_map->setCoordinateZoom(coordinate, 16);
  m_map->setStyleUrl("mapbox://styles/pd0wm/cknuhcgvr0vs817o1akcx6pek"); // Larger fonts

  connect(m_map.data(), SIGNAL(needsRendering()), this, SLOT(update()));
  timer->start(100);
}

void MapWindow::paintGL() {
  m_map->resize(size());
  m_map->setFramebufferObject(defaultFramebufferObject(), size());
  m_map->render();
}
