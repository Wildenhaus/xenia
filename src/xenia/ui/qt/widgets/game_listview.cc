
#include "xenia/app/library/game_library.h"
#include "xenia/base/string_util.h"
#include "xenia/ui/qt/delegates/game_listview_delegate.h"
#include "xenia/ui/qt/widgets/game_listview.h"

#include <QHeaderView>
#include <QIcon>

namespace xe {
namespace ui {
namespace qt {
using app::XGameLibrary;

XGameListView::XGameListView(QWidget* parent) : XTableView(parent) {
  model_ = new XGameLibraryModel();

  Build();
  return;
}

void XGameListView::Build() {
  // Properties
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
  setShowGrid(false);

  // Delegates
  this->setItemDelegate(new XGameListViewDelegate);

  setModel(model_);
}

}  // namespace qt
}  // namespace ui
}  // namespace xe