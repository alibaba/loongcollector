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

 rm -fr ./github.com

 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent/CLRMetric.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent/ConfigurationDiscoveryService.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent/JVMMetric.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent/Meter.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/language-agent/Tracing.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/profile/Profile.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/management/Management.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/common/Common.proto --proto_path=./proto
 protoc --go_out=. --go-grpc_out=. --go-grpc_opt=require_unimplemented_servers=false ./proto/logging/Logging.proto --proto_path=./proto

 rm -rf ./skywalking
 mv ./github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking ./
 rm -fr ./github.com
