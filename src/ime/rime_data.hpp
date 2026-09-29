#pragma once

#include <string>
#include <vector>

// A rime-data dir only counts as term-ime's own if it holds this schema. The
// system librime data dir also exists, and selecting it leaves the app showing
// [拼] while typing yields nothing at all -- a silent dead end for anyone whose
// install layout the candidate search missed.
//
// Declared apart from rime_engine.hpp because that header pulls in librime's
// types, which do not coexist with gtest's.
inline constexpr const char* kRimeDataMarker = "luna_pinyin_simp_fuzzy.schema.yaml";

// Pick the rime shared-data directory.
//
// `configured` (AppConfig::rime_shared_data_dir) wins outright when set, marker
// or not -- it is an explicit choice by the user. Otherwise the first candidate
// holding kRimeDataMarker is returned; if none has the marker, the first
// candidate that merely exists, so a hand-made layout still runs (the caller
// logs that). `fallback` is returned when no candidate exists.
std::string select_shared_data_dir(const std::string& configured, const std::vector<std::string>& candidates,
                                   const std::string& fallback);
