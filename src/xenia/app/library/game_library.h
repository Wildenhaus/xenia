#ifndef XENIA_APP_GAME_LIBRARY_H_
#define XENIA_APP_GAME_LIBRARY_H_

#include "xenia/app/library/game_entry.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace xe {
namespace app {

class XGameLibrary {
 public:
  XGameLibrary(XGameLibrary const&) = delete;
  XGameLibrary& operator=(XGameLibrary const&) = delete;
  static std::shared_ptr<XGameLibrary> Instance();

  bool add(const std::string file_path);
  bool add(XGameEntry* game_entry);
  bool remove(const XGameEntry* game_entry);
  bool rescan_game(const XGameEntry* game_entry);

  bool add_path(const std::string& path);
  bool remove_path(const std::string& path);
  bool scan_game_paths();

  bool load();
  bool save();

  const std::shared_ptr<XGameEntry>& game(const uint32_t& title_id) const;
  const std::vector<std::shared_ptr<XGameEntry>>& games() const;

 private:
  XGameLibrary(){};

  std::map<uint32_t, std::shared_ptr<XGameEntry>> games_;
  std::vector<std::string> game_paths_;
};

}  // namespace app
}  // namespace xe

#endif