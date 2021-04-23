#include <cmath>

#include "map-native.hpp"

#include <QDebug>
#include <QString>

#define RAD2DEG(x) ((x) * 180.0 / M_PI)

MapWindow::MapWindow(const QMapboxGLSettings &settings) : m_settings(settings) {
  sm = new SubMaster({"liveLocationKalman"});

  qDebug() << "init";
  timer = new QTimer(this);
  QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timerUpdate()));
}

MapWindow::~MapWindow() {
  makeCurrent();
}

void MapWindow::timerUpdate() {
  sm->update(0);
  if (sm->updated("liveLocationKalman")) {
    auto location = (*sm)["liveLocationKalman"].getLiveLocationKalman();
    auto pos = location.getPositionGeodetic();
    auto orientation = location.getOrientationNED();

    if (location.getStatus() == cereal::LiveLocationKalman::Status::VALID){
      m_map->setCoordinate(QMapbox::Coordinate(pos.getValue()[0], pos.getValue()[1]));
      m_map->setBearing(RAD2DEG(orientation.getValue()[2]));
    }
  }
  repaint();
}

qreal MapWindow::pixelRatio() {
    return devicePixelRatioF();
}

void MapWindow::initializeGL() {
  m_map.reset(new QMapboxGL(nullptr, m_settings, size(), pixelRatio()));
  connect(m_map.data(), SIGNAL(needsRendering()), this, SLOT(update()));

  // Set default location to Helsinki.
  m_map->setCoordinateZoom(QMapbox::Coordinate(60.170448, 24.942046), 17);
  m_map->setStyleUrl("mapbox://styles/mapbox/navigation-night-v1");
  timer->start(50);
}

void MapWindow::paintGL() {
  m_map->resize(size());
  m_map->setFramebufferObject(defaultFramebufferObject(), size() * pixelRatio());
  m_map->render();
}
