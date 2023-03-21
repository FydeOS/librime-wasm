//
// Created by fydeos on 23-3-21.
//

//
// Created by fydeos on 23-3-21.
//

// Copyright 2022 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#ifdef __EMSCRIPTEN__
#include <memory>
#include <utility>

#include "backend.h"
#include "file.h"
#include "support.h"
#include "wasmfs.h"

using namespace wasmfs;
namespace wasmfs_rime {

  class FastIndexedDbBackend;

  class FastIndexedDbFile : public DataFile {
  public:
    FastIndexedDbFile(mode_t mode, backend_t backend, std::string path)
        : DataFile(mode, backend) {}

  private:
    off_t getSize() override {
      return 0;
    }

    int setSize(off_t size) override {
      return 0;
    }

    int open(oflags_t flags) override {
      return 0;
    }

    int close() override {
      return 0;
    }

    ssize_t read(uint8_t* buf, size_t len, off_t offset) override {
      return 0;
    }

    ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
      return 0;
    }

    int flush() override {
      return 0;
    }
  };

  class FastIndexedDbDirectory : public Directory {
  public:

    FastIndexedDbDirectory(mode_t mode, backend_t backend, std::string path)
        : Directory(mode, backend){}

  private:
    std::string getChildPath(const std::string& name) {
      return "";
    }

    std::shared_ptr<File> getChild(const std::string& name) override {
      return nullptr;
    }

    int removeChild(const std::string& name) override {
      return 0;
    }

    std::shared_ptr<DataFile> insertDataFile(const std::string& name,
                                             mode_t mode) override {
      return nullptr;
    }

    std::shared_ptr<Directory> insertDirectory(const std::string& name,
                                               mode_t mode) override {
      return nullptr;
    }

    std::shared_ptr<Symlink> insertSymlink(const std::string& name,
                                           const std::string& target) override {
      // TODO
      abort();
    }

    int insertMove(const std::string& name, std::shared_ptr<File> file) override {
      // TODO
      abort();
    }

    ssize_t getNumEntries() override {
      return 0;
    }

    Directory::MaybeEntries getEntries() override {
      return {};
    }
  };

  class FastIndexedDbBackend : public Backend {
    // The underlying Node FS path of this backend's mount points.
    std::string mountPath;

  public:
    FastIndexedDbBackend(std::string mountPath) : mountPath(std::move(mountPath)) {}

    std::shared_ptr<DataFile> createFile(mode_t mode) override {
      return std::make_shared<FastIndexedDbFile>(mode, this, mountPath);
    }

    std::shared_ptr<Directory> createDirectory(mode_t mode) override {
      return std::make_shared<FastIndexedDbDirectory>(mode, this, mountPath);
    }

    std::shared_ptr<Symlink> createSymlink(std::string target) override {
      // TODO
      abort();
    }
  };

  backend_t my_wasmfs_create_fast_indexeddb_backend(const char* root) {
    return wasmFS.addBackend(std::make_unique<FastIndexedDbBackend>(root));
  }

}
#endif