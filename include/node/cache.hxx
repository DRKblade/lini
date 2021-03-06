#include "parse.hpp"

namespace node {

template<class T>
cache<T>::operator T() const {
  if (auto now = std::chrono::steady_clock::now(); now > cache_expire) {
    cache_value = calculator->operator T();
    cache_expire = now + std::chrono::milliseconds(duration_ms->operator int());
  }
  return cache_value;
}

template<class T> base_s
cache<T>::clone(clone_context& context) const {
  auto result = std::make_shared<cache>();
  result->calculator = checked_clone<T>(calculator, context, "cache::clone");
  result->duration_ms = checked_clone<int>(duration_ms, context, "cache::clone");
  result->cache_value = cache_value;
  result->cache_expire = cache_expire;
  return result;
}

template<class T> std::shared_ptr<cache<T>>
cache<T>::parse(parse_context& context, parse_preprocessed& prep) {
  if (prep.token_count != 3)
    throw parse_error("cache: Expected 2 components");
  auto result = std::make_shared<cache>();
  result->duration_ms = checked_parse_raw<int>(context, prep.tokens[1]);
  result->calculator = checked_parse_raw<T>(context, prep.tokens[2]);
  return result;
}

template<class T>
refcache<T>::operator T() const {
  auto now = std::chrono::steady_clock::now();
  if (auto newsrc = source->get(); newsrc != prevsrc || now > cache_expire || unset) {
    cache_value = calculator->operator T();
    prevsrc = newsrc;
    unset = false;
    cache_expire = now + std::chrono::milliseconds(duration_ms);
  }
  return cache_value;
}

template<class T> base_s
refcache<T>::clone(clone_context& context) const {
  auto result = std::make_shared<refcache>();
  result->source = checked_clone<string>(source, context, "refcache::clone");
  result->calculator = checked_clone<T>(calculator, context, "refcache::clone");
  result->cache_value = cache_value;
  result->duration_ms = duration_ms;
  result->prevsrc = prevsrc;
  result->unset = unset;
  return result;
}

template<class T> std::shared_ptr<refcache<T>>
refcache<T>::parse(parse_context& context, parse_preprocessed& prep) {
  if (prep.token_count != 4)
    throw parse_error("cache: Expected 4 components");
  auto result = std::make_shared<refcache>();
  result->source = checked_parse_raw<T>(context, prep.tokens[1]);
  result->duration_ms = node::parse<int>(prep.tokens[2], "refcache::parse");
  result->calculator = checked_parse_raw<T>(context, prep.tokens[3]);
  result->unset = true;
  return result;
}

template<class T>
arrcache<T>::operator T() const {
  return get(source->operator int());
}

template<class T> T
arrcache<T>::get(size_t index) const {
  if (index >= cache_arr.size())
    throw node_error("Index larger than cache maximum: " + std::to_string(index) + " > " + std::to_string(cache_arr.size() - 1));
  auto& result = cache_arr.operator[](index);
  if (!result) {
    result = calculator->operator T();
  }
  return *result;
}

template<class T> base_s
arrcache<T>::clone(clone_context& context) const {
  auto result = std::make_shared<arrcache>();
  result->source = checked_clone<int>(source, context, "arrcache::clone");
  result->calculator = checked_clone<T>(calculator, context, "arrcache::clone");
  result->cache_arr.reserve(cache_arr.size() + 1);
  for (size_t i = 0; i < cache_arr.size(); i++)
    result->cache_arr.emplace_back();
  return result;
}

inline std::optional<unsigned long int> parse_ulong(const char* str, size_t len) {
  char* end;
  auto result = std::strtoul(str, &end, 10);
  if (end != str + len)
    return {};
  return result;
}

template<class T> std::shared_ptr<arrcache<T>>
arrcache<T>::parse(parse_context& context, parse_preprocessed& prep) {
  if (prep.token_count == 4) {
    auto result = std::make_shared<arrcache>();
    auto size = parse_ulong(prep.tokens[1].begin(), prep.tokens[1].size());
    if (size) {
      result->cache_arr.reserve(*size + 1);
      for (size_t i = 0; i < size; i++)
        result->cache_arr.emplace_back();
    } else
      throw parse_error("1st argument must be the size of the cache: " + context.raw);
    result->source = checked_parse_raw<int>(context, prep.tokens[2]);
    result->calculator = checked_parse_raw<T>(context, prep.tokens[3]);
    return result;
  } else
    throw parse_error("arrcache: Expected 3 components");
}

}
