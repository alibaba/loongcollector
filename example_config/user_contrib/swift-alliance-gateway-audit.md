# SWIFT Alliance Gateway(SAG) Audit

## Description

The following configuration parses SWIFT Alliance Gateway logs to identify user login behaviors.

SWIFT (Society for Worldwide Interbank Financial Telecommunication) is a global financial messaging network that enables banks and financial institutions to securely exchange electronic messages and information. SWIFT Alliance Gateway is a software solution provided by SWIFT that allows banks and financial institutions to connect to the SWIFT network, manage their messaging traffic, and exchange financial information with other institutions.

### Example Input

``` plain
May  25 16:11:19 Swift-SAG sag_control[111111]: Component: Sag:System|Event Number: 100|Event Severity: Information|Event Class: Security|Description: Operator 'jack' logged in (session ID 100000000000).
May  25 16:10:57 Swift-SAG sag_control[111111]: Component: Sag:System|Event Number: 100|Event Severity: Warning|Event Class: Security|Description: Failed to log in operator 'rose'.  The TOTP provided is invalid
May  18 11:06:25 Swift-SAG sag_control[111111]: Component: Sag:System|Event Number: 100|Event Severity: Warning|Event Class: Security|Description: Operator 'rose' does not exist
May 9 18:06:22 Swift-SAG sag_control[111111]: Component: Sag:System|Event Number: 100|Event Severity: Warning|Event Class: Security|Description: Failed to log in operator 'rose'.  The password provided is invalid
```

### Example Output

``` json
[{
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  25",
  "eventtime": "16:11:19",
  "username": "jack",
  "event": "logged in",
  "__time__": "1685121648"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  25",
  "eventtime": "16:10:57",
  "username": "rose",
  "event": "The TOTP provided is invalid",
  "__time__": "1685121648"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May  18",
  "eventtime": "11:06:25",
  "username": "rose",
  "event": "does not exist",
  "__time__": "1685121648"
}, {
  "__tag__:__path__": "/var/log/message",
  "eventdate": "May 9",
  "eventtime": "18:06:22",
  "username": "rose",
  "event": "The password provided is invalid",
  "__time__": "1685121648"
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
      content: '^\w+ +\d+ \d+:\d+:\d+ [^ ]+ sag_control.*$'
  - Type: processor_regex
    SourceKey: content
    Regex: '^(\w+ +\d+) (\d+:\d+:\d+) [^|]+?\|[^|]+?\|[^|]+?\|[^|]+?\|.*''(.*)''[. ]*([\w+ ]*[\w+]+).*$'
    Keys:
      - eventdate
      - eventtime
      - username
      - event
    KeepSource: false
    KeepSourceIfParseError: true
    NoKeyError: false
    NoMatchError: true
flushers:
  - Type: flusher_stdout
    OnlyStdout: true
```
