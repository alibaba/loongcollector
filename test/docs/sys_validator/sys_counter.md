# sys_counter
## Description
this is a log field validator to check received log from subscriber
## Config
|  field   |   type   |   description   |
| ---- | ---- | ---- |
|expect_received_minimum_log_num|int|the expected minimum received log number|
|expect_received_minimum_log_group_num|int|the expected minimum received log group number|
|expect_minimum_raw_log_num|int|the expected minimum raw log number|
|expect_minimum_processed_log_num|int|the expected minimum processed log number|
|expect_minimum_flush_log_num|int|the expected minimum flushed log number|
|expect_minimum_flush_log_group_num|int|the expected minimum flushed log group number|
|expect_equal_raw_log|bool|whether the received log number equal to the raw log number|
|expect_equal_processed_log|bool|whether the received log number equal to the processed log number|
|expect_equal_flush_log|bool|whether the received log number equal to the flush log number|