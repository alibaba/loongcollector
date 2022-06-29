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

#ifndef _LOG_DEVINODE_H__
#define _LOG_DEVINODE_H__

#include <string>

namespace logtail {

const static uint64_t NO_BLOCK_INODE = 0;
const static uint64_t NO_BLOCK_DEV = 0;

class DevInode {
public:
    DevInode() : dev(NO_BLOCK_INODE), inode(NO_BLOCK_DEV) {}
    DevInode(uint64_t d, uint64_t i) : dev(d), inode(i) {}
    DevInode(const DevInode& devInode) : dev(devInode.dev), inode(devInode.inode) {}

    DevInode& operator=(const DevInode& devInode) {
        dev = devInode.dev;
        inode = devInode.inode;
        return *this;
    }
    bool IsValid() const { return !(dev == NO_BLOCK_DEV && inode == NO_BLOCK_INODE); }
    bool operator<(const DevInode& o) const {
        if (dev < o.dev) {
            return true;
        } else if (dev == o.dev) {
            return inode < o.inode;
        } else {
            return false;
        }
    }
    bool operator==(const DevInode& o) const { return o.inode == inode && o.dev == dev; }
    bool operator!=(const DevInode& o) const { return !(this->operator==(o)); }
    uint64_t dev;
    uint64_t inode;
};

struct DevInodeHash {
    size_t operator()(const DevInode& key) const { return (key.dev << 32 | key.dev >> 32) ^ key.inode; }
};

struct DevInodeEqual {
    bool operator()(const DevInode& l, const DevInode& r) const { return l.inode == r.inode && l.dev == r.dev; }
};

uint64_t GetFileInode(const std::string& filePath);
DevInode GetFileDevInode(const std::string& filePath);

} // namespace logtail

#endif //_LOG_DEVINODE_H__