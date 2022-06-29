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

#include <stdio.h>
#include <string.h>

extern "C" {
int CNaiD5EF8JV7EF9A(char*);
}

int main() {
    printf("start get uuid \n\n");

    char buff[128];
    memset(buff, 0, 128);
    if (CNaiD5EF8JV7EF9A(buff) < 0) {
        printf("get uuid error \n");
    }
    printf("uuid is %s \n", buff);
    return 0;
}
