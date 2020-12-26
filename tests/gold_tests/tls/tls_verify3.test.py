'''
'''
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

Test.Summary = '''
Test tls server certificate verification options
'''

# Define default ATS
ts = Test.MakeATSProcess("ts", select_ports=True, enable_tls=True)
server_foo = Test.MakeOriginServer("server_foo",
                                   ssl=True,
                                   options={"--key": "{0}/signed-foo.key".format(Test.RunDirectory),
                                            "--cert": "{0}/signed-foo.pem".format(Test.RunDirectory)})
server_bar = Test.MakeOriginServer("server_bar",
                                   ssl=True,
                                   options={"--key": "{0}/signed-bar.key".format(Test.RunDirectory),
                                            "--cert": "{0}/signed-bar.pem".format(Test.RunDirectory)})
server = Test.MakeOriginServer("server", ssl=True)

request_foo_header = {"headers": "GET / HTTP/1.1\r\nHost: foo.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
request_bad_foo_header = {"headers": "GET / HTTP/1.1\r\nHost: badfoo.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
request_bar_header = {"headers": "GET / HTTP/1.1\r\nHost: bar.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
request_bad_bar_header = {"headers": "GET / HTTP/1.1\r\nHost: badbar.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
response_header = {"headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
server_foo.addResponse("sessionlog.json", request_foo_header, response_header)
server_foo.addResponse("sessionlog.json", request_bad_foo_header, response_header)
server_bar.addResponse("sessionlog.json", request_bar_header, response_header)
server_bar.addResponse("sessionlog.json", request_bad_bar_header, response_header)

# add ssl materials like key, certificates for the server
ts.addSSLfile("ssl/signed-foo.pem")
ts.addSSLfile("ssl/signed-foo.key")
ts.addSSLfile("ssl/signed-bar.pem")
ts.addSSLfile("ssl/signed-bar.key")
ts.addSSLfile("ssl/server.pem")
ts.addSSLfile("ssl/server.key")
ts.addSSLfile("ssl/signer.pem")
ts.addSSLfile("ssl/signer.key")

ts.Disk.remap_config.AddLine(
    'map https://foo.com:{1}/ https://127.0.0.1:{0}'.format(server_foo.Variables.SSL_Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map https://bob.foo.com:{1}/ https://127.0.0.1:{0}'.format(server_foo.Variables.SSL_Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map https://bar.com:{1}/ https://127.0.0.1:{0}'.format(server_bar.Variables.SSL_Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map https://bob.bar.com:{1}/ https://127.0.0.1:{0}'.format(server_bar.Variables.SSL_Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map / https://127.0.0.1:{0}'.format(server.Variables.SSL_Port))

ts.Disk.ssl_multicert_config.AddLine(
    'dest_ip=* ssl_cert_name=server.pem ssl_key_name=server.key'
)

# Case 1, global config policy=permissive properties=signature
#         override for foo.com policy=enforced properties=all
ts.Disk.records_config.update({
    'proxy.config.ssl.server.cert.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.server.private_key.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.server.cipher_suite': 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:AES128-GCM-SHA256:AES256-GCM-SHA384:ECDHE-RSA-RC4-SHA:ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:RC4-SHA:RC4-MD5:AES128-SHA:AES256-SHA:DES-CBC3-SHA!SRP:!DSS:!PSK:!aNULL:!eNULL:!SSLv2',
    # set global policy
    'proxy.config.ssl.client.verify.server.policy': 'PERMISSIVE',
    'proxy.config.ssl.client.verify.server.properties': 'ALL',
    'proxy.config.ssl.client.CA.cert.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.client.CA.cert.filename': 'signer.pem',
    'proxy.config.exec_thread.autoconfig.scale': 1.0,
    'proxy.config.url_remap.pristine_host_hdr': 1
})

ts.Disk.sni_yaml.AddLines([
    'sni:',
    '- fqdn: bob.bar.com',
    '  verify_server_policy: ENFORCED',
    '  verify_server_properties: ALL',
    '- fqdn: bob.*.com',
    '  verify_server_policy: ENFORCED',
    '  verify_server_properties: SIGNATURE',
    "- fqdn: '*bar.com'",
    '  verify_server_policy: DISABLED',
])

tr = Test.AddTestRun("foo.com Permissive-Test")
tr.Setup.Copy("ssl/signed-foo.key")
tr.Setup.Copy("ssl/signed-foo.pem")
tr.Setup.Copy("ssl/signed-bar.key")
tr.Setup.Copy("ssl/signed-bar.pem")
tr.Processes.Default.Command = "curl -v -k --resolve 'foo.com:{0}:127.0.0.1' https://foo.com:{0}".format(ts.Variables.ssl_port)
tr.ReturnCode = 0
tr.Processes.Default.StartBefore(Test.Processes.ts)
tr.Processes.Default.StartBefore(server_foo)
tr.Processes.Default.StartBefore(server_bar)
tr.Processes.Default.StartBefore(server)
tr.StillRunningAfter = server
tr.StillRunningAfter = ts
tr.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should have succeeded")

tr = Test.AddTestRun("my.foo.com Permissive-Test log failure")
tr.Processes.Default.Command = "curl -v -k --resolve 'my.foo.com:{0}:127.0.0.1' https://my.foo.com:{0}".format(
    ts.Variables.ssl_port)
tr.ReturnCode = 0
tr.StillRunningAfter = server
tr.StillRunningAfter = ts
tr.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should have succeeded")


tr2 = Test.AddTestRun("bob.bar.com Override-enforcing-Test")
tr2.Processes.Default.Command = "curl -v -k --resolve 'bob.bar.com:{0}:127.0.0.1' https://bob.bar.com:{0}/".format(
    ts.Variables.ssl_port)
tr2.ReturnCode = 0
tr2.StillRunningAfter = server
tr2.StillRunningAfter = ts
tr2.Processes.Default.Streams.stdout = Testers.ContainsExpression("Could Not Connect", "Curl attempt should have succeeded")

tr3 = Test.AddTestRun("bob.foo.com override-enforcing-name-test")
tr3.Processes.Default.Command = "curl -v -k --resolve 'bob.foo.com:{0}:127.0.0.1' https://bob.foo.com:{0}/".format(
    ts.Variables.ssl_port)
tr3.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should not fail")
tr3.ReturnCode = 0
tr3.StillRunningAfter = server
tr3.StillRunningAfter = ts

tr3 = Test.AddTestRun("random.bar.com override-no-test")
tr3.Processes.Default.Command = "curl -v -k --resolve 'random.bar.com:{0}:127.0.0.1' https://random.bar.com:{0}".format(
    ts.Variables.ssl_port)
tr3.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should not fail")
tr3.ReturnCode = 0
tr3.StillRunningAfter = server
tr3.StillRunningAfter = ts


# Over riding the built in ERROR check since we expect tr3 to fail
ts.Disk.diags_log.Content = Testers.ContainsExpression(
    "WARNING: SNI \(bob.bar.com\) not in certificate", "Make sure bob.bar name checked failed.")
ts.Disk.diags_log.Content += Testers.ContainsExpression(
    r"WARNING: Core server certificate verification failed for \(my.foo.com\). Action=Continue",
    "Make sure default permissive action takes")
