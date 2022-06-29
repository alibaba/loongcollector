/*
 * Copyright 2022 iLogtail Authors
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

#ifndef __OAS_SHENNONG_FILE_ENCRYPTION_H__
#define __OAS_SHENNONG_FILE_ENCRYPTION_H__

#include <string>
#include <unordered_map>
#include <map>
#include "Flags.h"

namespace logtail {
class FileEncryption {
public:
    static FileEncryption* GetInstance() {
        static FileEncryption* fileEncryptionPtr = new FileEncryption();
        return fileEncryptionPtr;
    };
    static bool CheckHeader(const std::string& filename, std::unordered_map<std::string, std::string>& kvMap);
    bool Encrypt(const char* src, int32_t srcLength, char*& des, int32_t& desLength, int32_t version = 0);
    bool Decrypt(const char* src, int32_t srcLength, char* des, int32_t desLength, int32_t version);
    int32_t GetDefaultKeyVersion() { return mDefaultKey->mVersion; }

private:
    FileEncryption();
    ~FileEncryption();
    void SetDefaultKey();
    void LoadKeyInfo();

public:
    struct KeyInfo {
        KeyInfo(std::string key, int32_t version) {
            mBlockBytes = (int32_t)key.size();
            mVersion = version;
            mKey = key;
        }

        void Reset() {
            mKey = "";
            mBlockBytes = 0;
            mVersion = 0;
        }

        std::string mKey;
        int32_t mBlockBytes; //equal to mKey.size()
        int32_t mVersion;
    };

private:
    std::map<int32_t, KeyInfo*> mKeyMap; //version and its key
    KeyInfo* mDefaultKey; // the latest version key

    static const int32_t FIRST_KEY_VERSION;
    static const std::string FIRST_KEY_VALUE;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
#endif
};
} // namespace logtail

DECLARE_FLAG_STRING(file_encryption_magic_number);
DECLARE_FLAG_INT32(file_encryption_header_length);
DECLARE_FLAG_STRING(file_encryption_field_key_version);
DECLARE_FLAG_STRING(file_encryption_field_splitter);
DECLARE_FLAG_STRING(file_encryption_key_value_splitter);

#endif // __OAS_SHENNONG_FILE_ENCRYPTION_H__
