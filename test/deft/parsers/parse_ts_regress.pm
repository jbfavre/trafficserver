# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#
#  parse_regress_ts.pm
#
#
#   Description:
#
#   
#
#

package parse_ts_regress;
require Exporter;

use strict Vars;

our @ISA = qw(Exporter);
our @EXPORT = qw();
our @EXPORT_OK = qw();
our $VERSION = 1.00;

sub process_test_log_line {
    my ($instance_id, $level, $line) = @_;

    if ($level eq "stderr") {
	if ($$line =~ /FAIL/i ||
	    $$line =~ /abort/i ||
	    $$line =~ /core/i ||
	    $$line =~ /assert/i) {
	    return "error";
	} else {
	    return "ok";
	}
    } elsif ($$line =~ /error/i) {
	return "error";
    } elsif ($$line =~ /warn/i) {
	return "warning";
    } else {
	return "ok";
    }
    
}
