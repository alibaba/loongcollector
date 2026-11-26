@input
Feature: Docker stdout tag placement
  Both Go stdout inputs preserve their historical content fields by default.
  The event-group format matches native stdout when reading the same fixture.

  @e2e @docker-compose
  Scenario Outline: TestInputDockerStdout<mode>ContentTags
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-case} local config as below
    """
    enable: true
    global:
      <globalTagSetting>
      PipelineMetaTagKey:
        HOST_NAME: ""
        HOST_IP: ""
        HOST_ID: ""
        CLOUD_PROVIDER: ""
        AGENT_TAG: ""
      AgentEnvMetaTagKey: {}
    inputs:
      - Type: service_docker_stdout
        <pluginTagSetting>
        IncludeEnv:
          STDOUT_SWITCH: "true"
        Stdout: true
        Stderr: true
        ExternalEnvTag:
          STDOUT_IMAGE: _image_name_
          STDOUT_SWITCH: fixture_enabled
          MISSING_STDOUT_TEST_ENV: fixture_missing
    """
    When start docker-compose {input_docker_stdout}
    Then there is at least {2} logs
    Then the log fields have exact kv
    """
    - content: "target-stdout"
      _source_: "stdout"
    - content: "target-stderr"
      _source_: "stderr"
    """
    Then the log fields match exactly
    """
    content: 'target-(stdout|stderr)'
    _source_: 'stdout|stderr'
    _time_: '[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?(Z|[+-][0-9]{2}:[0-9]{2})'
    _container_name_: '.*[-_]container[-_]1'
    _container_ip_: '([0-9]{1,3}\.){3}[0-9]{1,3}'
    _image_name_: '.*[-_]container:latest'
    fixture_enabled: 'true'
    fixture_missing: ''
    """
    Then the log group tags match exactly
    """
    __pack_id__: '[^\s]+-[0-9A-F]+'
    """
    Then the log event tags match exactly
    """
    {}
    """

    Examples:
      | mode           | globalTagSetting          | pluginTagSetting            |
      | LegacyDefault  | # UsingOldContentTag      | # UsePipelineEventTags      |
      | LegacyFalse    | UsingOldContentTag: false | # UsePipelineEventTags      |
      | ExplicitLegacy | UsingOldContentTag: false | UsePipelineEventTags: false |
      | GlobalLegacy   | UsingOldContentTag: true  | UsePipelineEventTags: true  |

  @e2e @docker-compose
  Scenario Outline: TestInputDockerStdout<mode>GroupTags
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-case} local config as below
    """
    enable: true
    global:
      <globalTagSetting>
      PipelineMetaTagKey:
        HOST_NAME: ""
        HOST_IP: ""
        HOST_ID: ""
        CLOUD_PROVIDER: ""
        AGENT_TAG: ""
      AgentEnvMetaTagKey: {}
    inputs:
      - Type: service_docker_stdout
        UsePipelineEventTags: true
        IncludeEnv:
          STDOUT_SWITCH: "true"
        Stdout: true
        Stderr: true
        ExternalEnvTag:
          STDOUT_IMAGE: _image_name_
          STDOUT_SWITCH: fixture_enabled
          MISSING_STDOUT_TEST_ENV: fixture_missing
    """
    When start docker-compose {input_docker_stdout}
    Then there is at least {2} logs
    Then the log fields have exact kv
    """
    - content: "target-stdout"
      _source_: "stdout"
    - content: "target-stderr"
      _source_: "stderr"
    """
    Then the log fields match exactly
    """
    content: 'target-(stdout|stderr)'
    _source_: 'stdout|stderr'
    _time_: '[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?(Z|[+-][0-9]{2}:[0-9]{2})'
    """
    Then the log group tags match exactly
    """
    __pack_id__: '[^\s]+-[0-9A-F]+'
    _container_name_: '.*[-_]container[-_]1'
    _container_ip_: '([0-9]{1,3}\.){3}[0-9]{1,3}'
    _image_name_: 'overridden-image'
    fixture_enabled: 'true'
    """
    Then the log event tags match exactly
    """
    {}
    """

    Examples:
      | mode     | globalTagSetting          |
      | Frontend | # UsingOldContentTag      |
      | Explicit | UsingOldContentTag: false |

  @e2e @docker-compose
  Scenario Outline: TestInputDockerRawStdout<mode>ContentTags
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-case} local config as below
    """
    enable: true
    global:
      <globalTagSetting>
      PipelineMetaTagKey:
        HOST_NAME: ""
        HOST_IP: ""
        HOST_ID: ""
        CLOUD_PROVIDER: ""
        AGENT_TAG: ""
      AgentEnvMetaTagKey: {}
    inputs:
      - Type: service_docker_stdout_raw
        <pluginTagSetting>
        IncludeEnv:
          STDOUT_SWITCH: "true"
        Stdout: true
        Stderr: true
        ExternalEnvTag:
          STDOUT_IMAGE: _image_name_
          STDOUT_SWITCH: fixture_enabled
          MISSING_STDOUT_TEST_ENV: fixture_missing
    """
    When start docker-compose {input_docker_stdout}
    Then there is at least {2} logs
    Then the log fields have exact kv
    """
    - content: "target-stdout"
      _source_: "stdout"
    - content: "target-stderr"
      _source_: "stderr"
    """
    Then the log fields match exactly
    """
    content: 'target-(stdout|stderr)'
    _source_: 'stdout|stderr'
    _time_: '[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?(Z|[+-][0-9]{2}:[0-9]{2})'
    _container_name_: '.*[-_]container[-_]1'
    _container_ip_: '([0-9]{1,3}\.){3}[0-9]{1,3}'
    _image_name_: '.*[-_]container:latest'
    fixture_enabled: 'true'
    fixture_missing: ''
    """
    Then the log group tags match exactly
    """
    __pack_id__: '[^\s]+-[0-9A-F]+'
    """
    Then the log event tags match exactly
    """
    {}
    """

    Examples:
      | mode           | globalTagSetting          | pluginTagSetting            |
      | LegacyDefault  | # UsingOldContentTag      | # UsePipelineEventTags      |
      | LegacyFalse    | UsingOldContentTag: false | # UsePipelineEventTags      |
      | ExplicitLegacy | UsingOldContentTag: false | UsePipelineEventTags: false |
      | GlobalLegacy   | UsingOldContentTag: true  | UsePipelineEventTags: true  |

  @e2e @docker-compose
  Scenario Outline: TestInputDockerRawStdout<mode>GroupTags
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-case} local config as below
    """
    enable: true
    global:
      <globalTagSetting>
      PipelineMetaTagKey:
        HOST_NAME: ""
        HOST_IP: ""
        HOST_ID: ""
        CLOUD_PROVIDER: ""
        AGENT_TAG: ""
      AgentEnvMetaTagKey: {}
    inputs:
      - Type: service_docker_stdout_raw
        UsePipelineEventTags: true
        IncludeEnv:
          STDOUT_SWITCH: "true"
        Stdout: true
        Stderr: true
        ExternalEnvTag:
          STDOUT_IMAGE: _image_name_
          STDOUT_SWITCH: fixture_enabled
          MISSING_STDOUT_TEST_ENV: fixture_missing
    """
    When start docker-compose {input_docker_stdout}
    Then there is at least {2} logs
    Then the log fields have exact kv
    """
    - content: "target-stdout"
      _source_: "stdout"
    - content: "target-stderr"
      _source_: "stderr"
    """
    Then the log fields match exactly
    """
    content: 'target-(stdout|stderr)'
    _source_: 'stdout|stderr'
    _time_: '[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?(Z|[+-][0-9]{2}:[0-9]{2})'
    """
    Then the log group tags match exactly
    """
    __pack_id__: '[^\s]+-[0-9A-F]+'
    _container_name_: '.*[-_]container[-_]1'
    _container_ip_: '([0-9]{1,3}\.){3}[0-9]{1,3}'
    _image_name_: 'overridden-image'
    fixture_enabled: 'true'
    """
    Then the log event tags match exactly
    """
    {}
    """

    Examples:
      | mode     | globalTagSetting          |
      | Frontend | # UsingOldContentTag      |
      | Explicit | UsingOldContentTag: false |

  @e2e @docker-compose
  Scenario: TestInputContainerStdioGroupTags
    Given {docker-compose} environment
    Given subcribe data from {grpc} with config
    """
    """
    Given {input-docker-stdout-case} local config as below
    """
    enable: true
    global:
      PipelineMetaTagKey:
        HOST_NAME: ""
        HOST_IP: ""
        HOST_ID: ""
        CLOUD_PROVIDER: ""
        AGENT_TAG: ""
      AgentEnvMetaTagKey: {}
    inputs:
      - Type: input_container_stdio
        ContainerFilters:
          IncludeEnv:
            STDOUT_SWITCH: "true"
        IgnoringStdout: false
        IgnoringStderr: false
        ExternalEnvTag:
          STDOUT_IMAGE: _image_name_
          STDOUT_SWITCH: fixture_enabled
          MISSING_STDOUT_TEST_ENV: fixture_missing
    """
    When start docker-compose {input_docker_stdout}
    Then there is at least {2} logs
    Then the log fields have exact kv
    """
    - content: "target-stdout"
      _source_: "stdout"
    - content: "target-stderr"
      _source_: "stderr"
    """
    Then the log fields match exactly
    """
    content: 'target-(stdout|stderr)'
    _source_: 'stdout|stderr'
    _time_: '[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?(Z|[+-][0-9]{2}:[0-9]{2})'
    """
    Then the log group tags match exactly
    """
    __pack_id__: '[^\s]+-[0-9A-F]+'
    _container_name_: '.*[-_]container[-_]1'
    _container_ip_: '([0-9]{1,3}\.){3}[0-9]{1,3}'
    _image_name_: 'overridden-image'
    fixture_enabled: 'true'
    """
    Then the log event tags match exactly
    """
    {}
    """
