-- Copyright 2024 iLogtail Authors
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--      http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

-- Create database
CREATE DATABASE IF NOT EXISTS test_db;

-- Create table for E2E test
CREATE TABLE IF NOT EXISTS test_db.e2e_logs (
    content VARCHAR(65533),
    value VARCHAR(65533),
    __time__ BIGINT
)
DUPLICATE KEY(content)
DISTRIBUTED BY HASH(content) BUCKETS 1
PROPERTIES (
    "replication_num" = "1"
);

