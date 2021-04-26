#include <cmath>

#include "map.hpp"

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

  // auto img = new QImage("qt/widgets/location-night.png");
  // m_map->addImage("label-arrow", img);

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

    auto coordinate = QMapbox::Coordinate(pos.getValue()[0], pos.getValue()[1]);

    if (location.getStatus() == cereal::LiveLocationKalman::Status::VALID){
      m_map->setCoordinate(coordinate);
      m_map->setBearing(RAD2DEG(orientation.getValue()[2]));

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

  m_map->setCoordinateZoom(QMapbox::Coordinate(37.7393118509158, -122.46471285025565), 17);
  // m_map->setStyleUrl("mapbox://styles/mapbox/navigation-night-v1");
  m_map->setStyleUrl("mapbox://styles/pd0wm/cknuhcgvr0vs817o1akcx6pek"); // Larger fonts

  // connect(m_map.data(), SIGNAL(needsRendering()), this, SLOT(update()));
  timer->start(50);
}

void MapWindow::paintGL() {
  m_map->resize(size());
  m_map->setFramebufferObject(defaultFramebufferObject(), size());
  m_map->render();
}
