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
    // 设置坐标系的实际大小（所有轴统一）
    m_coordinateSystem->setSize(5.0f);

    // 设置各轴的名称和单位
    m_coordinateSystem->setAxisName('x', "Phase", "°");

    m_coordinateSystem->setAxisNameOffset('y', -0.2f, 0.3f);
    m_coordinateSystem->setAxisName('y', "Power", "dBm");

    // 设置各轴的刻度范围
    m_coordinateSystem->setTicksRange('x', 0, 360, 45);
    m_coordinateSystem->setTicksRange('y', -75, -30, 5);

    // m_coordinateSystem->setTicksOffset('x', 1.0f);

    // 设置轴名称的位置和样式
    m_coordinateSystem->setAxisNameLocation('x',
                                            ProGraphics::AxisName::Location::Middle);
    m_coordinateSystem->setAxisNameOffset('x', 0.0f, -0.3f);

    m_coordinateSystem->setAxisNameLocation('y',
                                            ProGraphics::AxisName::Location::End);
    m_coordinateSystem->setAxisNameLocation('z',
                                            ProGraphics::AxisName::Location::End);

    // m_coordinateSystem->setTicksOffset('x', QVector3D(0.4f, 0.8f, 0));
    // m_coordinateSystem->setTicksOffset('y', QVector3D(0.4f, 0.8f, 0));


    // 设置轴名称和刻度的可见性
    m_coordinateSystem->setAxisNameEnabled(true);
    m_coordinateSystem->setTicksEnabled(true);

    // 设置轴线可见性
    m_coordinateSystem->setAxisEnabled(true);
    // m_coordinateSystem->setAxisVisible('x', true);
    // m_coordinateSystem->setAxisVisible('y', true);
    // m_coordinateSystem->setAxisVisible('z', true);


    // 配置网格
    m_coordinateSystem->setGridEnabled(true);
}
