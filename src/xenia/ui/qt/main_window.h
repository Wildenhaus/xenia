#ifndef XENIA_UI_QT_MAINWINDOW_H_
#define XENIA_UI_QT_MAINWINDOW_H_

#include "xenia/ui/qt/widgets/shell.h"
#include "xenia/ui/qt/themeable_widget.h"

#include <QMainWindow>

namespace xe {
namespace ui {
namespace qt {

class MainWindow : public Themeable<QMainWindow> {
  Q_OBJECT
 public:
  explicit MainWindow();

  QString window_title() const { return window_title_; }

 private:
  QString window_title_;
  XShell* shell_;
};

}  // namespace qt
}  // namespace ui
}  // namespace xe

#endif