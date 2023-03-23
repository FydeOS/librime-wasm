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
    path = UTF8ToString(path);
    const details = await Module.fs.readFile(path);
    return details.blobs.map(b => b.size).reduce((partialSum, a) => partialSum + a, 0);
  } catch (e) {
    out(e);
    return -5;
  }
})

// { blobs: [ { id: 12345, size: 10 } ], mtime: xxx, mode: 0777 }
EM_ASYNC_JS(int, js_setFileSize, (const char* path, uint32_t size), {
  try {
    path = UTF8ToString(path);
    let curFile;
    if (await Module.fs.exists(path)) {
      curFile = await Module.fs.readFile(path);
    } else {
      curFile = { blobs: [], mtime: 0, mode: 0o777 };
    }
    const curBlobs = curFile.blobs;
    let newBlobs = [];
    let seek = 0;
    let i;
    for (i = 0; i < curBlobs.length; i++) {
      let curBlob = curBlobs[i];
      if (size >= seek + curBlob.size) {
        // not arrived yet
        newBlobs.push(curBlob);
      } else {
        const preserve = size - seek;
        if (preserve > 0) {
          const cutBlobData = await Module.readBlob(curBlob).subarray(0, preserve);
          const cutBlob = await Module.createBlob(cutBlobData);
          newBlobs.push(cutBlob);
        }
        break;
      }
      seek += curBlob.size;
    }

    if (seek < size) {
      let stubData = new ArrayBuffer(size - seek);
      const stubBlob = await Module.createBlob(stubData);
      newBlobs.push(stubBlob);
    }

    curFile.blobs = newBlobs;
    curFile.mtime = new Date().getTime();
    await Module.fs.writeFile(path, curFile);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_writeFile, (const char* path, const uint8_t* dat, uint32_t len, uint32_t pos), {
  try {
    path = UTF8ToString(path);
    const data = HEAPU8.subarray(dat, dat + len);
    let curFile;
    if (await Module.fs.exists(path)) {
      curFile = await Module.fs.readFile(path);
    } else {
      curFile = { blobs: [], mtime: 0, mode: 0o777 };
    }
    const curBlobs = curFile.blobs;
    let newBlobs = [];
    let seek = 0;
    let i;
    for (i = 0; i < curBlobs.length; i++) {
      let curBlob = curBlobs[i];
      if (pos >= seek + curBlob.size) {
        // not arrived yet
        newBlobs.push(curBlob);
      } else {
        const preserve = pos - seek;
        if (preserve > 0) {
          const cutBlobData = await Module.readBlob(curBlob).subarray(0, preserve);
          const cutBlob = await Module.createBlob(cutBlobData);
          newBlobs.push(cutBlob);
        }
        break;
      }
      seek += curBlob.size;
    }

    const contentBlob = await Module.createBlob(data);
    newBlobs.push(contentBlob);

    seek = 0;
    const end = pos + len;
    for (i = 0; i < curBlobs.length; i++) {
      let curBlob = curBlobs[i];
      if (end <= seek) {
        newBlobs.push(curBlob);
      } else if (end > seek && end <= seek + curBlob.size) {
        const preserveStart = end - seek;
        if (preserveStart != curBlob.size) {
          const cutBlobData = await Module.readBlob(curBlob).subarray(preserveStart, curBlob.size);
          const cutBlob = await Module.createBlob(cutBlobData);
          newBlobs.push(cutBlob);
        }
      }
      seek += curBlob.size;
    }

    curFile.blobs = newBlobs;
    curFile.mtime = new Date().getTime();
    await Module.fs.writeFile(path, curFile);
    return len;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int, js_readFile, (const char* path, uint8_t* dat, uint32_t len, uint32_t pos), {
  try {
    path = UTF8ToString(path);
    const data = HEAPU8.subarray(dat, dat + len);
    let curFile = await Module.fs.readFile(path);
    const curBlobs = curFile.blobs;
    let seek = 0;
    let i;
    let read = 0;
    for (i = 0; i < curBlobs.length; i++) {
      let curBlob = curBlobs[i];

      let start = pos - seek + read;
      let end = pos + len - seek;

      if (end > curBlob.size)
        end = curBlob.size;

      if (start >= 0 && start < curBlob.size && end > 0) {
        const cutBlobData = await Module.readBlob(curBlob);
        data.set(cutBlobData.subarray(start, end), read);
        read += (end - start);
        if (read == len)
          break;
      }

      seek += curBlob.size;
    }

    return read;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(uint32_t, js_getPathDetails, (const char* path), {
  try {
    path = UTF8ToString(path);
    if (!await Module.fs.exists(path)) {
      return 0;
    }
    let curFile = await Module.fs.details(path);
    if (curFile.type == 'file') {
      return Math.floor(curFile.data.mtime / 1000) + 1000;
    } else if (curFile.type == 'directory') {
      return 999;
    }
    return 0;
  } catch (e) {
    out(e);
    return 0;
  }
})

EM_ASYNC_JS(int32_t, js_removeFile, (const char* path), {
  try {
    path = UTF8ToString(path);
    if (!await Module.fs.exists(path)) {
      return 0;
    }

    await Module.fs.remove(path);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_moveFile, (const char* old_path, const char* new_path), {
  try {
    old_path = UTF8ToString(old_path);
    new_path = UTF8ToString(new_path);
    if (!await Module.fs.exists(old_path)) {
      return -2;
    }

    await Module.fs.moveFile(old_path, new_path);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_createDirectory, (const char* path), {
  try {
    path = UTF8ToString(path);
    await Module.fs.createDirectory(path);
    return 0;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_getDirectoryEntriesCount, (const char* path), {
  try {
    path = UTF8ToString(path);
    const dir = await Module.fs.readDirectory(path);
    return dir.filesCount + dir.directoriesCount;
  } catch (e) {
    out(e);
    return -5;
  }
})

EM_ASYNC_JS(int32_t, js_readDirectory, (const char* path, std::vector<wasmfs::Directory::Entry> * vec), {
  try {
    path = UTF8ToString(path);
    let entries;
    try {
      entries = await Module.fs.readDirectory(path);
    } catch (e) {
      out(e);
      return -5;
    }
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
      return 0;
    }

    int close() override {
      return 0;
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
    FastIndexedDbBackend(std::string mountPath) : mountPath(std::move(mountPath)) {}

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

  backend_t my_wasmfs_create_fast_indexeddb_backend(const char* root) {
    return wasmFS.addBackend(std::make_unique<FastIndexedDbBackend>(root));
  }
}
#endif