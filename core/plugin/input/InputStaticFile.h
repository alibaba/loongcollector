/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include "config/PipelineConfig.h"
#include "file_server/AdhocFileManager.h"

namespace logtail {

class InputStaticFile {
private:
    void GetStaticFileList();
    void SortFileList();

    std::string mJobName;
    AdhocFileManager* mAdhocFileManager;
    std::vector<StaticFile> mFileList;
    
public:
    InputStaticFile(/* args */);
    ~InputStaticFile();

    void Init(PipelineConfig &&config);
    void Start();
    void Stop(bool isRemoving);
};

}