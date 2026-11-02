#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

constexpr int LFS_ERR_OK = 0;
constexpr int LFS_ERR_IO = -5;
constexpr int LFS_ERR_NOENT = -2;
constexpr int LFS_O_RDONLY = 1;
constexpr int LFS_O_WRONLY = 2;
constexpr int LFS_O_CREAT = 0x100;
constexpr int LFS_O_TRUNC = 0x400;
constexpr uint8_t LFS_TYPE_REG = 1;
constexpr uint8_t LFS_TYPE_DIR = 2;
struct lfs_file_t {
  std::string path;
  size_t position = 0;
};
struct lfs_info {
  uint8_t type = LFS_TYPE_REG;
  uint32_t size = 0;
};

namespace Pinetime::Controllers {
  class FS {
  public:
    using Bytes = std::vector<uint8_t>;
    enum class Op { Stat, Open, Read, Write, Close, Rename, Delete };
    struct Call {
      Op op;
      std::string path;
    };
    struct Fault {
      Op op;
      std::string path;
      int result;
    };

    std::map<std::string, Bytes> files;
    std::map<std::string, uint8_t> types;
    std::vector<Call> calls;
    std::optional<Fault> fault;

    void FailNext(Op op, const std::string& path, int result = LFS_ERR_IO) {
      fault = Fault {op, path, result};
    }

    size_t Count(Op op, const std::string& path) const {
      return std::count_if(calls.begin(), calls.end(), [&](const auto& call) { return call.op == op && call.path == path; });
    }

    int Stat(const char* path, lfs_info* info) {
      if (const auto result = Before(Op::Stat, path)) {
        return *result;
      }
      const auto found = files.find(path);
      if (found == files.end()) {
        return LFS_ERR_NOENT;
      }
      info->type = types.count(path) ? types[path] : LFS_TYPE_REG;
      info->size = found->second.size();
      return LFS_ERR_OK;
    }

    int FileOpen(lfs_file_t* file, const char* path, int flags) {
      if (const auto result = Before(Op::Open, path)) {
        return *result;
      }
      if (!files.count(path) && !(flags & LFS_O_CREAT)) {
        return LFS_ERR_NOENT;
      }
      file->path = path;
      file->position = 0;
      if (flags & LFS_O_TRUNC) {
        files[path].clear();
      } else if (flags & LFS_O_CREAT) {
        files.try_emplace(path);
      }
      return LFS_ERR_OK;
    }

    int FileRead(lfs_file_t* file, uint8_t* buffer, uint32_t length) {
      const auto result = Before(Op::Read, file->path);
      if (result && *result < 0) {
        return *result;
      }
      const auto& bytes = files.at(file->path);
      const auto count = std::min<size_t>({length, bytes.size() - file->position, result ? static_cast<size_t>(*result) : length});
      std::copy_n(bytes.begin() + file->position, count, buffer);
      file->position += count;
      return static_cast<int>(count);
    }

    int FileWrite(lfs_file_t* file, const uint8_t* buffer, uint32_t length) {
      const auto result = Before(Op::Write, file->path);
      if (result && *result < 0) {
        return *result;
      }
      const auto count = std::min<size_t>(length, result ? static_cast<size_t>(*result) : length);
      auto& bytes = files.at(file->path);
      bytes.resize(std::max(bytes.size(), file->position + count));
      std::copy_n(buffer, count, bytes.begin() + file->position);
      file->position += count;
      return static_cast<int>(count);
    }

    int FileClose(lfs_file_t* file) {
      return Before(Op::Close, file->path).value_or(LFS_ERR_OK);
    }

    int Rename(const char* from, const char* to) {
      if (const auto result = Before(Op::Rename, from)) {
        return *result;
      }
      if (!files.count(from)) {
        return LFS_ERR_NOENT;
      }
      files[to] = std::move(files.at(from));
      files.erase(from);
      return LFS_ERR_OK;
    }

    int FileDelete(const char* path) {
      if (const auto result = Before(Op::Delete, path)) {
        return *result;
      }
      return files.erase(path) ? LFS_ERR_OK : LFS_ERR_NOENT;
    }

  private:
    std::optional<int> Before(Op op, const std::string& path) {
      calls.push_back(Call {op, path});
      if (fault && fault->op == op && fault->path == path) {
        const auto result = fault->result;
        fault.reset();
        return result;
      }
      return std::nullopt;
    }
  };
}
