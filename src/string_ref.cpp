#include "string_ref.hpp"
#include "common.hpp"
#include "execstream.hpp"
#include "string_interpolate.hpp"
#include "document.hpp"

#include <fstream>
#include <cstdlib>
#include <array>

GLOBAL_NAMESPACE

using namespace std;

bool container::has_child(const tstring& path) const {
  auto ptr = get_child_ptr(path);
  return ptr && *ptr;
}

optional<string> container::get_child(const tstring& path) const {
  if (auto ptr = get_child_ptr(path); ptr) {
    if (auto& value = *ptr; value) {
      return value->get();
    } else
      LG_INFO("document-get: failed due to value being null: " << path);
  } else
    LG_INFO("document-get: failed due to key not found: " << path);
  return {};
}

string container::get_child(const tstring& path, string&& fallback) const {
  if (auto result = get_child(path); result)
    return *result;
  return forward<string>(fallback);
}

string_ref& container::get_child_ref(const tstring& path) const {
  auto ptr = get_child_ptr(path);
  if (ptr && *ptr)
    return **ptr;
  throw string_ref::error("Key is empty");
}

bool container::set(const tstring& path, const string& value) {
  if (auto ptr = get_child_ptr(path); ptr) {
    if (auto settable_ref = dynamic_cast<settable*>(ptr->get()); settable_ref && !settable_ref->readonly()) {
      settable_ref->set(value);
      return true;
    }
  }
  return false;
}

string local_ref::get() const {
  if (ref && *ref)
    return (*ref)->get();
  return use_fallback("Referenced key doesn't exist");
}

bool local_ref::readonly() const {
  if (*ref) {
    if (auto settable_ref = dynamic_cast<settable*>(ref->get()); settable_ref) {
      return settable_ref->readonly();
    }
  } else if (fallback) {
    if (auto settable_ref = dynamic_cast<settable*>(fallback.get()); settable_ref) {
      return settable_ref->readonly();
    }
  }
  return false;
}

void local_ref::set(const string& val) {
  if (*ref) {
    if (auto settable_ref = dynamic_cast<settable*>(ref->get()); settable_ref) {
      settable_ref->set(val);
    }
  } else if (fallback) {
    if (auto settable_ref = dynamic_cast<settable*>(fallback.get()); settable_ref) {
      settable_ref->set(val);
    }
  }
}

string fallback_ref::use_fallback(const string& msg) const {
  if (fallback)
    return fallback->get();
  throw error("Reference failed: " + msg + ". And no fallback was found");
}

string env_ref::get() const {
  auto result = getenv(value->get().data());
  if (result == nullptr)
    return use_fallback("Environment variable not found: " + value->get());
  return string(result);
}

void env_ref::set(const string& newval) {
  setenv(value->get().data(), newval.data(), true);
}

string file_ref::get() const {
  ifstream ifs(value->get().data());
  if (ifs.fail())
    return use_fallback("Can't read file: " + value->get());
  string result(istreambuf_iterator<char>{ifs}, {});
  ifs.close();
  auto last_line = result.find_last_not_of("\r\n");
  result.erase(last_line + 1);
  return move(result);
}

void file_ref::set(const string& content) {
  ofstream ofs(value->get().data(), ios_base::trunc);
  if (ofs.fail())
    throw error("Can't write to file: " + value->get());
  ofs << content;
  ofs.close();
}

string color_ref::get() const {
  try {
    auto result = processor.operate(value->get());
    if (result.empty() && fallback)
      return fallback->get();
    return result;
  } catch(const exception& e) {
    return use_fallback("Color processing failed, due to: " + string(e.what()));
  }
}

string cmd_ref::get() const {
  string result;
  try {
    execstream exec(value->get().data(), execstream::type_out);
    result = exec.readall();
    if (auto exitstat = WEXITSTATUS(exec.close()); exitstat)
      use_fallback("Process exited with status " + to_string(exitstat) + ": " + value->get());
  } catch (const exception& e) {
    use_fallback("Can't start process due to: " + string(e.what()));
  }
  auto last_line = result.find_last_not_of("\r\n");
  result.erase(last_line + 1);
  return result;
}

#define SIRR string_interpolate_ref::replacement_list
struct SIRR::iterator {
  vector<string_ref_p>::const_iterator it;

  iterator(const std::vector<string_ref_p>::const_iterator& it) : it(it) {}
  iterator(const iterator& other) : it(other.it) {}
  string operator*() { return (*it)->get(); }
  iterator& operator++() { it++; return *this; }
  iterator operator++(int) { iterator res(it); operator++(); return res; }
  bool operator==(const iterator& other) { return other.it == it; }
};

SIRR::iterator SIRR::begin() const {
  return iterator(list.begin());
}

SIRR::iterator SIRR::end() const {
  return iterator(list.end());
}
#undef SIRR

string string_interpolate_ref::get() const {
  return interpolate(base, positions, replacements);
}

GLOBAL_NAMESPACE_END
