#pragma once

/**
 * @file term-ime.hpp
 * @brief TTY Chinese Input Method Engine Library
 *
 * This library provides a standalone Chinese input method engine
 * that can be embedded in terminal applications.
 *
 * Usage:
 *   #include <term-ime/term-ime.hpp>
 *
 *   auto dict = std::make_unique<termime::Dict>();
 *   dict->load("pinyin.dict");
 *
 *   termime::PinyinIme ime(std::move(dict));
 *
 *   ime.input('n');
 *   ime.input('i');
 *   auto candidates = ime.candidates();
 *   auto result = ime.select(0);  // "你"
 */

#include "engine.hpp"
#include "pinyin.hpp"
#include "dict.hpp"

namespace termime {

// Convenience aliases
using ImeEngine = ::ImeEngine;
using PinyinIme = ::PinyinIme;
using Dict = ::Dict;
using Candidate = ::Candidate;
using ImeState = ::ImeState;

}  // namespace termime
