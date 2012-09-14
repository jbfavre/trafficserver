/** @file

    TS Configuration utilities for Errata and logging.

    @section license License

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

# if !defined(_MSC_VER)
# include <stdio.h>
# include <string.h>
# endif
# include <stdarg.h>
# include <errno.h>
# include <TsErrataUtil.h>

namespace ts { namespace msg {

Errata::Code FATAL = 3; ///< Fatal, cannot continue.
Errata::Code WARN = 2; ///< Significant, should be fixed.
Errata::Code INFO = 1; /// Interesting, not necessarily a problem.
Errata::Code DEBUG = 0; /// Debugging information.

# if defined(_MSC_VER)
char* strerror_r(int err, char* s, size_t n) {
    strncpy(s, strerror(err), n-1);
    s[n-1] = 0; // guarantee null termination.
    return s;
}

# define snprintf _snprintf
# endif

Errata&
log(Errata& err, Errata::Id id, Errata::Code code, char const* text) {
  err.push(id, code, text);
  return err;
}

Errata&
log(Errata& err, Errata::Code code, char const* text) {
  err.push(0, code, text);
  return err;
}

Errata&
log(RvBase& rv, Errata::Code code, char const* text) {
    rv._errata.push(0, code, text);
    return rv._errata;
}

Errata
log(Errata::Code code, char const* text) {
  Errata err;
  err.push(0, code, text);
  return err;
}

Errata&
vlogf(
  Errata& err,
  Errata::Id id,
  Errata::Code code,
  char const* format,
  va_list& rest
) {
  static size_t const SIZE = 8192;
  char buffer[SIZE];
  
  vsnprintf(buffer, SIZE, format, rest);
  err.push(id, code, buffer);
  return err;
}

Errata&
logf(
  Errata& err,
  Errata::Id id,
  Errata::Code code,
  char const* format,
  ...
) {
  va_list rest;
  va_start(rest, format);
  return vlogf(err, id, code, format, rest);
}

Errata
logf(Errata::Code code, char const* format, ...) {
  Errata err;
  va_list rest;
  va_start(rest, format);
  return vlogf(err, Errata::Id(0), code, format, rest);
}

Errata&
logf(Errata& err, Errata::Code code, char const* format, ...) {
  va_list rest;
  va_start(rest, format);
  return vlogf(err, Errata::Id(0), code, format, rest);
}

Errata&
logf(RvBase& base, Errata::Code code, char const* format, ...) {
    va_list rest;
    va_start(rest, format);
    return vlogf(base._errata, Errata::Id(0), code, format, rest);
}

Errata
log_errno(Errata::Code code, char const* text) {
  static size_t const SIZE = 1024;
  char buffer[SIZE];
  return logf(code, "%s [%d] %s", text, errno, strerror_r(errno, buffer, SIZE));
}

Errata
vlogf_errno(Errata& errata, Errata::Id id, Errata::Code code, char const* format, va_list& rest) {
  int e = errno; // Preserve value before making system calls.
  int n;
  static int const E_SIZE = 256;
  char e_buffer[E_SIZE];
  static int const T_SIZE = 8192;
  char t_buffer[T_SIZE];
  
  n = vsnprintf(t_buffer, T_SIZE, format, rest);
  if (0 <= n && n < T_SIZE) // still have room.
    n += snprintf(t_buffer + n, T_SIZE - n, "[%d] %s", e, strerror_r(e, e_buffer, E_SIZE));
  errata.push(id, code, t_buffer);
  return errata;
}

Errata
logf_errno(Errata::Code code, char const* format, ...) {
  Errata zret;
  va_list rest;
  va_start(rest, format);
  return vlogf_errno(zret, 0, code, format, rest);
}

Errata
logf_errno(Errata& errata, Errata::Code code, char const* format, ...) {
  va_list rest;
  va_start(rest, format);
  return vlogf_errno(errata, 0, code, format, rest);
}

Errata
logf_errno(RvBase& rv, Errata::Code code, char const* format, ...) {
    va_list rest;
    va_start(rest, format);
    return vlogf_errno(rv._errata, 0, code, format, rest);
}
// ------------------------------------------------------
}} // namespace ts::msg
