/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2016 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/kernel/util/xdbf_utils.h"
#include "xenia/base/string.h"

namespace xe {
namespace kernel {
namespace util {

constexpr uint32_t kXdbfMagicXdbf = 'XDBF';

bool XdbfFile::Read(const uint8_t* data, size_t data_size) {
  if (!data || data_size <= sizeof(X_XDBF_HEADER)) {
    return false;
  }

  auto* ptr = data;
  memcpy(&header, ptr, sizeof(X_XDBF_HEADER));
  if (header.magic != kXdbfMagicXdbf) {
    return false;
  }

  ptr += sizeof(X_XDBF_HEADER);

  auto* free_ptr = (const X_XDBF_FILELOC*)(ptr + (sizeof(X_XDBF_ENTRY) *
                                                  header.entry_count));
  auto* data_ptr =
      (uint8_t*)free_ptr + (sizeof(X_XDBF_FILELOC) * header.free_count);

  for (uint32_t i = 0; i < header.entry_used; i++) {
    XdbfEntry entry;
    memcpy(&entry.info, ptr, sizeof(X_XDBF_ENTRY));
    entry.data.resize(entry.info.size);
    memcpy(entry.data.data(), data_ptr + entry.info.offset, entry.info.size);
    entries.push_back(entry);

    ptr += sizeof(X_XDBF_ENTRY);
  }

  for (uint32_t i = 0; i < header.free_used; i++) {
    free_entries.push_back(*free_ptr);
    free_ptr++;
  }

  return true;
}

bool XdbfFile::Write(uint8_t* data, size_t* data_size) {
  *data_size = 0;

  *data_size += sizeof(X_XDBF_HEADER);
  *data_size += entries.size() * sizeof(X_XDBF_ENTRY);
  *data_size += free_entries.size() * sizeof(X_XDBF_FILELOC);

  size_t entries_size = 0;
  for (auto ent : entries) {
    entries_size += ent.data.size();
  }

  *data_size += entries_size;

  if (!data) {
    return true;
  }

  header.entry_count = header.entry_used = (uint32_t)entries.size();
  header.free_count = header.free_used = (uint32_t)free_entries.size();

  auto* ptr = data;
  memcpy(ptr, &header, sizeof(X_XDBF_HEADER));
  ptr += sizeof(X_XDBF_HEADER);

  auto* free_ptr =
      (X_XDBF_FILELOC*)(ptr + (sizeof(X_XDBF_ENTRY) * header.entry_count));
  auto* data_start =
      (uint8_t*)free_ptr + (sizeof(X_XDBF_FILELOC) * header.free_count);

  auto* data_ptr = data_start;
  for (auto ent : entries) {
    ent.info.offset = (uint32_t)(data_ptr - data_start);
    ent.info.size = (uint32_t)ent.data.size();
    memcpy(ptr, &ent.info, sizeof(X_XDBF_ENTRY));

    memcpy(data_ptr, ent.data.data(), ent.data.size());
    data_ptr += ent.data.size();
    ptr += sizeof(X_XDBF_ENTRY);
  }

  for (auto ent : free_entries) {
    memcpy(free_ptr, &ent, sizeof(X_XDBF_FILELOC));
    free_ptr++;
  }

  return true;
}

XdbfEntry* XdbfFile::GetEntry(uint16_t section, uint64_t id) const {
  for (size_t i = 0; i < entries.size(); i++) {
    auto* entry = (XdbfEntry*)&entries[i];
    if (entry->info.section != section || entry->info.id != id) {
      continue;
    }

    return entry;
  }

  return nullptr;
}

bool XdbfFile::UpdateEntry(XdbfEntry entry) {
  for (size_t i = 0; i < entries.size(); i++) {
    auto* ent = (XdbfEntry*)&entries[i];
    if (ent->info.section != entry.info.section ||
        ent->info.id != entry.info.id) {
      continue;
    }

    ent->data = entry.data;
    ent->info.size = (uint32_t)entry.data.size();
    return true;
  }

  XdbfEntry new_entry;
  new_entry.info.section = entry.info.section;
  new_entry.info.id = entry.info.id;
  new_entry.info.size = (uint32_t)entry.data.size();
  new_entry.data = entry.data;

  entries.push_back(new_entry);
  return true;
}

std::string GetStringTableEntry_(const uint8_t* table_start, uint16_t string_id,
                                 uint16_t count) {
  auto* ptr = table_start;
  for (uint16_t i = 0; i < count; ++i) {
    auto entry = reinterpret_cast<const XdbfStringTableEntry*>(ptr);
    ptr += sizeof(XdbfStringTableEntry);
    if (entry->id == string_id) {
      return std::string(reinterpret_cast<const char*>(ptr),
                         entry->string_length);
    }
    ptr += entry->string_length;
  }
  return "";
}

std::string SpaFile::GetStringTableEntry(XdbfLocale locale,
                                         uint16_t string_id) const {
  auto xstr_table =
      GetEntry(static_cast<uint16_t>(XdbfSpaSection::kStringTable),
               static_cast<uint64_t>(locale));
  if (!xstr_table) {
    return "";
  }

  auto xstr_head =
      reinterpret_cast<const X_XDBF_TABLE_HEADER*>(xstr_table->data.data());
  assert_true(xstr_head->magic == static_cast<uint32_t>(XdbfSpaID::Xstr));
  assert_true(xstr_head->version == 1);

  const uint8_t* ptr = xstr_table->data.data() + sizeof(X_XDBF_TABLE_HEADER);

  return GetStringTableEntry_(ptr, string_id, xstr_head->count);
}

uint32_t SpaFile::GetAchievements(
    XdbfLocale locale, std::vector<XdbfAchievement>* achievements) const {
  auto xach_table = GetEntry(static_cast<uint16_t>(XdbfSpaSection::kMetadata),
                             static_cast<uint64_t>(XdbfSpaID::Xach));
  if (!xach_table) {
    return 0;
  }

  auto xach_head =
      reinterpret_cast<const X_XDBF_TABLE_HEADER*>(xach_table->data.data());
  assert_true(xach_head->magic == static_cast<uint32_t>(XdbfSpaID::Xach));
  assert_true(xach_head->version == 1);

  auto xstr_table =
      GetEntry(static_cast<uint16_t>(XdbfSpaSection::kStringTable),
               static_cast<uint64_t>(locale));
  if (!xstr_table) {
    return 0;
  }

  auto xstr_head =
      reinterpret_cast<const X_XDBF_TABLE_HEADER*>(xstr_table->data.data());
  assert_true(xstr_head->magic == static_cast<uint32_t>(XdbfSpaID::Xstr));
  assert_true(xstr_head->version == 1);

  const uint8_t* xstr_ptr =
      xstr_table->data.data() + sizeof(X_XDBF_TABLE_HEADER);

  if (achievements) {
    auto* ach_data =
        reinterpret_cast<const X_XDBF_SPA_ACHIEVEMENT*>(xach_head + 1);
    for (uint32_t i = 0; i < xach_head->count; i++) {
      XdbfAchievement ach;
      ach.id = ach_data->id;
      ach.image_id = ach_data->image_id;
      ach.gamerscore = ach_data->gamerscore;
      ach.flags = ach_data->flags;

      ach.label = xe::to_wstring(
          GetStringTableEntry_(xstr_ptr, ach_data->label_id, xstr_head->count));

      ach.description = xe::to_wstring(GetStringTableEntry_(
          xstr_ptr, ach_data->description_id, xstr_head->count));

      ach.unachieved_desc = xe::to_wstring(GetStringTableEntry_(
          xstr_ptr, ach_data->unachieved_id, xstr_head->count));

      achievements->push_back(ach);
      ach_data++;
    }
  }

  return xach_head->count;
}

XdbfEntry* SpaFile::GetIcon() const {
  return GetEntry(static_cast<uint16_t>(XdbfSpaSection::kImage),
                  static_cast<uint64_t>(XdbfSpaID::Title));
}

XdbfLocale SpaFile::GetDefaultLocale() const {
  auto block = GetEntry(static_cast<uint16_t>(XdbfSpaSection::kMetadata),
                        static_cast<uint64_t>(XdbfSpaID::Xstc));
  if (!block) {
    return XdbfLocale::kEnglish;
  }

  auto xstc = reinterpret_cast<const X_XDBF_XSTC_DATA*>(block->data.data());
  assert_true(xstc->magic == static_cast<uint32_t>(XdbfSpaID::Xstc));

  return static_cast<XdbfLocale>(static_cast<uint32_t>(xstc->default_language));
}

std::string SpaFile::GetTitleName() const {
  return GetStringTableEntry(GetDefaultLocale(),
                             static_cast<uint16_t>(XdbfSpaID::Title));
}

uint32_t SpaFile::GetTitleId() const {
  auto block = GetEntry(static_cast<uint16_t>(XdbfSpaSection::kMetadata),
                        static_cast<uint64_t>(XdbfSpaID::Xthd));
  if (!block) {
    return -1;
  }

  auto xthd = reinterpret_cast<const X_XDBF_XTHD_DATA*>(block->data.data());
  assert_true(xthd->magic == static_cast<uint32_t>(XdbfSpaID::Xthd));

  return xthd->title_id;
}

bool GpdFile::GetAchievement(uint16_t id, XdbfAchievement* dest) {
  for (size_t i = 0; i < entries.size(); i++) {
    auto* entry = (XdbfEntry*)&entries[i];
    if (entry->info.section !=
            static_cast<uint16_t>(XdbfGpdSection::kAchievement) ||
        entry->info.id != id) {
      continue;
    }

    auto* ach_data =
        reinterpret_cast<const X_XDBF_GPD_ACHIEVEMENT*>(entry->data.data());

    dest->ReadGPD(ach_data);
    return true;
  }

  return false;
}

uint32_t GpdFile::GetAchievements(
    std::vector<XdbfAchievement>* achievements) const {
  uint32_t ach_count = 0;

  for (size_t i = 0; i < entries.size(); i++) {
    auto* entry = (XdbfEntry*)&entries[i];
    if (entry->info.section !=
        static_cast<uint16_t>(XdbfGpdSection::kAchievement)) {
      continue;
    }
    if (entry->info.id == 0x100000000 || entry->info.id == 0x200000000) {
      continue;  // achievement sync data, ignore it
    }

    ach_count++;

    if (achievements) {
      auto* ach_data =
          reinterpret_cast<const X_XDBF_GPD_ACHIEVEMENT*>(entry->data.data());

      XdbfAchievement ach;
      ach.ReadGPD(ach_data);

      achievements->push_back(ach);
    }
  }

  return ach_count;
}

bool GpdFile::GetTitle(uint32_t title_id, XdbfTitlePlayed* dest) {
  for (size_t i = 0; i < entries.size(); i++) {
    auto* entry = (XdbfEntry*)&entries[i];
    if (entry->info.section != static_cast<uint16_t>(XdbfGpdSection::kTitle) ||
        entry->info.id != title_id) {
      continue;
    }

    auto* title_data =
        reinterpret_cast<const X_XDBF_GPD_TITLEPLAYED*>(entry->data.data());

    dest->ReadGPD(title_data);

    return true;
  }

  return false;
}

uint32_t GpdFile::GetTitles(std::vector<XdbfTitlePlayed>* titles) const {
  uint32_t title_count = 0;

  for (size_t i = 0; i < entries.size(); i++) {
    auto* entry = (XdbfEntry*)&entries[i];
    if (entry->info.section != static_cast<uint16_t>(XdbfGpdSection::kTitle)) {
      continue;
    }
    if (entry->info.id == 0x100000000 || entry->info.id == 0x200000000) {
      continue;  // achievement sync data, ignore it
    }

    title_count++;

    if (titles) {
      auto* title_data =
          reinterpret_cast<const X_XDBF_GPD_TITLEPLAYED*>(entry->data.data());

      XdbfTitlePlayed title;
      title.ReadGPD(title_data);
      titles->push_back(title);
    }
  }

  return title_count;
}

bool GpdFile::UpdateAchievement(XdbfAchievement ach) {
  XdbfEntry ent;
  ent.info.section = static_cast<uint16_t>(XdbfGpdSection::kAchievement);
  ent.info.id = ach.id;

  // calculate entry size...
  size_t label_len = (ach.label.length() * 2) + 2;
  size_t desc_len = (ach.description.length() * 2) + 2;
  size_t unach_len = (ach.unachieved_desc.length() * 2) + 2;

  size_t est_size = sizeof(X_XDBF_GPD_ACHIEVEMENT);
  est_size += label_len;
  est_size += desc_len;
  est_size += unach_len;

  ent.data.resize(est_size);
  memset(ent.data.data(), 0, est_size);

  // convert XdbfAchievement to GPD achievement
  auto* ach_data = reinterpret_cast<X_XDBF_GPD_ACHIEVEMENT*>(ent.data.data());
  ach_data->id = ach.id;
  ach_data->image_id = ach.image_id;
  ach_data->gamerscore = ach.gamerscore;
  ach_data->flags = ach.flags;
  ach_data->unlock_time = ach.unlock_time;

  auto* label_ptr = reinterpret_cast<uint8_t*>(ent.data.data() +
                                               sizeof(X_XDBF_GPD_ACHIEVEMENT));
  auto* desc_ptr = label_ptr + label_len;
  auto* unach_ptr = desc_ptr + desc_len;

  xe::copy_and_swap<wchar_t>((wchar_t*)label_ptr, ach.label.c_str(),
                             ach.label.size());
  xe::copy_and_swap<wchar_t>((wchar_t*)desc_ptr, ach.description.c_str(),
                             ach.description.size());
  xe::copy_and_swap<wchar_t>((wchar_t*)unach_ptr, ach.unachieved_desc.c_str(),
                             ach.unachieved_desc.size());

  return UpdateEntry(ent);
}

bool GpdFile::UpdateTitle(XdbfTitlePlayed title) {
  XdbfEntry ent;
  ent.info.section = static_cast<uint16_t>(XdbfGpdSection::kTitle);
  ent.info.id = title.title_id;

  // calculate entry size...
  size_t name_len = (title.title_name.length() * 2) + 2;

  size_t est_size = sizeof(X_XDBF_GPD_TITLEPLAYED);
  est_size += name_len;

  ent.data.resize(est_size);
  memset(ent.data.data(), 0, est_size);

  // convert XdbfTitlePlayed to GPD title
  auto* title_data = reinterpret_cast<X_XDBF_GPD_TITLEPLAYED*>(ent.data.data());
  title.WriteGPD(title_data);

  return UpdateEntry(ent);
}

}  // namespace util
}  // namespace kernel
}  // namespace xe
