#!/bin/bash

# Copyright 2021 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

rm -rf ./github.com
protoc --go_out=. --proto_path=./proto/ --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent/*.proto --go-grpc_out=.
protoc --go_out=. --proto_path=./proto/ --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent-v2/*.proto --go-grpc_out=.
protoc --go_out=. --proto_path=./proto/ --go-grpc_opt=require_unimplemented_servers=false ./proto/register/*.proto --go-grpc_out=.
protoc --go_out=. --proto_path=./proto/ --go-grpc_opt=require_unimplemented_servers=false ./proto/common/*.proto --go-grpc_out=.

rm -rf ./skywalking
mv ./github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking ./
rm -rf ./github.com
