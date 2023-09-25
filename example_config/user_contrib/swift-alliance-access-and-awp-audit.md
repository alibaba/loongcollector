# SWIFT Alliance Access and Alliance Web Platform(AWP) Audit

## Description

The following configuration parses Alliance Access and Alliance Web Platform logs to identify user login behaviors.

SWIFT (Society for Worldwide Interbank Financial Telecommunication) is a global financial messaging network that enables banks and financial institutions to securely exchange electronic messages and information. Alliance Access is a software solution provided by SWIFT that allows banks and financial institutions to connect to the SWIFT network and manage their messaging traffic. Alliance Web Platform is a web-based interface provided by SWIFT that allows banks and financial institutions to access SWIFT services, manage their messaging traffic, and exchange financial information with other institutions.

### Example Input

``` plain
May  23 16:40:24 Swift-SAA WS_appsrv[1111]: CEF:0|SWIFT|Alliance Access|7.6.62|ABC-0000|Successful signon|Low|cn1=2222222222 cn1Label=Event Sequence ID cn2=0 cn2Label=Is Alarm cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs2=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs2Label=Correlation ID cs4=5f5f5f5f5f5f5f5f5f5f cs4Label=Session ID cs5=Security cs5Label=Event Type cat=Operator msg=Operator jack : group, Password + TOTP authenticated - successfully signed on to the terminal '10.100.10.100@222' at 16:40:24 (Asia/Shanghai) using 'Alliance Web Platform'.\nOperator Profiles: profile1, profile2 suid=jack dvchost=Swift-SAA dvc=10.100.10.100 dvcmac=00:AA:11:BB:22:CC deviceProcessName=WS_appsrv src=10.100.10.100 dtz=Asia/Shanghai rt=1000000000000 outcome=Success
Apr 7 14:51:33 SWIFT-SAA WS_appsrv[1111]: CEF:0|SWIFT|Alliance Access|7.6.62|ABC-0000|Successful signon|Low|cn1=2222222222 cn1Label=Event Sequence ID cn2=0 cn2Label=Is Alarm cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs2=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs2Label=Correlation ID cs4=5f5f5f5f5f5f5f5f5f5f cs4Label=Session ID cs5=Security cs5Label=Event Type cat=Operator msg=Operator jack : , Locally authenticated - successfully signed on to the terminal '10.100.10.100@222' at 14:51:32 (Asia/Shanghai) using 'Alliance Web Platform'.\nOperator Profiles: profile3 suid=jack dvchost=SWIFT-SAA dvc=10.100.10.100 dvcmac=00:AA:11:BB:22:CC deviceProcessName=WS_appsrv src=10.100.10.100 dtz=Asia/Shanghai rt=1000000000000 outcome=Success
May  15 16:09:07 Swift-SAA swpservice[1111]: CEF:0|SWIFT|Alliance Web Platform|7.6.61|10000|login.success|Low|cn1=2222222222 cn1Label=Event Sequence ID cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs3=10.100.10.100 cs3Label=Proxy IP cat=Security msg=User jack logged in successfully to application group Alliance Web Platform Administration 7.6.61. suid=jack dvchost=Swift-SAA dvc=20.200.20.200 dvcmac=00:AA:11:BB:22:CC deviceProcessName=swpservice src=10.100.10.100 dtz=Asia/Shanghai rt=1000000000000
May  23 16:44:06 Swift-SAA WS_appsrv[1111]: CEF:0|SWIFT|Alliance Access|7.6.62|ABC-0000|Signoff|Low|cn1=2222222222 cn1Label=Event Sequence ID cn2=0 cn2Label=Is Alarm cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs2=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs2Label=Correlation ID cs4=5f5f5f5f5f5f5f5f5f5f cs4Label=Session ID cs5=Security cs5Label=Event Type cat=Operator msg=Operator jack : signed off from the terminal '20.200.20.200'. suid=jack dvchost=Swift-SAA dvc=20.200.20.200 dvcmac=00:AA:11:BB:22:CC deviceProcessName=WS_appsrv src=20.200.20.200 dtz=Asia/Shanghai rt=1000000000000 outcome=Success
May  25 16:08:49 Swift-SAA swpservice[1111]: CEF:0|SWIFT|Alliance Web Platform|7.6.61|10000|logout.success|Low|cn1=2222222222 cn1Label=Event Sequence ID cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs3=10.100.10.100 cs3Label=Proxy IP cat=Security msg=User jack logged out successfully from application group Alliance Web Platform Administration 7.6.61. suid=jack dvchost=Swift-SAA dvc=20.200.20.200 dvcmac=00:AA:11:BB:22:CC deviceProcessName=swpservice src=10.100.10.100 dtz=Asia/Shanghai rt=1000000000000
May  24 14:23:03 Swift-SAA WS_appsrv[1111]: CEF:0|SWIFT|Alliance Access|7.6.62|ABC-0000|Invalid signon attempt|Medium|cn1=2222222222 cn1Label=Event Sequence ID cn2=1 cn2Label=Is Alarm cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs2=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs2Label=Correlation ID cs5=Security cs5Label=Event Type cat=Operator msg=Operator [rose]: Wrong username or password : signon failed. dvchost=Swift-SAA dvc=20.200.20.200 dvcmac=00:AA:11:BB:22:CC deviceProcessName=WS_appsrv src=20.200.20.200 dtz=Asia/Shanghai rt=1000000000000 outcome=Failure
May  18 11:32:23 Swift-SAA swpservice[1111]: CEF:0|SWIFT|Alliance Web Platform|7.6.61|10000|login.failure|Medium|cn1=2222222222 cn1Label=Event Sequence ID cs1=1a1a1a1a-2b2b-3c3c-4d4d-5e5e5e5e5e5e cs1Label=Instance UUID cs3=10.100.10.10 cs3Label=Proxy IP cat=Security msg=User rose failed to login to application group Alliance Access/Entry 7.6.60 to instance SAAProd as Access/Entry Operator.\nInvalid credentials. suid=rose dvchost=Swift-SAA dvc=20.200.20.200 dvcmac=00:AA:11:BB:22:CC deviceProcessName=swpservice src=10.100.10.100 dtz=Asia/Shanghai rt=1000000000000
```

### Example Output

``` json
[{
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  23",
  "eventtime": "16:40:24",
  "product": "Alliance Access",
  "event": "signon",
  "cat": "Operator",
  "auth_method": "Password + TOTP authenticated",
  "suid": "jack",
  "__time__": "1685119719"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "Apr 7",
  "eventtime": "14:51:33",
  "product": "Alliance Access",
  "event": "signon",
  "cat": "Operator",
  "auth_method": "Locally authenticated",
  "suid": "jack",
  "__time__": "1685119719"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  15",
  "eventtime": "16:09:07",
  "product": "Alliance Web Platform",
  "event": "login",
  "cat": "Security",
  "suid": "jack",
  "__time__": "1685119719"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  23",
  "eventtime": "16:44:06",
  "product": "Alliance Access",
  "event": "Signoff",
  "cat": "Operator",
  "suid": "jack",
  "__time__": "1685119719"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  25",
  "eventtime": "16:08:49",
  "product": "Alliance Web Platform",
  "event": "logout",
  "cat": "Security",
  "suid": "jack",
  "__time__": "1685119719"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  24",
  "eventtime": "14:23:03",
  "product": "Alliance Access",
  "event": "Invalid signon attempt",
  "cat": "Operator",
  "suid": "rose",
  "__time__": "1685119719"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  18",
  "eventtime": "11:32:23",
  "product": "Alliance Web Platform",
  "event": "login.failure",
  "cat": "Security",
  "suid": "rose",
  "__time__": "1685119719"
}]
```

## Configuration

``` YAML
enable: true
inputs:
  - Type: file_log
    LogPath: /var/log
    FilePattern: message
processors:
  - Type: processor_filter_regex
    Include:
      content: '^\w+ +\d+ \d+:\d+:\d+ [^|]+?\|SWIFT\|(Alliance Access|Alliance Web Platform)\|.*$'
  - Type: processor_regex
    SourceKey: content
    Regex: '^(\w+ +\d+) (\d+:\d+:\d+) [^|]+?\|[^|]+?\|([^|]+?)\|[^|]+?\|[^|]+?\|([^|]+?)\|[^|]+?\|.*?cat=(.*?) .* \[(.*)\]: .*$'
    Keys:
      - eventdate
      - eventtime
      - product
      - event
      - cat
      - suid
    KeepSource: false
    KeepSourceIfParseError: true
    NoKeyError: false
    NoMatchError: false
  - Type: processor_regex
    SourceKey: content
    Regex: '^(\w+ +\d+) (\d+:\d+:\d+) [^|]+?\|[^|]+?\|([^|]+?)\|[^|]+?\|[^|]+?\|[^|]*(login\.failure|login|Signoff|logout)[^|]*\|[^|]+?\|.*?cat=(.*?) .*?suid=(.*?) .*$'
    Keys:
      - eventdate
      - eventtime
      - product
      - event
      - cat
      - suid
    KeepSource: false
    KeepSourceIfParseError: true
    NoKeyError: false
    NoMatchError: false
  - Type: processor_regex
    SourceKey: content
    Regex: '^(\w+ +\d+) (\d+:\d+:\d+) [^|]+?\|[^|]+?\|([^|]+?)\|[^|]+?\|[^|]+?\|[^|]*(signon)[^|]*\|[^|]+?\|.*?cat=(.*?) .*, (.*) - .*?suid=(.*?) .*'
    Keys:
      - eventdate
      - eventtime
      - product
      - event
      - cat
      - auth_method
      - suid
    KeepSource: false
    KeepSourceIfParseError: true
    NoKeyError: false
    NoMatchError: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
