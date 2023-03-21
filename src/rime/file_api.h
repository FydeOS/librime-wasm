//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 20230-03-20 Yunhao Tian <t123yh@outlook.com>
//

#ifndef RIME_FILE_API_H_
#define RIME_FILE_API_H_

#include <cstddef>
#include <string>
#include "dict/mapped_file.h"
#include <boost/operators.hpp>

namespace rime {
  class BufferedFileReader : MappedFile, public std::streambuf {
  public:
    explicit BufferedFileReader(const std::string& file_name);
  };

  class BufferedFileWriter : MappedFile, public std::streambuf {
  public:
    explicit BufferedFileWriter(const std::string& file_name);
    ~BufferedFileWriter() override;
  protected:
    std::streamsize xsputn(const char *s, std::streamsize n) override;
    int sync() override;
  };
}

#endif //RIME_FILE_API_H
