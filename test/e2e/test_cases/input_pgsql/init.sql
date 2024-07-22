-- Copyright 2022 iLogtail Authors
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

CREATE TABLE IF NOT EXISTS specialalarmtest (
    id BIGSERIAL NOT NULL, 
	time TIMESTAMP NOT NULL, 
	alarmtype varchar(64) NOT NULL, 
	ip varchar(16) NOT NULL, 
	COUNT INT NOT NULL, 
	PRIMARY KEY (id)
);

insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 0);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 1);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 2);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 3);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 4);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 5);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 6);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 7);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 8);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 9);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 10);
insert into specialalarmtest (time, alarmtype, ip, count) values(now(), 'NO_ALARM', '10.10.***.***', 11);