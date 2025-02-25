//
// Created by Yang1206 on 2025/2/16.
//

#pragma once
#include "prographics/charts/coordinate/coordinate2d.h"
#include <QWidget>

class TestCoordinate2d : public QWidget {
  Q_OBJECT

public:
  explicit TestCoordinate2d(QWidget *parent = nullptr);

  ~TestCoordinate2d() override = default;

private:
  void setupCoordinateSystem();

private:
  ProGraphics::Coordinate2D *m_coordinateSystem;
};
