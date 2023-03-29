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

EM_ASYNC_JS(int, js_getFileSize, (const char *path), {
  try {
    const pathStr = UTF8ToString(path);
    return await Module.fsc.getFileSize(pathStr);
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_setFileSize, (const char* path, uint32_t size), {
  try {
    const pathStr = UTF8ToString(path);
    await Module.fsc.setFileSize(pathStr, size);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_openFile, (const char* path, bool readonly), {
  try {
    const pathStr = UTF8ToString(path);
    return await Module.fsc.openFile(pathStr, readonly);
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_closeFile, (const char* path), {
  try {
    const pathStr = UTF8ToString(path);
    return await Module.fsc.closeFile(pathStr);
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_writeFile, (const char* path, const uint8_t* dat, uint32_t len, uint32_t pos), {
  try {
    const pathStr = UTF8ToString(path);
    const data = HEAPU8.subarray(dat, dat + len);
    return await Module.fsc.writeFile(pathStr, data, pos);
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_readFile, (const char* path, uint8_t* dat, uint32_t len, uint32_t pos), {
  try {
    const pathStr = UTF8ToString(path);
    const data = HEAPU8.subarray(dat, dat + len);
    return await Module.fsc.readFile(pathStr, data, pos);
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(uint32_t, js_getPathDetails, (const char* path), {
  try {
    const pathStr = UTF8ToString(path);
    let curFile = await Module.fsc.readEntry(pathStr, false);
    if (!curFile) {
      return 0;
    } else if (curFile.isDirectory) {
      return 999;
    } else {
      return Math.floor(curFile.mtime / 1000) + 1000;
    }
  } catch (e) {
    out(e);
    return 0;
  }
})

EM_ASYNC_JS(int32_t, js_removeFile, (const char* path), {
  try {
    const pathStr = UTF8ToString(path);
    await Module.fsc.delPath(pathStr);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_moveFile, (const char* old_path, const char* new_path), {
  try {
    const oldStr = UTF8ToString(old_path);
    const newStr = UTF8ToString(new_path);
    await Module.fsc.move(oldStr, newStr);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_createDirectory, (const char* path), {
  try {
    const pathStr = UTF8ToString(path);
    await Module.fsc.createDirectory(pathStr);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_getDirectoryEntriesCount, (const char* path), {
  try {
    const pathStr = UTF8ToString(path);
    return await Module.fsc.getDirectoryEntriesCount(pathStr);
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_readDirectory, (const char* path, std::vector<wasmfs::Directory::Entry> * vec), {
  try {
    const pathStr = UTF8ToString(path);
    let entries = await Module.fsc.readDirectory(path);
    withStackSave(() => {
      entries.files.forEach((entry) => {
        let name = allocateUTF8OnStack(entry.name);
        __wasmfs_node_record_dirent(vec, name, 1);
      });
      entries.directories.forEach((entry) => {
        let name = allocateUTF8OnStack(entry.name);
        __wasmfs_node_record_dirent(vec, name, 2);
      });
    });
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

using namespace wasmfs;
namespace wasmfs_rime {

  class FastIndexedDbBackend;

  class FastIndexedDbFile : public DataFile {
  public:
    std::string path;
    FastIndexedDbFile(mode_t mode, backend_t backend, std::string path, time_t mtime_)
        : DataFile(mode, backend), path(path){
      mtime = mtime_;
    }

  private:

    off_t getSize() override {
      return js_getFileSize(path.c_str());
    }

    int setSize(off_t size) override {
      int ret = js_setFileSize(path.c_str(), size);
      if (ret >= 0) {
        mtime = time(NULL);
      }
      return ret;
    }

    int open(oflags_t flags) override {
      return js_openFile(path.c_str(), flags == O_RDONLY);
    }

    int close() override {
      return js_closeFile(path.c_str());
    }

    ssize_t read(uint8_t* buf, size_t len, off_t offset) override {
      return js_readFile(path.c_str(), buf, len, offset);
    }

    ssize_t write(const uint8_t* buf, size_t len, off_t offset) override {
      if (len > 0) {
        ssize_t ret = js_writeFile(path.c_str(), buf, len, offset);
        if (ret >= 0) {
          mtime = time(NULL);
        }
        return ret;
      }
      return 0;
    }

    int flush() override {
      return 0;
    }
  };

  class FastIndexedDbDirectory : public Directory {
  public:

    std::string path;
    FastIndexedDbDirectory(mode_t mode, backend_t backend, std::string path)
        : Directory(mode, backend), path(path) {}

  private:
    std::string getChildPath(const std::string& name) {
      return path + "/" + name;
    }

    std::shared_ptr<File> getChild(const std::string& name) override {
      auto childPath = getChildPath(name);
      uint32_t details = js_getPathDetails(childPath.c_str());
      if (details == 999) {
        return std::make_shared<FastIndexedDbDirectory>(0777, getBackend(), childPath);
      } else if (details >= 1000) {
        return std::make_shared<FastIndexedDbFile>(0777, getBackend(), childPath, details - 1000);
      }
      // not found or error
      return nullptr;
    }

    int removeChild(const std::string& name) override {
      auto childPath = getChildPath(name);
      return js_removeFile(childPath.c_str());
    }

    std::shared_ptr<DataFile> insertDataFile(const std::string& name,
                                             mode_t mode) override {
      auto childPath = getChildPath(name);
      auto f = std::make_shared<FastIndexedDbFile>(0777, getBackend(), childPath, time(NULL));
      return f;
    }

    std::shared_ptr<Directory> insertDirectory(const std::string& name,
                                               mode_t mode) override {
      auto childPath = getChildPath(name);
      int ret = js_createDirectory(childPath.c_str());
      if (ret == 0) {
        auto f = std::make_shared<FastIndexedDbDirectory>(0777, getBackend(), childPath);
        return f;
      } else {
        return nullptr;
      }
    }

    std::shared_ptr<Symlink> insertSymlink(const std::string& name,
                                           const std::string& target) override {
      // TODO
      abort();
    }

    int insertMove(const std::string& name, std::shared_ptr<File> file) override {
      auto childPath = getChildPath(name);
      auto f = std::dynamic_pointer_cast<FastIndexedDbFile>(file);
      if (f) {
        return js_moveFile(f->path.c_str(), childPath.c_str());
      } else {
        return -22;
      }
    }

    ssize_t getNumEntries() override {
      return js_getDirectoryEntriesCount(path.c_str());
    }

    Directory::MaybeEntries getEntries() override {
      std::vector<Directory::Entry> entries;
      int err = js_readDirectory(path.c_str(), &entries);
      if (err) {
        return {-err};
      }
      return {entries};
    }
  };

  class FastIndexedDbBackend : public Backend {
    // The underlying Node FS path of this backend's mount points.
    std::string mountPath;

  public:
    FastIndexedDbBackend() : mountPath("/root") {}

    std::shared_ptr<DataFile> createFile(mode_t mode) override {
      return std::make_shared<FastIndexedDbFile>(mode, this, mountPath, time(NULL));
    }

    std::shared_ptr<Directory> createDirectory(mode_t mode) override {
      int ret = js_createDirectory(mountPath.c_str());
      if (ret == 0) {
        auto f = std::make_shared<FastIndexedDbDirectory>(0777, this, mountPath);
        return f;
      } else {
        return nullptr;
      }
    }

    std::shared_ptr<Symlink> createSymlink(std::string target) override {
      // TODO
      abort();
    }
  };

  backend_t my_wasmfs_create_fast_indexeddb_backend() {
    return wasmFS.addBackend(std::make_unique<FastIndexedDbBackend>());
  }
}
#endif