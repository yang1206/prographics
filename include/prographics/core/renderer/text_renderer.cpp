//
// Created by admin on 2025/2/8.
//

#include "text_renderer.h"

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
    QVector4D clipSpace =
        projectionMatrix * viewMatrix * QVector4D(worldPos, 1.0f);

    if (qAbs(clipSpace.w()) < 0.0001f)
      return QVector2D(-1, -1);

    QVector3D ndc = QVector3D(clipSpace) / clipSpace.w();

    return QVector2D((ndc.x() + 1.0f) * width / 2, (1.0f - ndc.y()) * height / 2);
  }

  void TextRenderer::render(QPainter &painter, const QMatrix4x4 &viewMatrix,
                            const QMatrix4x4 &projectionMatrix, int width,
                            int height) {
    painter.setRenderHint(QPainter::TextAntialiasing);

    for (const auto &label: m_labels) {
      if (!label->visible)
        continue;

      // 设置字体样式
      QFont font(label->style.fontFamily, label->style.fontSize);
      font.setBold(label->style.bold);
      font.setItalic(label->style.italic);
      painter.setFont(font);

      // 设置颜色
      painter.setPen(label->style.color);

      // 计算屏幕位置
      QVector2D screenPos = worldToScreen(label->position, viewMatrix,
                                          projectionMatrix, width, height);
      if (screenPos.x() < 0 || screenPos.y() < 0)
        continue;

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

      // 绘制文本
      painter.drawText(QPointF(screenPos.x(), screenPos.y()), label->text);
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
