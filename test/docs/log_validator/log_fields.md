# log_fields
## Description
this is a log field validator to check received log from subscriber
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|expect_log_fields|[]string|the expected log fields mapping|null|
|expect_log_field_num|int|the expected log fields number|0|
|expect_log_tags|[]string|the expected log tags mapping|null|
|expect_tag_num|int|the expected log tags number|0|
|expect_log_category|string|the expected log category|""|
|expect_log_topic|string|the expected log topic|""|
|expect_log_source|string|the expected log source|""|