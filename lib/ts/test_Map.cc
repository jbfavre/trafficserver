/** @file

  Test code for the Map templates.

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
#include <stdint.h>
#include "Map.h"

typedef const char cchar;

int main(int argc, char **argv) {
  typedef Map<cchar *, cchar *> SSMap;
  typedef MapElem<cchar *, cchar *> SSMapElem;
#define form_SSMap(_p, _v) form_Map(SSMapElem, _p, _v)
  SSMap ssm;
  ssm.put("a", "A");
  ssm.put("b", "B");
  ssm.put("c", "C");
  ssm.put("d", "D");
  form_SSMap(x, ssm) ;

  StringChainHash<> h;
  cchar *hi = "hi", *ho = "ho", *hum = "hum", *hhi = "hhi";
  hhi++;
  h.put(hi);
  h.put(ho);
  assert(h.put(hum) == hum);
  assert(h.put(hhi) == hi);
  assert(h.get(hhi) == hi && h.get(hi) == hi && h.get(ho) == ho);
  assert(h.get("he") == 0 && h.get("hee") == 0);
  h.del(ho);
  assert(h.get(ho) == 0);

  StringBlockHash hh;
  hh.put(hi);
  hh.put(ho);
  assert(hh.put(hum) == 0);
  assert(hh.put(hhi) == hi);
  assert(hh.get(hhi) == hi && hh.get(hi) == hi && hh.get(ho) == ho);
  assert(hh.get("he") == 0 && hh.get("hee") == 0);
  hh.del(hi);
  assert(hh.get(hhi) == 0);
  assert(hh.get(hi) == 0);

  HashMap<cchar *, StringHashFns, int> sh;
  sh.put(hi, 1);
  sh.put(ho, 2);
  sh.put(hum, 3);
  sh.put(hhi, 4);
  assert(sh.get(hi) == 4);
  assert(sh.get(ho) == 2);
  assert(sh.get(hum) == 3);
  sh.put("aa", 5);
  sh.put("ab", 6);
  sh.put("ac", 7);
  sh.put("ad", 8);
  sh.put("ae", 9);
  sh.put("af", 10);
  assert(sh.get(hi) == 4);
  assert(sh.get(ho) == 2);
  assert(sh.get(hum) == 3);
  assert(sh.get("af") == 10);
  assert(sh.get("ac") == 7);

  ChainHashMap<cchar *, StringHashFns, int> ssh;
  ssh.put(hi, 1);
  ssh.put(ho, 2);
  ssh.put(hum, 3);
  ssh.put(hhi, 4);
  assert(ssh.get(hi) == 4);
  assert(ssh.get(ho) == 2);
  assert(ssh.get(hum) == 3);
  ssh.put("aa", 5);
  ssh.put("ab", 6);
  ssh.put("ac", 7);
  ssh.put("ad", 8);
  ssh.put("ae", 9);
  ssh.put("af", 10);
  assert(ssh.get(hi) == 4);
  assert(ssh.get(ho) == 2);
  assert(ssh.get(hum) == 3);
  assert(ssh.get("af") == 10);
  assert(ssh.get("ac") == 7);
  ssh.del(ho);
  assert(ssh.get(ho) == 0);
  
  Vec<int> ints;
  ssh.get_values(ints);
  assert(ints.n == 8);
  Vec<cchar *> chars;
  ssh.get_keys(chars);
  assert(chars.n == 8);
  printf("test_Map PASSED\n");
}

