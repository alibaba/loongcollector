@input
Feature: input http
  Test input http

  @e2e @docker-compose
  Scenario: TestInputHTTP
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-http-case} local config as below
    """
    enable: true
    inputs:
      - Type: metric_http
        IntervalMs: 1000
        Addresses: 
          - http://www.baidu.com
        IncludeBody: true
    processors:
      - Type: processor_anchor
        SourceKey: content
        NoAnchorError: true
        Anchors:
          - Start: ""
            Stop: ""
            FieldName: body
            FieldType: json
            ExpondJson: true
    """
    When start docker-compose {input_http}
    Then there is at least {4} logs
    Then the log fields match as below
    """
    - _method_
    - _address_
    - _result_
    - _http_response_code_
    - _response_time_ms_
    """
    Then the log fields match kv
    """
    _address_: "http://www.baidu.com"
    _method_: "GET"
    _result_: "success"
    _http_response_code_: "200"
    """
  
