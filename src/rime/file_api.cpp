//
// Created by fydeos on 23-3-20.
//

#include <boost/filesystem/operations.hpp>
#include <fstream>
#include "file_api.h"

namespace rime {
  BufferedFileReader::BufferedFileReader(const string &file_name) : MappedFile(file_name) {
    OpenReadOnly();
    this->setg(address(), address(), address() + file_size());
  }

  BufferedFileWriter::BufferedFileWriter(const string &file_name) : MappedFile(file_name) {
    Create(1024);
  }

  std::streamsize BufferedFileWriter::xsputn(const char *s, std::streamsize n) {
    char* buf = Allocate<char>(n);
    memcpy(buf, s, n);
    return n;
  }

  int BufferedFileWriter::sync() {
    Flush();
    return 0;
  }

  BufferedFileWriter::~BufferedFileWriter() {
    Flush();
  }
}