#include <cmath>
#include <eigen3/Eigen/Dense>

#include "map.hpp"
#include "common/util.h"
#include "common/transformations/coordinates.hpp"
#include "common/transformations/orientation.hpp"

#include <QDebug>

#define RAD2DEG(x) ((x) * 180.0 / M_PI)
const int PAN_TIMEOUT = 100;
const bool DRAW_MODEL_PATH = false;
const qreal REROUTE_DISTANCE = 25;
const float METER_2_MILE = 0.000621371;

// TODO: get from param
QMapbox::Coordinate nav_destination(32.71565912901338, -117.16380347622167);

static QGeoCoordinate to_QGeoCoordinate(const QMapbox::Coordinate &in) {
  return QGeoCoordinate(in.first, in.second);
}

static QMapbox::CoordinatesCollections model_to_collection(
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

static QMapbox::CoordinatesCollections coordinate_to_collection(QMapbox::Coordinate c){
  QMapbox::Coordinates coordinates;
  coordinates.push_back(c);

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections collections;
  collections.push_back(collection);
  return collections;
}

static QMapbox::CoordinatesCollections coordinate_list_to_collection(QList<QGeoCoordinate> coordinate_list) {
  QMapbox::Coordinates coordinates;

  for (auto &c : coordinate_list){
    QMapbox::Coordinate coordinate(c.latitude(), c.longitude());
    coordinates.push_back(coordinate);
  }

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

  // Routing
  QVariantMap parameters;
  parameters["mapbox.access_token"] = m_settings.accessToken();

  geoservice_provider = new QGeoServiceProvider("mapbox", parameters);
  routing_manager = geoservice_provider->routingManager();
  if (routing_manager == nullptr){
    qDebug() << geoservice_provider->errorString();
    assert(routing_manager);
  }
  connect(routing_manager, SIGNAL(finished(QGeoRouteReply*)), this, SLOT(routeCalculated(QGeoRouteReply*)));

  grabGesture(Qt::GestureType::PinchGesture);
}

MapWindow::~MapWindow() {
  makeCurrent();
}

void MapWindow::timerUpdate() {
  // This doesn't work from initializeGL
  if (!m_map->layerExists("modelPathLayer")){
    QVariantMap modelPath;
    modelPath["id"] = "modelPathLayer";
    modelPath["type"] = "line";
    modelPath["source"] = "modelPathSource";
    m_map->addLayer(modelPath);
    m_map->setPaintProperty("modelPathLayer", "line-color", QColor("red"));
    m_map->setPaintProperty("modelPathLayer", "line-width", 5.0);
    m_map->setLayoutProperty("modelPathLayer", "line-cap", "round");
  }
  if (!m_map->layerExists("navLayer")){
    QVariantMap nav;
    nav["id"] = "navLayer";
    nav["type"] = "line";
    nav["source"] = "navSource";
    m_map->addLayer(nav, "road-intersection");
    m_map->setPaintProperty("navLayer", "line-color", QColor("#ed5e5e"));
    m_map->setPaintProperty("navLayer", "line-width", 7.5);
    m_map->setLayoutProperty("navLayer", "line-cap", "round");
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
      last_position = coordinate;

      if (shouldRecompute()){
        calculateRoute(nav_destination);
      }

      if (segment.isValid()) {
        auto next_segment = segment.nextRouteSegment();
        if (next_segment.isValid()){
          auto maneuver = next_segment.maneuver();
          if (maneuver.isValid()){
            float maneuver_distance = maneuver.position().distanceTo(to_QGeoCoordinate(last_position));
            emit instructionsChanged(maneuver_distance, maneuver.instructionText());
            if (maneuver_distance < REROUTE_DISTANCE && maneuver_distance > last_maneuver_distance){
              segment = next_segment;
            }
            last_maneuver_distance = maneuver_distance;
          }
        } else {
          qDebug() << "End of route";
        }
      }

      if (pan_counter == 0){
        m_map->setCoordinate(coordinate);
        m_map->setBearing(RAD2DEG(orientation.getValue()[2]));
      } else {
        pan_counter--;
      }

      if (zoom_counter == 0){
        // Scale zoom between 16 and 19 based on speed
        m_map->setZoom(19 - std::min(3.0f, velocity_filter.update(velocity) / 10));
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
      if (DRAW_MODEL_PATH) {
        auto path_points = model_to_collection(location.getCalibratedOrientationECEF(), location.getPositionECEF(), model.getPosition());
        QMapbox::Feature feature2(QMapbox::Feature::LineStringType, path_points, {}, {});
        QVariantMap modelPathSource;
        modelPathSource["type"] = "geojson";
        modelPathSource["data"] = QVariant::fromValue<QMapbox::Feature>(feature2);
        m_map->updateSource("modelPathSource", modelPathSource);
      }
    }
    update();
  }

}

void MapWindow::initializeGL() {
  m_map.reset(new QMapboxGL(nullptr, m_settings, size(), 1));

  // TODO: Get from last gps position param
  m_map->setCoordinateZoom(last_position, 18);
  m_map->setStyleUrl("mapbox://styles/pd0wm/cknuhcgvr0vs817o1akcx6pek"); // Larger fonts

  connect(m_map.data(), SIGNAL(needsRendering()), this, SLOT(update()));
  timer->start(100);
}

void MapWindow::paintGL() {
  m_map->resize(size());
  m_map->setFramebufferObject(defaultFramebufferObject(), size());
  m_map->render();
}


void MapWindow::calculateRoute(QMapbox::Coordinate destination) {
  QGeoRouteRequest request(to_QGeoCoordinate(last_position), to_QGeoCoordinate(destination));
  routing_manager->calculateRoute(request);
}

void MapWindow::routeCalculated(QGeoRouteReply *reply) {
  qDebug() << "route update";
  if (reply->routes().size() != 0) {
    route = reply->routes().at(0);
    segment = route.firstRouteSegment();

    auto route_points = coordinate_list_to_collection(route.path());
    QMapbox::Feature feature(QMapbox::Feature::LineStringType, route_points, {}, {});
    QVariantMap navSource;
    navSource["type"] = "geojson";
    navSource["data"] = QVariant::fromValue<QMapbox::Feature>(feature);
    m_map->updateSource("navSource", navSource);
    has_route = true;
  }

  reply->deleteLater();
}


// TODO: put in helper file
static QGeoCoordinate sub(QGeoCoordinate v, QGeoCoordinate w){
  return QGeoCoordinate(v.latitude() - w.latitude(), v.longitude() - w.longitude());
}

static QGeoCoordinate add(QGeoCoordinate v, QGeoCoordinate w){
  return QGeoCoordinate(v.latitude() + w.latitude(), v.longitude() + w.longitude());
}

static QGeoCoordinate mul(QGeoCoordinate v, float c){
  return QGeoCoordinate(c * v.latitude(), c * v.longitude());
}

static float dot(QGeoCoordinate v, QGeoCoordinate w){
  return v.latitude() * w.latitude() + v.longitude() * w.longitude();

}

static float minimum_distance(QGeoCoordinate a, QGeoCoordinate b, QGeoCoordinate p) {
  const QGeoCoordinate ap = sub(p, a);
  const QGeoCoordinate ab = sub(b, a);
  const float t = std::clamp(dot(ap, ab) / dot(ab, ab), 0.0f, 1.0f);
  const QGeoCoordinate projection = add(a, mul(ab, t));
  return projection.distanceTo(p);
}

bool MapWindow::shouldRecompute(){
  // Recompute based on some heuristics
  // - Destination changed
  // - Distance to current segment
  // - Wrong direcection in segment
  if (!segment.isValid()){
    return true;
  }

  // Compute closest distance to all line segments in the current path
  float min_d = REROUTE_DISTANCE + 1;
  auto path = segment.path();
  auto cur = to_QGeoCoordinate(last_position);
  for (size_t i = 0; i < path.size() - 1; i++){
    auto a = path[i];
    auto b = path[i+1];
    min_d = std::min(min_d, minimum_distance(a, b, cur));
  }
  return min_d > REROUTE_DISTANCE;
}


// Events
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

bool MapWindow::event(QEvent *event) {
  if (event->type() == QEvent::Gesture){
    return gestureEvent(static_cast<QGestureEvent*>(event));
  }

  return QWidget::event(event);
}

bool MapWindow::gestureEvent(QGestureEvent *event) {
  if (QGesture *pinch = event->gesture(Qt::PinchGesture)){
    pinchTriggered(static_cast<QPinchGesture *>(pinch));
  }
  return true;
}

void MapWindow::pinchTriggered(QPinchGesture *gesture) {
  QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
  if (changeFlags & QPinchGesture::ScaleFactorChanged) {
    // TODO: figure out why gesture centerPoint doesn't work
    m_map->scaleBy(gesture->scaleFactor(), {width() / 2.0, height() / 2.0});
    zoom_counter = PAN_TIMEOUT;
  }
}

MapInstructions::MapInstructions(QWidget * parent) : QWidget(parent){

}


void MapInstructions::updateInstructions(float distance, QString text){
  // TODO: Why does the map get messed up if we run this in the initializer
  if (instruction == nullptr){
    setFixedWidth(width());
    QHBoxLayout *layout = new QHBoxLayout;
    instruction = new QLabel;
    instruction->setStyleSheet(R"(font-size: 40px;)");
    layout->addWidget(instruction);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);

    setStyleSheet(R"(
      * {
        color: white;
        background-color: black;
      }
    )");
  }

  QString distance_str;
  distance_str.setNum(distance * METER_2_MILE, 'f', 1);
  distance_str += " miles,\n  ";
  instruction->setText("  In " + distance_str + text);
}
