//
// Created by Yang1206 on 2025/2/16.
//

#include "TestCoordinate2d.h"
#include <QVBoxLayout>

TestCoordinate2d::TestCoordinate2d(QWidget *parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    m_coordinateSystem = new ProGraphics::Coordinate2D(this);

    layout->addWidget(m_coordinateSystem);
    setLayout(layout);

    setupCoordinateSystem();
}

void TestCoordinate2d::setupCoordinateSystem() {
}
