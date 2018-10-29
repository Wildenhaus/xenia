
#include "xenia/app/library/game_scanner.h"
#include "third_party/crypto/rijndael-alg-fst.h"
#include "xenia/vfs/devices/stfs_container_entry.h"

namespace xe {
namespace app {

// Helper Functions ///////////////////////////////////////////////////////////

inline const std::wstring GetFileExtension(const std::wstring& path) {
  auto index = path.find_last_of('.');

  if (index < 0) {
    return L"";
  }

  return path.substr(index + 1);
}

inline const std::wstring& GetParentDirectory(const std::wstring& path) {
  auto index = path.find_last_of('/');
  return path.substr(0, index);
}

inline uint8_t* Read(File* file, size_t offset = 0, size_t length = 0) {
  if (!length) {
    length = file->entry()->size();
  }

  uint8_t* data = (uint8_t*)calloc(1, length);
  size_t bytes_read;
  file->ReadSync(data, length, offset, &bytes_read);

  return data;
}

template <typename T>
inline T Read(File* file, size_t offset) {
  uint8_t* data = Read(file, offset, sizeof(T));
  T flipped = xe::load_and_swap<T>(data);
  delete[] data;

  return flipped;
}

Device* CreateDevice(const GameInfo& info) {
  std::string mount_path = "\\SCAN";

  switch (info.format) {
    case XGameFormat::kIso:
      return new vfs::DiscImageDevice(mount_path, info.path);
    default:
      return nullptr;
  }
}

XGameFormat ResolveFormat(const std::wstring& path) {
  const std::wstring& extension = GetFileExtension(path);

  // TODO: Case Insensitivity
  if (extension.compare(L"iso") == 0) return XGameFormat::kIso;
  if (extension.compare(L"xex") == 0) return XGameFormat::kXex;
  // TODO: Stfs Container

  return XGameFormat::kIso;  // TODO
}

// Filesystem Scanning ////////////////////////////////////////////////////////

const GameInfo XGameScanner::ScanGame(const std::wstring& path) {
  GameInfo info;
  info.path = path;
  info.format = ResolveFormat(path);

  auto device = CreateDevice(info);
  if (device == nullptr || !device->Initialize()) {
    throw 0x0;  // TODO
  }

  // Read XEX
  auto xex_entry = device->ResolvePath("default.xex");
  if (xex_entry) {
    File* xex_file = nullptr;
    auto status = xex_entry->Open(FileAccess::kFileReadData, &xex_file);
    if (XSUCCEEDED(status)) {
      info.xex_info = XGameScanner::ReadXex(xex_file);
    }

    xex_file->Destroy();
  }

  // Read NXE
  auto nxe_entry = device->ResolvePath("nxeart");
  if (nxe_entry) {
    File* nxe_file = nullptr;
    auto status = nxe_entry->Open(FileAccess::kFileReadData, &nxe_file);
    if (XSUCCEEDED(status)) {
      info.nxe_info = XGameScanner::ReadNxe(nxe_file);
    }

    nxe_file->Destroy();
  }

  delete device;
  return info;
}

// NXE Scanning ///////////////////////////////////////////////////////////////

const NxeInfo* XGameScanner::ReadNxe(File* file) {
  NxeInfo* info = new NxeInfo();

  // Read Header
  uint8_t* data = Read(file);
  StfsHeader header;
  header.Read(data);

  // Read Title
  std::wstring title(header.title_name);
  info->game_title = std::string(title.begin(), title.end());

  // Read Icon
  info->icon_size = header.title_thumbnail_image_size;
  info->icon = (uint8_t*)calloc(1, info->icon_size);
  memcpy(info->icon, header.title_thumbnail_image, info->icon_size);

  // TODO: Read nxebg.jpg
  // TODO: Read nxeslot.jpg
  //   How can we open the file with a StfsContainerDevice?

  delete[] data;
  return info;
}

// XEX Scanning ///////////////////////////////////////////////////////////////

uint8_t* DecryptXexData(const uint8_t* key, uint8_t* data, size_t length) {
  const uint16_t BLOCK_SIZE = 0x10;

  uint32_t rk[4 * (MAXNR + 1)];
  uint32_t Nr = rijndaelKeySetupDec(rk, key, 128);
  uint8_t iv[0x10] = {0};
  uint8_t* decrypted = (uint8_t*)calloc(length, 1);

  uint8_t* in_cursor = data;
  uint8_t* out_cursor = decrypted;
  for (uint32_t i = 0; i < length; i += BLOCK_SIZE) {
    rijndaelDecrypt(rk, Nr, in_cursor, out_cursor);
    for (size_t j = 0; j < BLOCK_SIZE; j++) {
      out_cursor[j] ^= iv[j];
      iv[j] = in_cursor[j];
    }

    in_cursor += BLOCK_SIZE;
    out_cursor += BLOCK_SIZE;
  }

  return decrypted;
}

const XexInfo* XGameScanner::ReadXex(File* file) {
  XexInfo* info = new XexInfo();
  info->header = ReadXexHeader(file);

  ReadXexResources(file, info);

  return info;
}

XexOptHeader ReadXexAltTitleIds(File* file, XexHeader& header,
                                uint32_t offset) {
  uint32_t length = Read<uint32_t>(file, offset);
  uint32_t count = (length - 0x04) / 0x04;
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_ALTERNATE_TITLE_IDS;
  opt_header.offset = offset;
  opt_header.length = length;

  header.alt_title_id_count = count;
  header.alt_title_ids = (uint32_t*)calloc(length, sizeof(uint32_t));

  uint8_t* cursor = data + 0x04;
  for (uint32_t i = 0; i < length; i++, cursor += 0x04) {
    header.alt_title_ids[i] = xe::load_and_swap<uint32_t>(cursor);
  }

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexExecutionInfo(File* file, XexHeader& header,
                                  uint32_t offset) {
  uint32_t length = sizeof(xe_xex2_execution_info_t);
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_EXECUTION_INFO;
  opt_header.offset = offset;
  opt_header.length = length;

  auto info = &header.execution_info;
  info->media_id = xe::load_and_swap<uint32_t>(data + 0x00);
  info->version.value = xe::load_and_swap<uint32_t>(data + 0x04);
  info->base_version.value = xe::load_and_swap<uint32_t>(data + 0x08);
  info->title_id = xe::load_and_swap<uint32_t>(data + 0x0c);
  info->platform = xe::load_and_swap<uint8_t>(data + 0x10);
  info->executable_table = xe::load_and_swap<uint8_t>(data + 0x11);
  info->disc_number = xe::load_and_swap<uint8_t>(data + 0x12);
  info->disc_count = xe::load_and_swap<uint8_t>(data + 0x13);
  info->savegame_id = xe::load_and_swap<uint8_t>(data + 0x14);

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexFileFormatInfo(File* file, XexHeader& header,
                                   uint32_t offset) {
  uint32_t length = Read<uint32_t>(file, offset);  // TODO
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_FILE_FORMAT_INFO;
  opt_header.offset = offset;
  opt_header.length = length;

  uint8_t* cursor = data;
  auto info = &header.file_format_info;
  info->encryption_type =
      (xe_xex2_encryption_type)xe::load_and_swap<uint16_t>(cursor + 0x04);
  info->compression_type =
      (xe_xex2_compression_type)xe::load_and_swap<uint16_t>(cursor + 0x06);

  switch (info->compression_type) {
    case XEX_COMPRESSION_BASIC: {
      auto compression = &info->compression_info.basic;
      uint32_t region_size = xe::load_and_swap<uint32_t>(cursor + 0x00);
      uint32_t block_count = (region_size - 0x08) / 0x08;
      size_t block_size = sizeof(xe_xex2_file_basic_compression_block_t);

      compression->block_count = block_count;
      compression->blocks = (xe_xex2_file_basic_compression_block_t*)calloc(
          block_count, block_size);

      cursor += 0x08;
      for (uint32_t i = 0; i < block_count; i++) {
        auto block = &compression->blocks[i];
        block->data_size = xe::load_and_swap<uint32_t>(cursor + 0x00);
        block->zero_size = xe::load_and_swap<uint32_t>(cursor + 0x04);
        cursor += 0x08;
      }
    } break;
    case XEX_COMPRESSION_NORMAL: {
      auto compression = &info->compression_info.normal;
      uint32_t window_size = xe::load_and_swap<uint32_t>(cursor + 0x08);
      uint32_t window_bits = 0x00;

      for (int i = 0; i < 32; i++, window_bits++) {
        window_size >>= 1;
        if (window_size == 0x00) {
          break;
        }
      }

      compression->window_size = window_size;
      compression->window_bits = window_bits;
      compression->block_size = xe::load_and_swap<uint32_t>(cursor + 0x0C);
      memcpy(compression->block_hash, cursor + 0x10, 0x14);
    } break;
  }

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexGameRatings(File* file, XexHeader& header,
                                uint32_t offset) {
  uint32_t length = 0xC;
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_GAME_RATINGS;
  opt_header.offset = offset;
  opt_header.length = length;

  auto ratings = &header.game_ratings;
  ratings->esrb =
      (xe_xex2_rating_esrb_value)xe::load_and_swap<uint8_t>(data + 0x00);
  ratings->pegi =
      (xe_xex2_rating_pegi_value)xe::load_and_swap<uint8_t>(data + 0x01);
  ratings->pegifi =
      (xe_xex2_rating_pegi_fi_value)xe::load_and_swap<uint8_t>(data + 0x02);
  ratings->pegipt =
      (xe_xex2_rating_pegi_pt_value)xe::load_and_swap<uint8_t>(data + 0x03);
  ratings->bbfc =
      (xe_xex2_rating_bbfc_value)xe::load_and_swap<uint8_t>(data + 0x04);
  ratings->cero =
      (xe_xex2_rating_cero_value)xe::load_and_swap<uint8_t>(data + 0x05);
  ratings->usk =
      (xe_xex2_rating_usk_value)xe::load_and_swap<uint8_t>(data + 0x06);
  ratings->oflcau =
      (xe_xex2_rating_oflc_au_value)xe::load_and_swap<uint8_t>(data + 0x07);
  ratings->oflcnz =
      (xe_xex2_rating_oflc_nz_value)xe::load_and_swap<uint8_t>(data + 0x08);
  ratings->kmrb =
      (xe_xex2_rating_kmrb_value)xe::load_and_swap<uint8_t>(data + 0x09);
  ratings->brazil =
      (xe_xex2_rating_brazil_value)xe::load_and_swap<uint8_t>(data + 0x0A);
  ratings->fpb =
      (xe_xex2_rating_fpb_value)xe::load_and_swap<uint8_t>(data + 0x0B);

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexImageBaseAddress(File* file, XexHeader& header,
                                     uint32_t value) {
  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_IMAGE_BASE_ADDRESS;
  opt_header.value = value;

  header.exe_address = value;

  return opt_header;
}

XexOptHeader ReadXexMultiDiscMediaIds(File* file, XexHeader& header,
                                      uint32_t offset) {
  uint32_t length = Read<uint32_t>(file, offset);
  uint32_t count = (length - 0x04) / 0x10;
  uint32_t entry_size = sizeof(xe_xex2_multi_disc_media_id_t);
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_MULTIDISC_MEDIA_IDS;
  opt_header.offset = offset;
  opt_header.length = length;

  header.multi_disc_media_id_count = count;
  header.multi_disc_media_ids =
      (xe_xex2_multi_disc_media_id_t*)calloc(count, entry_size);

  uint8_t* cursor = data + 0x04;
  for (uint32_t i = 0; i < count; i++, cursor += 0x10) {
    auto id = &header.multi_disc_media_ids[i];
    memcpy(id->hash, cursor, 0x0C);
    id->media_id = xe::load_and_swap<uint32_t>(cursor + 0x0C);
  }

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexOriginalPeName(File* file, XexHeader& header,
                                   uint32_t offset) {
  uint32_t length = Read<uint32_t>(file, offset);
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_ORIGINAL_PE_NAME;
  opt_header.offset = offset;
  opt_header.length = length;

  uint8_t* cursor = data + 0x04;
  header.original_pe_name = (uint8_t*)calloc(length, 0x01);
  memcpy(header.original_pe_name, cursor, length - 0x04);

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexResourceInfo(File* file, XexHeader& header,
                                 uint32_t offset) {
  uint32_t length = Read<uint32_t>(file, offset);
  uint32_t count = (length - 0x04) / 0x10;
  uint32_t size = sizeof(xe_xex2_resource_info_t);
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_RESOURCE_INFO;
  opt_header.offset = offset;
  opt_header.length = length;

  header.resource_info_count = count;
  header.resource_infos = (xe_xex2_resource_info_t*)calloc(count, size);

  uint8_t* cursor = data + 0x04;
  for (uint32_t i = 0; i < count; i++, cursor += 0x10) {
    auto info = &header.resource_infos[i];
    memcpy(info->name, cursor, 0x08);
    info->address = xe::load_and_swap<uint32_t>(cursor + 0x08);
    info->size = xe::load_and_swap<uint32_t>(cursor + 0x0C);
  }

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexSystemFlags(File* file, XexHeader& header,
                                uint32_t offset) {
  uint32_t length = sizeof(xe_xex2_system_flags);
  uint8_t* data = Read(file, offset, length);

  XexOptHeader opt_header;
  opt_header.key = XEX_HEADER_SYSTEM_FLAGS;
  opt_header.offset = offset;
  opt_header.length = length;

  header.system_flags = (xe_xex2_system_flags)*data;

  delete[] data;
  return opt_header;
}

XexOptHeader ReadXexOptHeader(File* file, XexHeader& header, uint8_t* data) {
  const uint32_t key = xe::load_and_swap<uint32_t>(data + 0x00);
  const uint32_t value = xe::load_and_swap<uint32_t>(data + 0x04);

  switch (key) {
    case XEX_HEADER_ALTERNATE_TITLE_IDS:
      return ReadXexAltTitleIds(file, header, value);
    case XEX_HEADER_EXECUTION_INFO:
      return ReadXexExecutionInfo(file, header, value);
    case XEX_HEADER_FILE_FORMAT_INFO:
      return ReadXexFileFormatInfo(file, header, value);
    case XEX_HEADER_GAME_RATINGS:
      return ReadXexGameRatings(file, header, value);
    case XEX_HEADER_MULTIDISC_MEDIA_IDS:
      return ReadXexMultiDiscMediaIds(file, header, value);
    case XEX_HEADER_RESOURCE_INFO:
      return ReadXexResourceInfo(file, header, value);
    case XEX_HEADER_ORIGINAL_PE_NAME:
      return ReadXexOriginalPeName(file, header, value);
    case XEX_HEADER_IMAGE_BASE_ADDRESS:
      return ReadXexImageBaseAddress(file, header, value);
    case XEX_HEADER_SYSTEM_FLAGS:
      return ReadXexSystemFlags(file, header, value);
  }

  // Return unparsed header if not implemented
  XexOptHeader unknown_opt_header;
  unknown_opt_header.key = key;
  unknown_opt_header.value = value;
  return unknown_opt_header;
}

void ReadXexLoaderInfo(File* file, XexHeader& header) {
  uint32_t length = 0x180;
  uint8_t* data = Read(file, header.certificate_offset, length);

  auto security = &header.loader_info;
  security->header_size = xe::load_and_swap<uint32_t>(data + 0x000);
  security->image_size = xe::load_and_swap<uint32_t>(data + 0x004);
  memcpy(security->rsa_signature, data + 0x008, 0x100);
  security->unklength = xe::load_and_swap<uint32_t>(data + 0x108);
  security->image_flags =
      (xe_xex2_image_flags)xe::load_and_swap<uint32_t>(data + 0x10C);
  security->load_address = xe::load_and_swap<uint32_t>(data + 0x110);
  memcpy(security->section_digest, data + 0x114, 0x14);
  security->import_table_count = xe::load_and_swap<uint32_t>(data + 0x128);
  memcpy(security->import_table_digest, data + 0x12C, 0x14);
  memcpy(security->media_id, data + 0x140, 0x10);
  memcpy(security->file_key, data + 0x150, 0x10);
  security->export_table = xe::load_and_swap<uint32_t>(data + 0x160);
  memcpy(security->header_digest, data + 0x164, 0x14);
  security->game_regions =
      (xe_xex2_region_flags)xe::load_and_swap<uint32_t>(data + 0x178);
  security->media_flags =
      (xe_xex2_media_flags)xe::load_and_swap<uint32_t>(data + 0x17C);

  delete[] data;

  // Decrypt Session Key
  auto Decrypt = [file](XexHeader& header, const uint8_t* key) {
    auto file_key = header.loader_info.file_key;
    uint32_t rk[4 * (MAXNR + 1)];
    uint32_t Nr = rijndaelKeySetupDec(rk, key, 128);
    rijndaelDecrypt(rk, Nr, file_key, header.session_key);

    // Check key validity by searching for PE Magic
    uint8_t enc_buffer[0x10] = {0};
    uint8_t dec_buffer[0x10] = {0};
    uint8_t* block = Read(file, header.exe_offset, 0x10);

    memcpy(enc_buffer, block, 0x10);

    Nr = rijndaelKeySetupDec(rk, header.session_key, 128);
    rijndaelDecrypt(rk, Nr, enc_buffer, dec_buffer);

    const uint16_t PE_MAGIC = 0x4D5A;  // "MZ"
    uint16_t magic = xe::load_and_swap<uint16_t>(dec_buffer);

    return magic == PE_MAGIC;
  };

  if (!Decrypt(header, xe_xex2_retail_key)) {
    if (!Decrypt(header, xe_xex2_devkit_key)) {
      throw 0x0;  // TODO: Decryption didn't work
    }
  }
}

void ReadXexSectionInfo(File* file, XexHeader& header) {
  uint32_t offset = header.certificate_offset;
  uint32_t count = Read<uint32_t>(file, offset);
  uint32_t size = sizeof(xe_xex2_section_t);
  uint32_t length = count * size;
  uint8_t* data = Read(file, offset, length);

  header.section_count = count;
  header.sections = (xe_xex2_section_t*)calloc(count, size);

  if (!header.sections || !count) {
    return;
  }

  uint8_t* cursor = data + 0x04;
  for (uint32_t i = 0; i < count; i++) {
    auto section = &header.sections[i];

    // Determine page size (4kb/64kb)
    uint32_t image_flags = header.loader_info.image_flags;
    uint32_t page_size_4kb_flag = xe_xex2_image_flags::XEX_IMAGE_PAGE_SIZE_4KB;
    bool is_4kb = image_flags & page_size_4kb_flag;
    section->page_size = is_4kb ? 0x1000 : 0x10000;

    section->info.value = xe::load_and_swap<uint32_t>(cursor + 0x00);
    memcpy(section->digest, cursor + 0x04, sizeof(section->digest));
    cursor += 0x04 + sizeof(section->digest);
  }

  delete[] data;
}

const XexHeader XGameScanner::ReadXexHeader(File* file) {
  XexHeader header;
  uint32_t header_size = Read<uint32_t>(file, 0x8);
  uint8_t* data = Read(file, 0x0, header_size);

  // Read Main Header Data
  header.xex2 = xe::load_and_swap<uint32_t>(data + 0x00);
  header.module_flags =
      (xe_xex2_module_flags)xe::load_and_swap<uint32_t>(data + 0x04);
  header.exe_offset = xe::load_and_swap<uint32_t>(data + 0x08);
  header.unknown0 = xe::load_and_swap<uint32_t>(data + 0x0C);
  header.certificate_offset = xe::load_and_swap<uint32_t>(data + 0x10);
  header.header_count = xe::load_and_swap<uint32_t>(data + 0x14);

  // Read Optional Headers
  uint8_t* cursor = data + 0x18;
  for (int i = 0; i < header.header_count; i++, cursor += 0x8) {
    header.headers[i] = ReadXexOptHeader(file, header, cursor);
  }

  ReadXexLoaderInfo(file, header);
  ReadXexSectionInfo(file, header);

  delete[] data;
  return header;
}

X_STATUS ReadXexImageUncompressed(File* file, XexHeader& header,
                                  uint8_t*& image, size_t& image_size) {
  size_t file_size = file->entry()->size();
  auto format = &header.file_format_info;

  const size_t exe_size = file_size - header.exe_offset;

  switch (format->encryption_type) {
    case XEX_ENCRYPTION_NONE: {
      image = Read(file, header.exe_offset, exe_size);
      image_size = exe_size;
      return X_STATUS_SUCCESS;
    }
    case XEX_ENCRYPTION_NORMAL: {
      uint8_t* data = Read(file, header.exe_offset, exe_size);
      image = DecryptXexData(header.session_key, data, exe_size);
      image_size = exe_size;
      delete[] data;
      return X_STATUS_SUCCESS;
    }
    default:
      return X_STATUS_UNSUCCESSFUL;
  }
}

X_STATUS ReadXexImageBasic(File* file, XexHeader& header, uint8_t*& image,
                           size_t& image_size) {
  auto file_size = file->entry()->size();
  auto format = &header.file_format_info;
  auto compression = &format->compression_info.basic;
  auto encryption = format->encryption_type;

  size_t exe_size = file_size - header.exe_offset;

  // Calculate uncompressed size
  uint32_t uncompressed_size = 0;
  for (size_t i = 0; i < compression->block_count; i++) {
    const uint32_t data_size = compression->blocks[i].data_size;
    const uint32_t zero_size = compression->blocks[i].zero_size;
    uncompressed_size += data_size + zero_size;
  }

  uint8_t* data = Read(file, header.exe_offset, exe_size);
  uint8_t* decompressed = (uint8_t*)calloc(1, uncompressed_size);

  uint8_t* cursor_in = data;
  uint8_t* cursor_out = decompressed;
  for (size_t i = 0; i < compression->block_count; i++) {
    const uint32_t data_size = compression->blocks[i].data_size;
    const uint32_t zero_size = compression->blocks[i].zero_size;
    const uint32_t size = data_size + zero_size;

    switch (encryption) {
      case XEX_ENCRYPTION_NONE:
        memcpy(cursor_out, cursor_in, data_size);
        break;
      case XEX_ENCRYPTION_NORMAL:
        uint8_t* decrypted =
            DecryptXexData(header.session_key, cursor_in, data_size);
        memcpy(cursor_out, decrypted, data_size);
        delete[] decrypted;
        cursor_in += data_size;
        cursor_out += data_size + zero_size;
        break;
    }
  }

  image = decompressed;
  image_size = uncompressed_size;
  return X_STATUS_SUCCESS;
}

X_STATUS ReadXexImageNormal(File* file, XexHeader& header, uint8_t*& image,
                            size_t& image_size) {
  // TODO
  return X_STATUS_UNSUCCESSFUL;
}

X_STATUS XGameScanner::ReadXexImage(File* file, XexHeader& header,
                                    uint8_t*& image, size_t& image_size) {
  auto format = &header.file_format_info;

  switch (format->compression_type) {
    case XEX_COMPRESSION_NONE:
      return ReadXexImageUncompressed(file, header, image, image_size);
    case XEX_COMPRESSION_BASIC:
      return ReadXexImageBasic(file, header, image, image_size);
    case XEX_COMPRESSION_NORMAL:
      return ReadXexImageNormal(file, header, image, image_size);
    default:
      return X_STATUS_UNSUCCESSFUL;
  }
}

X_STATUS XGameScanner::ReadXexResources(File* file, XexInfo* info) {
  auto header = info->header;
  auto resources = header.resource_infos;

  for (size_t i = 0; i < header.resource_info_count; i++) {
    auto resource = &resources[i];

    uint32_t title_id = header.execution_info.title_id;
    uint32_t name =
        xe::string_util::from_string<uint32_t>(resource->name, true);

    // Game resources are listed as the TitleID
    if (name == title_id) {
      uint8_t* image = nullptr;
      size_t image_size;

      if (XFAILED(ReadXexImage(file, header, image, image_size))) {
        return X_STATUS_UNSUCCESSFUL;
      }

      auto offset = resource->address - header.exe_address;
      auto data = XdbfGameData(image + offset, resource->size);

      if (!data.is_valid()) {
        delete[] image;
        return X_STATUS_UNSUCCESSFUL;
      }

      // Extract Game Title
      info->game_title = data.title();

      // Extract Game Icon
      auto icon = data.icon();
      info->icon_size = icon.size;
      info->icon = (uint8_t*)calloc(1, icon.size);
      memcpy(info->icon, icon.buffer, icon.size);

      // TODO: Extract Achievements

      delete[] image;
      return X_STATUS_SUCCESS;
    }
  }

  return X_STATUS_SUCCESS;
}

}  // namespace app
}  // namespace xe