#include "TestCoordinate3d.h"
#include <QVBoxLayout>

TestCoordinate3d::TestCoordinate3d(QWidget *parent) : QWidget(parent) {
    auto layout = new QVBoxLayout(this);
    m_coordinateSystem = new ProGraphics::Coordinate3D(this);

    layout->addWidget(m_coordinateSystem);
    setLayout(layout);

    setupCoordinateSystem();
}

void TestCoordinate3d::setupCoordinateSystem() {
    // 设置坐标系的实际大小（所有轴统一）
    m_coordinateSystem->setSize(6.0f);

    // 设置各轴的名称和单位
    m_coordinateSystem->setAxisName('x', "Phase", "°");
    m_coordinateSystem->setAxisName('y', "", "dBm");


    // 设置各轴的刻度范围
    m_coordinateSystem->setTicksRange('x', 0, 360, 90);
    m_coordinateSystem->setTicksRange('y', -75, -30, 10);
    m_coordinateSystem->setTicksRange('z', 0, 5, 2.5f);


    // 设置轴名称的位置和样式
    m_coordinateSystem->setAxisNameLocation('x',
                                            ProGraphics::AxisName::Location::End);
    m_coordinateSystem->setAxisNameLocation('y',
                                            ProGraphics::AxisName::Location::End);
    m_coordinateSystem->setAxisNameLocation('z',
                                            ProGraphics::AxisName::Location::Middle);


    // 设置轴线可见性
    // m_coordinateSystem->setAxisEnabled(true);
    // m_coordinateSystem->setAxisVisible('x', true);
    // m_coordinateSystem->setAxisVisible('y', true);
    // m_coordinateSystem->setAxisVisible('z', true);

    // 配置网格
    m_coordinateSystem->setGridEnabled(true);
    m_coordinateSystem->setGridVisible("xy", true);
    m_coordinateSystem->setGridVisible("xz", true);
    m_coordinateSystem->setGridVisible("yz", false);
}
