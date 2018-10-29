#ifndef XENIA_APP_GAME_ENTRY_H_
#define XENIA_APP_GAME_ENTRY_H_

#include "xenia/app/library/game_scanner.h"
#include "xenia/app/library/types.h"
#include "xenia/kernel/util/xex2_info.h"

#include <map>
#include <vector>

namespace xe {
namespace app {

class XGameEntry final {
 public:
  XGameEntry();
  ~XGameEntry();

  bool is_valid();
  bool is_missing_data();

  const XGameFormat& format() const { return format_; }
  const std::string& file_path() const { return file_path_; }
  const std::string& file_name() const { return file_name_; }

  const std::string& title() const { return title_; }
  const uint8_t* icon() const { return icon_; }
  const uint32_t title_id() const { return title_id_; }
  const uint32_t media_id() const { return media_id_; }
  const std::vector<uint32_t>& alt_title_ids() const { return alt_title_ids_; }
  const std::vector<uint32_t>& alt_media_ids() const { return alt_media_ids_; }
  const std::map<uint8_t, uint32_t>& disc_map() const { return disc_map_; }
  const XGameVersion& version() const { return version_; }
  const XGameVersion& base_version() const { return base_version_; }
  const XGameRatings& ratings() const { return ratings_; }
  const XGameRegions& regions() const { return regions_; }

 private:
  // File Info
  XGameFormat format_;
  std::string file_path_;
  std::string file_name_;

  // Game Metadata
  std::string title_;
  uint8_t icon_[0x4000];
  uint32_t title_id_;
  uint32_t media_id_;
  std::vector<uint32_t> alt_title_ids_;
  std::vector<uint32_t> alt_media_ids_;
  std::map<uint8_t, uint32_t> disc_map_;  // <Disc #, MediaID>
  XGameVersion version_;
  XGameVersion base_version_;
  XGameRatings ratings_;
  XGameRegions regions_;

};

}  // namespace app
}  // namespace xe

#endif