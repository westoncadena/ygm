#pragma once

#include <sys/time.h>
#include <syscall.h>
#include <unistd.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stack>
#include <string>
#include <unordered_map>

#include <any>

using ProcessID      = unsigned long int;
using ThreadID       = unsigned long int;
using TimeResolution = unsigned long long int;
using EventType      = char*;
using ConstEventType = const char*;

namespace ygm::detail {

class tracer {
 public:
  tracer() {}

  ~tracer() {
    if (output_file.is_open()) {
      output_file.close();
      if (output_file.fail()) {
        std::cerr << "Error closing trace file!" << std::endl;
        // Handle failure to close the file
      }
    }
  }

  inline TimeResolution get_time() {
    struct timeval tv {};
    gettimeofday(&tv, NULL);
    TimeResolution t = 1000000 * tv.tv_sec + tv.tv_usec;
    return t;
  }

  void create_directory(std::string trace_path) {
    if (!std::filesystem::is_directory(trace_path)) {
      if (!std::filesystem::create_directories(trace_path)) {
        std::cerr << "Error creating directory!" << std::endl;
      }
    }
  }

  void trace_event(ConstEventType event_name, int rank,
                   TimeResolution start_time, TimeResolution end_time,
                   std::string                                trace_path,
                   std::unordered_map<std::string, std::any>* metadata_ptr) {
    if (!output_file.is_open()) {
      open_file(rank, trace_path);
    }

    ProcessID pid = rank;

    ThreadID tid = syscall(SYS_gettid);

    ConstEventType category = "ygm";

    TimeResolution duration = end_time - start_time;

    std::string meta_str = stream_metadata(*metadata_ptr);

    // convert to json formating
    convert_json(event_name, category, start_time, duration, meta_str, pid, tid,
                 &size, data);

    output_file.write(data, size);
  }

 private:
  std::ofstream output_file;

  static const int MAX_LINE_SIZE      = 4096;
  static const int MAX_META_LINE_SIZE = 3000;

  char data[MAX_LINE_SIZE];
  int  size = 0;

  void convert_json(ConstEventType event_name, ConstEventType category,
                    TimeResolution start_time, TimeResolution duration,
                    std::string meta_str, ProcessID process_id,
                    ThreadID thread_id, int* size, char* data) {
    *size = snprintf(
        data, MAX_LINE_SIZE,
        "{\"name\":\"%s\",\"cat\":\"%s\",\"pid\":\"%lu\","
        "\"tid\":\"%lu\",\"ts\":\"%llu\",\"dur\":\"%llu\",\"ph\":\"X\","
        "\"args\":{%s}},\n",
        event_name, category, process_id, thread_id, start_time, duration,
        meta_str.c_str());
  }

  void open_file(int rank, std::string trace_path) {
    std::string file_path =
        trace_path + "/trace_" + std::to_string(rank) + ".txt";
    ;

    output_file.open(file_path);

    if (!output_file.is_open()) {
      std::cerr << "Error opening tracing file for writing!" << std::endl;
      // Handle failure to open the file
    }

    size = snprintf(data, MAX_LINE_SIZE, "[\n");
    output_file.write(data, size);
  }

  std::string stream_metadata(
      const std::unordered_map<std::string, std::any>& metadata) {
    std::stringstream meta_stream;
    bool              has_meta = false;
    size_t            i        = 0;

    for (const auto& item : metadata) {
      if (has_meta) {
        meta_stream << ",";
      }

      try {
        meta_stream << "\"" << item.first << "\":\"";

        if (item.second.type() == typeid(unsigned int)) {
          const auto& value = std::any_cast<unsigned int>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(int)) {
          const auto& value = std::any_cast<int>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(const char*)) {
          const auto& value = std::any_cast<const char*>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(std::string)) {
          const auto& value = std::any_cast<std::string>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(size_t)) {
          const auto& value = std::any_cast<size_t>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(long)) {
          const auto& value = std::any_cast<long>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(ssize_t)) {
          const auto& value = std::any_cast<ssize_t>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(off_t)) {
          const auto& value = std::any_cast<off_t>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(off64_t)) {
          const auto& value = std::any_cast<off64_t>(item.second);
          meta_stream << value;
        } else if (item.second.type() == typeid(float)) {
          const auto& value = std::any_cast<float>(item.second);
          meta_stream << value;
        } else {
          meta_stream << "No conversion for " << item.first << "'s type";
        }

      } catch (const std::bad_any_cast&) {
        meta_stream << "No conversion for type";
      }
      meta_stream << "\"";
      has_meta = true;
      ++i;
    }
    return meta_stream.str();
  }
};
};  // namespace ygm::detail