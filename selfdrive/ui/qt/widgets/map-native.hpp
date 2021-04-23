#pragma once

#include <QMapboxGL>

#include <QOpenGLWidget>
#include <QPropertyAnimation>
#include <QScopedPointer>
#include <QtGlobal>
#include <QTimer>

#include "messaging.hpp"

class MapWindow : public QOpenGLWidget
{
    Q_OBJECT

public:
  MapWindow(const QMapboxGLSettings &);
  ~MapWindow();


private:
  qreal pixelRatio();

  void initializeGL() final;
  void paintGL() final;
  QPointF m_lastPos;

  QMapboxGLSettings m_settings;
  QScopedPointer<QMapboxGL> m_map;

  QVariant m_symbolAnnotationId;
  QVariant m_lineAnnotationId;
  QVariant m_fillAnnotationId;

  bool m_sourceAdded = false;
  SubMaster *sm;
  QTimer* timer;

private slots:
  void timerUpdate();
};
