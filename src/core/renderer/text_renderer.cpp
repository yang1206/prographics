//
// Created by admin on 2025/2/8.
//

#include "prographics/core/renderer/text_renderer.h"

namespace ProGraphics {
  TextRenderer::Label *TextRenderer::addLabel(const QString &text,
                                              const QVector3D &position,
                                              const TextStyle &style) {
    auto label = std::make_unique<Label>();
    label->text = text;
    label->position = position;
    label->style = style;

    Label *ptr = label.get();
    m_labels.push_back(std::move(label));
    return ptr;
  }

  void TextRenderer::removeLabel(Label *label) {
    auto it =
        std::find_if(m_labels.begin(), m_labels.end(),
                     [label](const auto &ptr) { return ptr.get() == label; });
    if (it != m_labels.end()) {
      m_labels.erase(it);
    }
  }

  void TextRenderer::clear() { m_labels.clear(); }

  QVector2D TextRenderer::worldToScreen(const QVector3D &worldPos,
                                        const QMatrix4x4 &viewMatrix,
                                        const QMatrix4x4 &projectionMatrix,
                                        int width, int height) {
    // 输入参数有效性检查
    if (width <= 0 || height <= 0) {
        return QVector2D(-1, -1); // 返回无效坐标
    }

    QVector4D clipSpace =
        projectionMatrix * viewMatrix * QVector4D(worldPos, 1.0f);

    // 更严格的w分量检查，避免除零和极小值问题
    if (qAbs(clipSpace.w()) < 0.0001f)
        return QVector2D(-1, -1);

    // 执行透视除法
    QVector3D ndc = QVector3D(clipSpace.x() / clipSpace.w(),
                              clipSpace.y() / clipSpace.w(),
                              clipSpace.z() / clipSpace.w());
    
    // 确保NDC在有效范围内，否则返回无效坐标
    if (ndc.x() < -1.0f || ndc.x() > 1.0f || 
        ndc.y() < -1.0f || ndc.y() > 1.0f) {
        return QVector2D(-1, -1);
    }

    // 转换为屏幕坐标
    float screenX = (ndc.x() + 1.0f) * width / 2.0f;
    float screenY = (1.0f - ndc.y()) * height / 2.0f;
    
    return QVector2D(screenX, screenY);
  }

  void TextRenderer::render(QPainter &painter, const QMatrix4x4 &viewMatrix,
                            const QMatrix4x4 &projectionMatrix, int width,
                            int height) {
    // 输入参数有效性检查
    if (width <= 0 || height <= 0 || m_labels.empty()) {
        return;
    }

    painter.setRenderHint(QPainter::TextAntialiasing);

    for (const auto &label: m_labels) {
        if (!label || !label->visible)
            continue;

        // 计算屏幕位置
        QVector2D screenPos = worldToScreen(label->position, viewMatrix,
                                        projectionMatrix, width, height);
        
        // 检查计算得到的屏幕坐标是否有效
        if (screenPos.x() < 0 || screenPos.y() < 0 || 
            screenPos.x() > width || screenPos.y() > height) {
            continue;  // 跳过视口外的标签
        }

        // 设置字体样式
        QFont font(label->style.fontFamily, label->style.fontSize);
        font.setBold(label->style.bold);
        font.setItalic(label->style.italic);
        painter.setFont(font);

        // 设置颜色
        painter.setPen(label->style.color);

        // 计算文本大小
        QFontMetrics fm(painter.font());
        QRect textRect = fm.boundingRect(label->text);

        // 根据对齐方式调整位置
        if (label->alignment & Qt::AlignRight)
            screenPos.setX(screenPos.x() - textRect.width());
        else if (label->alignment & Qt::AlignHCenter)
            screenPos.setX(screenPos.x() - textRect.width() / 2);

        if (label->alignment & Qt::AlignBottom)
            screenPos.setY(screenPos.y() + textRect.height());
        else if (label->alignment & Qt::AlignVCenter)
            screenPos.setY(screenPos.y() + textRect.height() / 2);

        // 应用偏移
        screenPos += QVector2D(label->offsetX, label->offsetY);

        // 确保最终位置在屏幕内
        float finalX = qBound(0.0f, screenPos.x(), static_cast<float>(width));
        float finalY = qBound(0.0f, screenPos.y(), static_cast<float>(height));

        // 绘制文本
        painter.drawText(QPointF(finalX, finalY), label->text);
    }
  }

  void TextRenderer::updateLabel(Label *label, const QString &text) {
    if (label)
      label->text = text;
  }

  void TextRenderer::updateLabel(Label *label, const QVector3D &position) {
    if (label)
      label->position = position;
  }

  void TextRenderer::updateLabel(Label *label, const TextStyle &style) {
    if (label)
      label->style = style;
  }

  void TextRenderer::setAlignment(Label *label, Qt::Alignment alignment) {
    if (label) {
      label->alignment = alignment;
    }
  }
} // namespace ProGraphics
