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

#include "FileEncryption.h"
#include <time.h>
#include <stdlib.h>
#include "StringTools.h"
#include "FileSystemUtil.h"
#include "logger/Logger.h"

using namespace std;

DEFINE_FLAG_STRING(file_encryption_magic_number,
                   "indicate it is an logtail encryption file",
                   "L\1O\1G\1T\1A\1I\1L\1\1E\1N\1C\1R\1Y\1P\1T");
DEFINE_FLAG_INT32(file_encryption_header_length, "the encryption file header length in bytes", 128);
DEFINE_FLAG_STRING(file_encryption_field_key_version, "field name: version of key", "key_version");
DEFINE_FLAG_STRING(file_encryption_field_splitter, "splitter between two fields", "\2");
DEFINE_FLAG_STRING(file_encryption_key_value_splitter, "splitter between key and value in one field", ":");

namespace logtail {
/*
  set all key-version pair here:
  version: incremental integer, [1, 2^32)
  key: consists of several chars, default is a md5 string
*/
const int32_t FileEncryption::FIRST_KEY_VERSION = 1;
const string FileEncryption::FIRST_KEY_VALUE = "b394d709b96949bb3ca6f7b2f2d9a493";

bool FileEncryption::CheckHeader(const std::string& filename, std::unordered_map<std::string, std::string>& kvMap) {
    FILE* pFile = FileReadOnlyOpen(filename.c_str(), "r");
    if (NULL == pFile) {
        LOG_ERROR(sLogger, ("fopen error, filename", filename));
        return false;
    }
    char* header = new char[INT32_FLAG(file_encryption_header_length) + 1];
    int32_t nbytes = (int32_t)fread(header, 1, INT32_FLAG(file_encryption_header_length), pFile);
    fclose(pFile);
    if ((nbytes != INT32_FLAG(file_encryption_header_length))
        || (strstr(header, STRING_FLAG(file_encryption_magic_number).c_str()) != header)) {
        delete[] header;
        return false;
    }

    header[INT32_FLAG(file_encryption_header_length)] = '\0';
    kvMap.clear();
    string reserve = string(header + STRING_FLAG(file_encryption_magic_number).size());
    vector<string> fields = StringSpliter(reserve, STRING_FLAG(file_encryption_field_splitter));
    for (vector<string>::iterator iter = fields.begin(); iter != fields.end(); ++iter) {
        size_t pos = iter->find(STRING_FLAG(file_encryption_key_value_splitter));
        if (pos != string::npos)
            kvMap.insert(pair<string, string>(iter->substr(0, pos), iter->substr(pos + 1)));
    }
    delete[] header;
    return true;
}

FileEncryption::FileEncryption() {
    mKeyMap.clear();
    LoadKeyInfo();
    SetDefaultKey();
    srand(time(NULL));
}

void FileEncryption::LoadKeyInfo() {
    // add new (version, key) pair here
    KeyInfo* keyV1 = new KeyInfo(FIRST_KEY_VALUE, FIRST_KEY_VERSION);
    mKeyMap.insert(pair<int32_t, KeyInfo*>(keyV1->mVersion, keyV1));
}

void FileEncryption::SetDefaultKey() {
    // use the latest version as default
    mDefaultKey = mKeyMap.begin()->second;
    map<int32_t, KeyInfo*>::iterator iter = mKeyMap.begin();
    while (++iter != mKeyMap.end()) {
        if (mDefaultKey->mVersion < iter->second->mVersion)
            mDefaultKey = iter->second;
    }
}

FileEncryption::~FileEncryption() {
    for (map<int32_t, KeyInfo*>::iterator iter = mKeyMap.begin(); iter != mKeyMap.end(); ++iter) {
        delete iter->second;
    }
    mKeyMap.clear();
}

// if encrypt success, must release memory(des) after call this function
bool FileEncryption::Encrypt(const char* src, int32_t srcLength, char*& des, int32_t& desLength, int32_t version) {
    KeyInfo* encryptKey;
    if (mKeyMap.find(version) != mKeyMap.end())
        encryptKey = mKeyMap[version];
    else if (version == 0)
        encryptKey = mDefaultKey;
    else {
        LOG_ERROR(sLogger, ("key_version for encrypt is invalid", version));
        return false;
    }
    desLength = 0;
    if (srcLength == 0)
        return false;
    int32_t blockCount = srcLength / encryptKey->mBlockBytes;
    if ((srcLength % encryptKey->mBlockBytes) != 0) {
        blockCount += 1;
    }
    desLength = blockCount * encryptKey->mBlockBytes;
    des = new char[desLength];
    for (int32_t pos = 0; pos < desLength; ++pos) {
        int32_t byteIdx = pos % encryptKey->mBlockBytes;
        if (pos < srcLength) {
            des[pos] = src[pos] ^ encryptKey->mKey[byteIdx];
        } else {
            des[pos] = char((rand() % 94) + 33) ^ encryptKey->mKey[byteIdx];
        }
    }
    return true;
}

bool FileEncryption::Decrypt(const char* src, int32_t srcLength, char* des, int32_t desLength, int32_t version) {
    KeyInfo* decryptKey;
    if (mKeyMap.find(version) != mKeyMap.end())
        decryptKey = mKeyMap[version];
    else {
        LOG_ERROR(sLogger, ("key_version for decrypt is invalid", version));
        return false;
    }

    if (srcLength == 0 || desLength > srcLength) {
        LOG_ERROR(sLogger, ("decrypt error, srcLength:", srcLength)("desLength", desLength));
        return false;
    }
    if (srcLength % decryptKey->mBlockBytes != 0) {
        LOG_ERROR(sLogger, ("decrypt error, key_version:", decryptKey->mVersion));
        return false;
    }
    for (int32_t pos = 0; pos < desLength; ++pos) {
        int32_t byteIdx = pos % decryptKey->mBlockBytes;
        des[pos] = src[pos] ^ decryptKey->mKey[byteIdx];
    }
    return true;
}

} // namespace logtail
