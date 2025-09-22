#pragma once
/**
 * @file MqttTopic.h
 * @brief Portable MQTT topic utilities (build + validate), no Arduino deps.
 *
 * - Allocation-free: caller supplies the output buffer.
 * - No project-specific strings: you pass all segments.
 * - Safe snprintf-based joining with configurable separator (default '/').
 * - Validation helpers for publish vs. subscribe filters (wildcards).
 *
 * MQTT notes (v3.1.1 / v5):
 * - Topic names are UTF-8, up to 65,535 bytes.
 * - Publishing: topic MUST NOT contain '+' or '#'.
 * - Subscribing: '+' matches one level; '#' matches remaining levels and
 *   MUST be the last character and either alone or following a separator.
 */

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace mqttt {

struct BuildOptions {
  char separator = '/';         // Typically '/'
  bool disallowEmptyLevels = false; // If true, rejects "" segments (e.g., "//")
};

/// Append a single segment to an existing topic buffer.
inline bool append(char* out, size_t cap,
                   const char* segment,
                   const BuildOptions& opt = {}) {
  if (!out || cap == 0 || !segment) return false;

  // current length
  size_t len = ::strnlen(out, cap);
  if (len >= cap) return false;

  // optionally reject empty segments
  if (opt.disallowEmptyLevels && segment[0] == '\0') return false;

  // if empty, write first segment without leading separator
  if (len == 0) {
    int n = std::snprintf(out, cap, "%s", segment);
    return n >= 0 && static_cast<size_t>(n) < cap;
  }

  // otherwise append sep + segment
  int n = std::snprintf(out + len, cap - len, "%c%s", opt.separator, segment);
  return n >= 0 && static_cast<size_t>(n) < (cap - len);
}

/// Build from N segments in one call.
template <size_t N>
inline bool build(char* out, size_t cap,
                  const char* (&segments)[N],
                  const BuildOptions& opt = {}) {
  if (!out || cap == 0) return false;
  out[0] = '\0';
  for (size_t i = 0; i < N; ++i) {
    if (!append(out, cap, segments[i] ? segments[i] : "", opt)) return false;
  }
  return true;
}

/// Quick join for an initializer_list (handy for variable-length cases).
inline bool join(char* out, size_t cap,
                 std::initializer_list<const char*> segments,
                 const BuildOptions& opt = {}) {
  if (!out || cap == 0) return false;
  out[0] = '\0';
  for (const char* s : segments) {
    if (!append(out, cap, s ? s : "", opt)) return false;
  }
  return true;
}

/// Validate a topic for PUBLISH (no wildcards allowed).
inline bool validatePublishTopic(const char* topic, char sep = '/') {
  if (!topic || topic[0] == '\0') return false;
  for (const char* p = topic; *p; ++p) {
    if (*p == '+' || *p == '#') return false; // wildcards forbidden in publish
    // NUL is impossible here since loop ends at '\0'
  }
  return true;
}

/// Validate a topic filter for SUBSCRIBE (wildcards allowed with rules).
inline bool validateSubscribeFilter(const char* filter, char sep = '/') {
  if (!filter || filter[0] == '\0') return false;

  // '#' must be last character and either alone or following a separator.
  // '+' must occupy an entire level (i.e., between separators or at ends).
  const char* p = filter;
  bool levelStart = true; // at start of a level
  for (; *p; ++p) {
    char c = *p;
    if (c == '#') {
      // must be last character
      if (*(p + 1) != '\0') return false;
      // must be alone or preceded by separator
      if (p != filter && *(p - 1) != sep) return false;
      return true; // valid and done
    }
    if (c == '+') {
      // '+' must occupy the whole level: next char must be sep or NUL
      char next = *(p + 1);
      char prev = (p == filter) ? '\0' : *(p - 1);
      if (!(levelStart && (next == sep || next == '\0'))) return false;
      // ok; this level consumed by '+'
    }
    levelStart = (c == sep);
  }

  // if we reached here with no rule violations, it's valid
  return true;
}

} // namespace mqttt
