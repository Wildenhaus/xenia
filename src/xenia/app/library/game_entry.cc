#include "xenia/app/library/game_entry.h"

namespace xe {
namespace app {

XGameEntry::XGameEntry() {};
XGameEntry::~XGameEntry() {};

bool XGameEntry::is_valid() {
  // Minimum requirements
  return  file_path_.length() 
    && file_name_.length() 
    && title_id_
    && media_id_;
}

bool XGameEntry::is_missing_data() {
  return title_.length() == 0
    || icon_[0] == 0
    || disc_map_.size() == 0;
  // TODO: Version
  // TODO: Base Version
  // TODO: Ratings
  // TODO: Regions
}

}
}  // namespace xe