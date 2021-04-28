#pragma once

#include <QMapboxGL>

#include <QOpenGLWidget>
#include <QPropertyAnimation>
#include <QScopedPointer>
#include <QtGlobal>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGestureEvent>
#include <QTimer>
#include <QGeoRouteRequest>
#include <QGeoServiceProvider>
#include <QGeoRoutingManager>
#include <QGeoCoordinate>

#include "messaging.hpp"

class MapWindow : public QOpenGLWidget
{
    Q_OBJECT

public:
  MapWindow(const QMapboxGLSettings &);
  ~MapWindow();


private:
  void initializeGL() final;
  void paintGL() final;

  QMapboxGLSettings m_settings;
  QScopedPointer<QMapboxGL> m_map;

  void mousePressEvent(QMouseEvent *ev) final;
  void mouseMoveEvent(QMouseEvent *ev) final;
  void wheelEvent(QWheelEvent *ev) final;
  bool event(QEvent *event) final;
  bool gestureEvent(QGestureEvent *event);
  void pinchTriggered(QPinchGesture *gesture);

  bool m_sourceAdded = false;
  SubMaster *sm;
  QTimer* timer;

  // Panning
  QPointF m_lastPos;
  int pan_counter = 0;
  int zoom_counter = 0;

  // Route
  QGeoServiceProvider *geoservice_provider;
  QGeoRoutingManager *routing_manager;
  QGeoRoute route;
  QMapbox::Coordinate last_position = QMapbox::Coordinate(37.7393118509158, -122.46471285025565);
  void calculateRoute(QMapbox::Coordinate destination);
  bool shouldRecompute();


private slots:
  void timerUpdate();
  void routeCalculated(QGeoRouteReply *reply);
};
