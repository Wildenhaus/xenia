#include "xenia/ui/qt/main_window.h"

#include <QVBoxLayout>

namespace xe {
namespace ui {
namespace qt {

MainWindow::MainWindow() : Themeable<QMainWindow>("MainWindow") {
  // Custom Frame Border
  // Disable for now until windows aero additions are added
  // setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

  shell_ = new XShell(this);
  this->setCentralWidget(shell_);

};

}  // namespace qt
}  // namespace ui
}  // namespace xe