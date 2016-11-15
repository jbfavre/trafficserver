/*
  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * @file pattern.h
 * @brief PRCE related classes (header file).
 */

#ifndef PLUGINS_EXPERIMENTAL_CACHEKEY_PATTERN_H_
#define PLUGINS_EXPERIMENTAL_CACHEKEY_PATTERN_H_

#include "ts/ink_defs.h"

#ifdef HAVE_PCRE_PCRE_H
#include <pcre/pcre.h>
#else
#include <pcre.h>
#endif

#include "common.h"

/**
 * @brief PCRE matching, capturing and replacing
 */
class Pattern
{
public:
  static const int TOKENCOUNT = 10;             /**< @brief Capturing groups $0..$9 */
  static const int OVECOUNT   = TOKENCOUNT * 3; /**< @brief pcre_exec() array count, handle 10 capture groups */

  Pattern();
  virtual ~Pattern();

  bool init(const String &pattern, const String &replacenemt);
  bool init(const String &config);
  bool empty() const;
  bool match(const String &subject);
  bool capture(const String &subject, StringVector &result);
  bool replace(const String &subject, String &result);
  bool process(const String &subject, StringVector &result);

private:
  bool compile();
  bool failed(const String &subject) const;
  void pcreFree();

  pcre *_re;          /**< @brief PCRE compiled info structure, computed during initialization */
  pcre_extra *_extra; /**< @brief PCRE study data block, computed during initialization */

  String _pattern;     /**< @brief PCRE pattern string, containing PCRE patterns and capturing groups. */
  String _replacement; /**< @brief PCRE replacement string, containing $0..$9 to be replaced with content of the capturing groups */

  int _tokenCount;              /**< @brief number of replacements $0..$9 found in the replacement string if not empty */
  int _tokens[TOKENCOUNT];      /**< @brief replacement index 0..9, since they can be used in the replacement string in any order */
  int _tokenOffset[TOKENCOUNT]; /**< @brief replacement offset inside the replacement string */
};

/**
 * @brief Named list of regular expressions.
 */
class MultiPattern
{
public:
  MultiPattern(const String name = "") : _name(name) {}
  virtual ~MultiPattern();

  bool empty() const;
  void add(Pattern *pattern);
  virtual bool match(const String &subject) const;
  const String &name() const;

protected:
  std::vector<Pattern *> _list; /**< @brief vector which dictates the order of the pattern evaluation. */
  String _name;                 /**< @brief multi-pattern name */

private:
  MultiPattern(const MultiPattern &);            // disallow
  MultiPattern &operator=(const MultiPattern &); // disallow
};

/**
 * @brief Named list of non-matching regular expressions.
 */
class NonMatchingMultiPattern : public MultiPattern
{
public:
  NonMatchingMultiPattern(const String &name) { _name = name; }
  /*
   * @brief Matches the subject string against all patterns.
   * @param subject subject string
   * @return return false if any of the patterns matches, true otherwise.
   */
  virtual bool
  match(const String &subject) const
  {
    return !MultiPattern::match(subject);
  }

private:
  NonMatchingMultiPattern();                                           // disallow
  NonMatchingMultiPattern(const NonMatchingMultiPattern &);            // disallow
  NonMatchingMultiPattern &operator=(const NonMatchingMultiPattern &); // disallow
};

/**
 * @brief Simple classifier which classifies a subject string using a list of named multi-patterns.
 */
class Classifier
{
public:
  Classifier() {}
  ~Classifier();

  bool classify(const String &subject, String &name) const;
  void add(MultiPattern *pattern);

private:
  std::vector<MultiPattern *> _list; /**< @brief vector which dictates the multi-pattern evaluation order */

  Classifier(const Classifier &);            // disallow
  Classifier &operator=(const Classifier &); // disallow
};

#endif /* PLUGINS_EXPERIMENTAL_CACHEKEY_PATTERN_H_ */
