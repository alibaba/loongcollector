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

IF NOT EXISTS(SELECT * FROM sys.databases WHERE name = 'LogtailTest')
BEGIN
    CREATE DATABASE [LogtailTest]
END
GO
USE [LogtailTest]
GO

IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='LogtailTestTable' and xtype='U')
BEGIN
    CREATE TABLE LogtailTestTable (
        id INT PRIMARY KEY IDENTITY (1, 1),
        name NVARCHAR(50), 
		quantity INT
    )
END
GO

INSERT INTO LogtailTestTable (name, quantity) values('banana', 1);
INSERT INTO LogtailTestTable (name, quantity) values('banana', 2);
INSERT INTO LogtailTestTable (name, quantity) values('banana', 3);
INSERT INTO LogtailTestTable (name, quantity) values('banana', 4);
SELECT * FROM LogtailTestTable;
GO