#pragma once
#include "prographics/charts/coordinate/coordinate3d.h"
#include <QWidget>

class TestCoordinate3d : public QWidget {
  Q_OBJECT

public:
  explicit TestCoordinate3d(QWidget *parent = nullptr);

  ~TestCoordinate3d() override = default;

private:
  void setupCoordinateSystem();

private:
  ProGraphics::Coordinate3D *m_coordinateSystem;
};
