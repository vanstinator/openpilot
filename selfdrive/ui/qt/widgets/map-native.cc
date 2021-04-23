#include <iostream>

#include "map-native.hpp"

QtMapNative::QtMapNative(QWidget *parent) : QOpenGLWidget(parent) {
  timer = new QTimer(this);
  QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timerUpdate()));
}

void QtMapNative::initializeGL() {
  makeCurrent();
  initializeOpenGLFunctions();
  std::cout << "QtMapNative:" << std::endl;
  std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
  std::cout << "OpenGL vendor: " << glGetString(GL_VENDOR) << std::endl;
  std::cout << "OpenGL renderer: " << glGetString(GL_RENDERER) << std::endl;
  std::cout << "OpenGL language version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

  timer->start(50);
}

QtMapNative::~QtMapNative() {
  makeCurrent();
  doneCurrent();
}

void QtMapNative::timerUpdate() {
    repaint();
}

void QtMapNative::paintGL() {
}

void QtMapNative::resizeGL(int w, int h) {
  std::cout << "resize map " << w << "x" << h << std::endl;
  width = w;
  height = h;
}
