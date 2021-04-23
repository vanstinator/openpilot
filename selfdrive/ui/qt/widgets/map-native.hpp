#pragma once

#include <QTimer>
#include <QFrame>
#include <QQuickItem>
#include <QWidget>
#include <QWidget>
#include <QTimer>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>

#include "messaging.hpp"


class QtMapNative : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  using QOpenGLWidget::QOpenGLWidget;
  explicit QtMapNative(QWidget* parent = 0);
  ~QtMapNative();

private:
  void timerUpdate();
  bool initialized = false;
  SubMaster *sm;
  QTimer* timer;
  int width = 0;
  int height = 0;

  void updatePosition();
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
};
