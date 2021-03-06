#include "wrapper.hpp"
#include "parse.hpp"
#include "common.hpp"
#include "tstring.hpp"
#include "token_iterator.hpp"

#include <sstream>
#include <iostream>

NAMESPACE(node)

// Returns the pointer to the node at the specified path
// This will return the inner node of a wrapper.
base_s wrapper::get_child_ptr(tstring path) const {
  if (auto immediate_path = cut_front(trim(path), '.'); !immediate_path.untouched()) {
    if (auto child = get_wrapper(immediate_path))
      return child->get_child_ptr(path);
  } else if (auto iterator = map.find(path); iterator != map.end()) {
    if (auto child = std::dynamic_pointer_cast<wrapper>(iterator->second))
      return child->map[""];
    return iterator->second;
  }
  return {};
}

// Returns the pointer to the storage of the node or wrapper at the specified path
// Returns null if it doesn't exist
base_s* wrapper::get_child_place(tstring path) {
  if (auto immediate_path = cut_front(trim(path), '.'); !immediate_path.untouched()) {
    if (auto child = get_wrapper(immediate_path))
      return child->get_child_place(path);
  } else if (auto iterator = map.find(path); iterator != map.end())
    return &iterator->second;
  return nullptr;
}

std::optional<string> wrapper::get_child_safe(const tstring& path) const {
  auto ptr = get_child_ptr(path);
  return ptr ? ptr->get() : std::optional<string>{};
}

string wrapper::get_child(const tstring& path) const {
  auto ptr = get_child_ptr(path);
  return ptr ? ptr->get() : throw std::logic_error("Child does not exist: " + path);
}

string wrapper::get_child(const tstring& path, string&& fallback) const {
  try {
    auto result = get_child_safe(path);
    return result ? *result : move(fallback);
  } catch (const std::exception& e) {
    LG_DBUG("Error: " << e.what());
    return fallback;
  }
}

wrapper_s wrapper::get_wrapper(const string& path) const {
  if (auto it = map.find(path); it != map.end())
    return std::dynamic_pointer_cast<wrapper>(it->second);
  return wrapper_s();
}

base_s& wrapper::add(tstring path) {
  trim(path);
  for (char c : path)
    if (auto invalid = strchr(" #$\"'(){}[]", c))
      THROW_ERROR(wrapper, "Invalid character '" + string{*invalid} + "' in path: " + path);

  if (auto immediate_path = cut_front(path, '.'); !immediate_path.untouched()) {
    // This isn't the final part of the path
    auto ancestor = add_wrapper(immediate_path);
    return ancestor->add(path);
  } else {
    // This is the final part of the path
    auto& place = map[path];
    if (!place)
      return place;
    auto wrp = std::dynamic_pointer_cast<wrapper>(place);
    return wrp ? wrp->map[""] : place;
  }
}

base_s& wrapper::add(tstring path, const base_s& value) {
  auto& place = add(path);
  return !place ? (place = value) : !value ? place : THROW_ERROR(wrapper, "Add: Duplicate key");
}

base_s& wrapper::add(tstring path, parse_context& context, tstring& value) {
  add(path);
  context.parent = shared_from_this();
  context.current.reset();
  context.place = get_child_place(path);
  if (auto node = parse_raw<string>(context, value))
    return context.get_place() = node;
  return *context.place;
}

wrapper_s wrapper::add_wrapper(const string& path) {
  auto& child = map[path];
  wrapper_s result;
  if (!child)
    child = result = std::make_shared<wrapper>();
  else if (!(result = std::dynamic_pointer_cast<wrapper>(child)))
    result = wrap(child);
  return result;
}

void wrapper::iterate_children(std::function<void(const string&, const base_s&)> processor) const {
  for(auto& pair : map)
    processor(pair.first, pair.second);
}

wrapper_s wrapper::wrap(base_s& place) {
  auto wrp = std::make_shared<wrapper>(place);
  place = wrp;
  return wrp;
}

wrapper::operator string() const {
  const auto& value = get_child_ptr(""_ts);
  return value ? value->get() : "";
}

void wrapper::merge(const const_wrapper_s& src, clone_context& context) {
  auto ancestors_mark = context.ancestors.size();
  context.ancestors.emplace_back(src, shared_from_this());
  for(auto& pair : src->map) {
    if (!pair.second || (!pair.first.empty() && pair.first.front() == '.'))
      continue;
    auto last_path = context.current_path;
    context.current_path += context.ancestors.size() == 1 ? pair.first : ("." + pair.first);
    try {
      auto& place = map[pair.first];
      if (auto src_wrp = std::dynamic_pointer_cast<wrapper>(pair.second)) {
        if (src_wrp->map.find(".hidden") != src_wrp->map.end())
          continue;
        wrapper_s wrp;
        if (!place)
          place = wrp = std::make_shared<wrapper>();
        else if (!(wrp = std::dynamic_pointer_cast<wrapper>(place)))
          wrp = wrap(place);
        wrp->merge(src_wrp, context);
      } else if (!place)
        place = checked_clone<string>(pair.second, context, "wrapper::merge");
    } catch (const std::exception& e) {
      context.report_error("Exception while cloning " + context.current_path + ": " + e.what());
    }
    context.current_path = last_path;
  }
  context.ancestors.erase(context.ancestors.begin() + ancestors_mark, context.ancestors.end());
}

void wrapper::optimize(clone_context& context) {
  context.optimize = true;
  auto result = std::make_shared<wrapper>();
  result->merge(shared_from_this(), context);
  result->map.swap(map);
}

base_s wrapper::clone(clone_context& context) const {
  auto result = std::make_shared<wrapper>();
  result->merge(shared_from_this(), context);
  return result;
}

bool wrapper::is_fixed() const {
  auto it = map.find("");
  return it != map.end() && it->second ? it->second->is_fixed() : true;
}
NAMESPACE_END
