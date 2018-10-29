#ifndef XENIA_APP_GAME_SCANNER_H_
#define XENIA_APP_GAME_SCANNER_H_

#include "xenia/base/string_util.h"
#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/kernel/util/xex2_info.h"
#include "xenia/vfs/devices/disc_image_device.h"
#include "xenia/vfs/devices/host_path_device.h"
#include "xenia/vfs/devices/stfs_container_device.h"
#include "xenia/vfs/file.h"

namespace xe {
namespace app {
using namespace vfs;
using kernel::util::XdbfGameData;

typedef xe_xex2_header_t XexHeader;
typedef xe_xex2_opt_header_t XexOptHeader;
typedef xe_xex2_game_ratings_t XGameRatings;
typedef xe_xex2_region_flags XGameRegions;
typedef xe_xex2_version_t XGameVersion;

enum XGameFormat {
  kIso,
  kStfs,
  kXex,
};

struct NxeInfo {
  std::string game_title;
  uint8_t* icon;
  size_t icon_size;
  uint8_t* nxe_background_image;    // TODO
  size_t nxe_background_image_size; // TODO
  uint8_t* nxe_slot_image;          // TODO
  size_t nxe_slot_image_size;       // TODO
};

struct XexInfo {
  XexHeader header;
  std::string game_title;
  uint8_t* icon;
  size_t icon_size;
};

struct GameInfo {
  XGameFormat format;
  std::wstring path;
  const NxeInfo* nxe_info = nullptr;
  const XexInfo* xex_info = nullptr;
};

class XGameScanner {
 public:
  static const GameInfo ScanGame(const std::wstring& path);

  static const NxeInfo* ReadNxe(File* file);
  static const XexInfo* ReadXex(File* file);

 private:
  static const XexHeader ReadXexHeader(File* file);
  static X_STATUS ReadXexImage(File* file, XexHeader& header, uint8_t*& image,
                               size_t& image_size);
  static X_STATUS ReadXexResources(File* file, XexInfo* info);
};

}  // namespace app
}  // namespace xe

#endif