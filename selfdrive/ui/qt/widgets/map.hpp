#pragma once

#include <QGeoCoordinate>
#include <QGeoManeuver>
#include <QGeoRouteRequest>
#include <QGeoRouteSegment>
#include <QGeoRoutingManager>
#include <QGeoServiceProvider>
#include <QGestureEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMapboxGL>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QPropertyAnimation>
#include <QScopedPointer>
#include <QString>
#include <QtGlobal>
#include <QTimer>
#include <QWheelEvent>

#include "messaging.hpp"

class MapWindow : public QOpenGLWidget {
  Q_OBJECT

public:
  MapWindow(const QMapboxGLSettings &);
  ~MapWindow();

private:
  void initializeGL() final;
  void paintGL() final;
  void resizeGL(int w, int h) override;

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
  bool has_route = false;
  QGeoServiceProvider *geoservice_provider;
  QGeoRoutingManager *routing_manager;
  QGeoRoute route;
  QGeoRouteSegment segment;
  QWidget* map_instructions;
  QMapbox::Coordinate last_position = QMapbox::Coordinate(37.7393118509158, -122.46471285025565);
  double last_maneuver_distance = 1000;
  void calculateRoute(QMapbox::Coordinate destination);
  bool shouldRecompute();


private slots:
  void timerUpdate();
  void routeCalculated(QGeoRouteReply *reply);

signals:
  void instructionsChanged(float distance, QString text);
};

class MapInstructions : public QWidget {
  Q_OBJECT

private:
  QLabel *instruction = nullptr;

public:
  MapInstructions(QWidget * parent=nullptr);

public slots:
  void updateInstructions(float distance, QString text);
};
