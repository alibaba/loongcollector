@processor
Feature: processor v2 pipeline
  Test migrated processors on a pure v2 log pipeline.
  metric_debug_file (Log input, v2 Read) -> processors -> flusher_grpc (FlusherV2.Export).
  Each scenario feeds a crafted line, runs the processor(s) under test, and asserts the
  converted contents that reach the gRPC sink, so the v2 Process path is really exercised
  (not a no-op passthrough over Metric events).

  @e2e @docker-compose
  Scenario: TestProcessorV2SplitKeyValueAddFieldsReplaceAppend
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector container mount {./content_kv.log} to {/root/test/content_kv.log}
    Given {processor-v2-kv-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/content_kv.log
    processors:
      - Type: processor_split_key_value
        SourceKey: content
        Delimiter: " "
        Separator: "="
      - Type: processor_add_fields
        Fields:
          source: e2e_v2
      - Type: processor_string_replace
        SourceKey: msg
        Method: const
        Match: hello
        ReplaceString: world
      - Type: processor_appender
        Key: level
        Value: "-checked"
    """
    When start docker-compose {processor_v2}
    Then there is at least {5} logs
    Then the log fields match kv
    """
    level: "INFO-checked"
    msg: "world"
    status: "200"
    source: "e2e_v2"
    """

  @e2e @docker-compose
  Scenario: TestProcessorV2RegexBase64MD5
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector container mount {./regex_hash.log} to {/root/test/regex_hash.log}
    Given {processor-v2-regex-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/regex_hash.log
    processors:
      - Type: processor_regex
        SourceKey: content
        Regex: "(\\w+) (\\w+)"
        Keys:
          - user
          - role
        KeepSource: true
      - Type: processor_base64_encoding
        SourceKey: user
        NewKey: user_b64
      - Type: processor_base64_decoding
        SourceKey: user_b64
        NewKey: user_decoded
      - Type: processor_md5
        SourceKey: role
        MD5Key: role_md5
    """
    When start docker-compose {processor_v2}
    Then there is at least {5} logs
    Then the log fields match kv
    """
    user: "alice"
    role: "admin"
    user_b64: "YWxpY2U="
    user_decoded: "alice"
    role_md5: "21232f297a57a5a743894a0e4a801fc3"
    """

  @e2e @docker-compose
  Scenario: TestProcessorV2SplitCharDictMap
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector container mount {./split_pipe.log} to {/root/test/split_pipe.log}
    Given {processor-v2-splitchar-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/split_pipe.log
    processors:
      - Type: processor_split_char
        SourceKey: content
        SplitSep: "|"
        SplitKeys:
          - code
          - token
          - name
      - Type: processor_dict_map
        SourceKey: code
        DestKey: code_label
        Mode: overwrite
        MapDict:
          "1": ok
    """
    When start docker-compose {processor_v2}
    Then there is at least {5} logs
    Then the log fields match kv
    """
    code: "1"
    token: "secret"
    name: "abc"
    code_label: "ok"
    """

  @e2e @docker-compose
  Scenario: TestProcessorV2Csv
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector container mount {./csv_line.log} to {/root/test/csv_line.log}
    Given {processor-v2-csv-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/csv_line.log
    processors:
      - Type: processor_csv
        SourceKey: content
        SplitKeys:
          - c1
          - c2
          - c3
    """
    When start docker-compose {processor_v2}
    Then there is at least {5} logs
    Then the log fields match kv
    """
    c1: "foo"
    c2: "bar"
    c3: "baz"
    """

  @e2e @docker-compose
  Scenario: TestProcessorV2Grok
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given loongcollector container mount {./grok_line.log} to {/root/test/grok_line.log}
    Given {processor-v2-grok-case} local config as below
    """
    enable: true
    global:
      StructureType: v2
      InputIntervalMs: 1000
    inputs:
      - Type: metric_debug_file
        InputFilePath: /root/test/grok_line.log
    processors:
      - Type: processor_grok
        SourceKey: content
        Match:
          - "id=%{NUMBER:gid} name=%{WORD:gname}"
    """
    When start docker-compose {processor_v2}
    Then there is at least {5} logs
    Then the log fields match kv
    """
    gid: "42"
    gname: "bob"
    """
