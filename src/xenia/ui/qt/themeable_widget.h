#ifndef XENIA_UI_QT_THEMEABLEWIDGET_H_
#define XENIA_UI_QT_THEMEABLEWIDGET_H_

#include <QPainter>
#include <QStyleOption>
#include <QWidget>
#include "theme_manager.h"

#include <QtDebug>

namespace xe {
namespace ui {
namespace qt {

template <typename T>
class Themeable : public T {
 public:
  Themeable(QString name, QWidget* parent = nullptr) : T(parent) {
    static_assert(std::is_base_of<QWidget, T>::value,
                  "T is not derived from QWidget");

    ApplyTheme(name);
  }

  void ApplyTheme(const QString& theme_name) {
    if (theme_name != QString::null) {
      setObjectName(theme_name);
    }

    ThemeManager& manager = ThemeManager::SharedManager();
    Theme theme = manager.current_theme();

    QString style = theme.StylesheetForComponent(theme_name);
    QString base_style = manager.base_style();
    if (style != QString::null) {
      setStyleSheet(base_style + style);
    }
  };

  void paintEvent(QPaintEvent*) override {
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
  }
};
}  // namespace qt
}  // namespace ui
}  // namespace xe
#endif