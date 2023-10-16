// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "LogtailRuntimePluginAdapter.h"
#include <stdio.h>

volatile UpdateLogtailConfigFun gAdapterUpdateLogtailConfigFun = NULL;

void RegisterLogtailCallBack(UpdateLogtailConfigFun updateLogtailConfigFun) {
    printf("register fun %p \n", updateLogtailConfigFun);
    gAdapterUpdateLogtailConfigFun = updateLogtailConfigFun;
    printf("after register fun %p \n", gAdapterUpdateLogtailConfigFun);
}

int UpdateLogtailConfig(const char* configJSON, int configJSONSize) {
    printf("UpdateLogtailConfig %p\n", gAdapterUpdateLogtailConfigFun);
    if (gAdapterUpdateLogtailConfigFun != NULL)
        return gAdapterUpdateLogtailConfigFun(configJSON, configJSONSize);
    return 255;
}
