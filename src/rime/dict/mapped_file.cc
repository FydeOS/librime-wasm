//
// Copyright RIME Developers
// Distributed under the BSD License
//
// register components
//
// 2011-06-30 GONG Chen <chen.sst@gmail.com>
//
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <rime/dict/mapped_file.h>

#ifdef BOOST_RESIZE_FILE

#define RESIZE_FILE boost::filesystem::resize_file

#else

#ifdef _WIN32
#include <windows.h>
#define RESIZE_FILE(P,SZ) (resize_file_api(P, SZ) != 0)
static BOOL resize_file_api(const char* p, boost::uintmax_t size) {
  HANDLE handle = CreateFileA(p, GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, 0);
  LARGE_INTEGER sz;
  sz.QuadPart = size;
  return handle != INVALID_HANDLE_VALUE
    && ::SetFilePointerEx(handle, sz, 0, FILE_BEGIN)
    && ::SetEndOfFile(handle)
    && ::CloseHandle(handle);
}
#else
#include <unistd.h>
#define RESIZE_FILE(P,SZ) (::truncate(P, SZ) == 0)
#endif  // _WIN32

#endif  // BOOST_RESIZE_FILE

namespace rime {

MappedFile::MappedFile(const string& file_name)
    : file_name_(file_name) {
}

bool MappedFile::Create(size_t capacity) {
  Close();
  read_write_ = true;
  size_ = 0;
  buffer_ = static_cast<uint8_t *>(malloc(capacity));
  if (buffer_) {
    buffer_size_ = capacity;
  }
  return bool(buffer_);
}

MappedFile::~MappedFile() {
  if (buffer_) {
    free(buffer_);
    buffer_ = nullptr;
  }
}

bool MappedFile::OpenReadOnly() {
  if (!Exists()) {
    LOG(ERROR) << "attempt to open non-existent file '" << file_name_ << "'.";
    return false;
  }
  Close();
  std::size_t s = boost::filesystem::file_size(file_name_);
  buffer_ = static_cast<uint8_t *>(malloc(s));
  if (buffer_) {
    buffer_size_ = size_ = s;
    read_write_ = false;
    FILE* f = fopen(file_name_.c_str(), "r");
    if (f) {
      fread(buffer_, 1, size_, f);
      fclose(f);
      return true;
    } else {
      return false;
    }
  }
  return bool(buffer_);
}

void MappedFile::Close() {
  if (buffer_) {
    Flush();
    free(buffer_);
    buffer_ = nullptr;
    size_ = 0;
    buffer_size_ = 0;
  }
}

bool MappedFile::Exists() const {
  return boost::filesystem::exists(file_name_);
}

bool MappedFile::IsOpen() const {
  return bool(buffer_);
}

bool MappedFile::Flush() {
  if (buffer_) {
    if (!read_write_)
      return true;
    // write buffer to disk
    FILE* f = fopen(file_name_.c_str(), "w");
    if (f) {
      fwrite(buffer_, 1, size_, f);
      fclose(f);
      return true;
    } else {
      return false;
    }
  }
  return false;
}

bool MappedFile::ShrinkToFit() {
  LOG(INFO) << "shrinking file to fit data size. capacity: " << capacity() << ", actual size: " << size_;
  // shrink buffer to current size
  buffer_ = static_cast<uint8_t *>(realloc(buffer_, size_));
  return Flush();
}

bool MappedFile::Remove() {
  if (IsOpen())
    Close();
  return boost::interprocess::file_mapping::remove(file_name_.c_str());
}

String* MappedFile::CreateString(const string& str) {
  String* ret = Allocate<String>();
  if (ret && !str.empty()) {
    CopyString(str, ret);
  }
  return ret;
}

bool MappedFile::CopyString(const string& src, String* dest) {
  if (!dest)
    return false;
  size_t size = src.length() + 1;
  char* ptr = Allocate<char>(size);
  if (!ptr)
    return false;
  std::strncpy(ptr, src.c_str(), size);
  dest->data = ptr;
  return true;
}

size_t MappedFile::capacity() const {
  return buffer_size_;
}

char* MappedFile::address() const {
  return reinterpret_cast<char*>(buffer_);
}

}  // namespace rime
