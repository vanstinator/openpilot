#include <cmath>
#include <eigen3/Eigen/Dense>

#include "map.hpp"
#include "common/util.h"
#include "common/transformations/coordinates.hpp"
#include "common/transformations/orientation.hpp"

#include <QDebug>
#include <QString>

#define RAD2DEG(x) ((x) * 180.0 / M_PI)
const int PAN_TIMEOUT = 100;

QMapbox::CoordinatesCollections model_to_collection(
  const cereal::LiveLocationKalman::Measurement::Reader &calibratedOrientationECEF,
  const cereal::LiveLocationKalman::Measurement::Reader &positionECEF,
  const cereal::ModelDataV2::XYZTData::Reader &line){

  Eigen::Vector3d ecef(positionECEF.getValue()[0], positionECEF.getValue()[1], positionECEF.getValue()[2]);
  Eigen::Vector3d orient(calibratedOrientationECEF.getValue()[0], calibratedOrientationECEF.getValue()[1], calibratedOrientationECEF.getValue()[2]);
  Eigen::Matrix3d ecef_from_local = euler2rot(orient).transpose();

  QMapbox::Coordinates coordinates;
  auto x = line.getX();
  auto y = line.getY();
  auto z = line.getZ();
  for (int i = 0; i < x.size(); i++){
    Eigen::Vector3d point_ecef = ecef_from_local * Eigen::Vector3d(x[i], y[i], z[i]) + ecef;
    Geodetic point_geodetic = ecef2geodetic((ECEF){.x = point_ecef[0], .y = point_ecef[1], .z = point_ecef[2]});
    QMapbox::Coordinate coordinate(point_geodetic.lat, point_geodetic.lon);
    coordinates.push_back(coordinate);
  }

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections collections;
  collections.push_back(collection);
  return collections;
}

QMapbox::CoordinatesCollections coordinate_to_collection(QMapbox::Coordinate c){
  QMapbox::Coordinates coordinates;
  coordinates.push_back(c);

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections collections;
  collections.push_back(collection);
  return collections;
}

MapWindow::MapWindow(const QMapboxGLSettings &settings) : m_settings(settings) {
  sm = new SubMaster({"liveLocationKalman", "modelV2"});

  timer = new QTimer(this);
  QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timerUpdate()));
}

MapWindow::~MapWindow() {
  makeCurrent();
}

void MapWindow::timerUpdate() {
  // This doesn't work from initializeGL
  if (!m_map->layerExists("modelPathLayer")){
    m_map->addImage("label-arrow", QImage("../assets/images/triangle.svg"));

    QVariantMap modelPath;
    modelPath["id"] = "modelPathLayer";
    modelPath["type"] = "line";
    modelPath["source"] = "modelPathSource";
    m_map->addLayer(modelPath);
    m_map->setPaintProperty("modelPathLayer", "line-color", QColor("red"));
    m_map->setPaintProperty("modelPathLayer", "line-width", 5.0);
    m_map->setLayoutProperty("modelPathLayer", "line-cap", "round");
  }
  if (!m_map->layerExists("carPosLayer")){
    m_map->addImage("label-arrow", QImage("../assets/images/triangle.svg"));

    QVariantMap carPos;
    carPos["id"] = "carPosLayer";
    carPos["type"] = "symbol";
    carPos["source"] = "carPosSource";
    m_map->addLayer(carPos);
    m_map->setLayoutProperty("carPosLayer", "icon-image", "label-arrow");
    m_map->setLayoutProperty("carPosLayer", "icon-ignore-placement", true);
  }

  sm->update(0);
  if (sm->updated("liveLocationKalman")) {
    auto model = (*sm)["modelV2"].getModelV2();
    auto location = (*sm)["liveLocationKalman"].getLiveLocationKalman();
    auto pos = location.getPositionGeodetic();
    auto orientation = location.getOrientationNED();

    float velocity = location.getVelocityCalibrated().getValue()[0];
    static FirstOrderFilter velocity_filter(velocity, 30, 0.1);

    auto coordinate = QMapbox::Coordinate(pos.getValue()[0], pos.getValue()[1]);

    if (location.getStatus() == cereal::LiveLocationKalman::Status::VALID){

      if (pan_counter == 0){
        m_map->setCoordinate(coordinate);
        m_map->setBearing(RAD2DEG(orientation.getValue()[2]));
      } else {
        pan_counter--;
      }

      if (zoom_counter == 0){
        m_map->setZoom(19 - std::min(3.0f, velocity_filter.update(velocity) / 10)); // Scale zoom between 16 and 19 based on speed
      } else {
        zoom_counter--;
      }

      // Update current location marker
      auto point = coordinate_to_collection(coordinate);
      QMapbox::Feature feature1(QMapbox::Feature::PointType, point, {}, {});
      QVariantMap carPosSource;
      carPosSource["type"] = "geojson";
      carPosSource["data"] = QVariant::fromValue<QMapbox::Feature>(feature1);
      m_map->updateSource("carPosSource", carPosSource);

      // Update model path
      auto path_points = model_to_collection(location.getCalibratedOrientationECEF(), location.getPositionECEF(), model.getPosition());
      QMapbox::Feature feature2(QMapbox::Feature::LineStringType, path_points, {}, {});
      QVariantMap modelPathSource;
      modelPathSource["type"] = "geojson";
      modelPathSource["data"] = QVariant::fromValue<QMapbox::Feature>(feature2);
      m_map->updateSource("modelPathSource", modelPathSource);
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

void MapWindow::mousePressEvent(QMouseEvent *ev) {
  m_lastPos = ev->localPos();
  ev->accept();
}

void MapWindow::mouseMoveEvent(QMouseEvent *ev){
  QPointF delta = ev->localPos() - m_lastPos;

  if (!delta.isNull()) {
    pan_counter = PAN_TIMEOUT;
    m_map->moveBy(delta);
  }

  m_lastPos = ev->localPos();
  ev->accept();
}

void MapWindow::wheelEvent(QWheelEvent *ev) {
  if (ev->orientation() == Qt::Horizontal) {
      return;
  }

  float factor = ev->delta() / 1200.;
  if (ev->delta() < 0) {
      factor = factor > -1 ? factor : 1 / factor;
  }

  m_map->scaleBy(1 + factor, ev->pos());
  zoom_counter = PAN_TIMEOUT;
  ev->accept();
}
