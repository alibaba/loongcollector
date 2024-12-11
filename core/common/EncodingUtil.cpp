// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/EncodingUtil.h"

#include <sstream>

using namespace std;

namespace logtail {
    
static void Base64Encoding(istream& is, ostream& os, char makeupChar, const char* alphabet) {
    int out[4];
    int remain = 0;
    while (!is.eof()) {
        int byte1 = is.get();
        if (byte1 < 0) {
            break;
        }
        int byte2 = is.get();
        int byte3;
        if (byte2 < 0) {
            byte2 = 0;
            byte3 = 0;
            remain = 1;
        } else {
            byte3 = is.get();
            if (byte3 < 0) {
                byte3 = 0;
                remain = 2;
            }
        }
        out[0] = static_cast<unsigned>(byte1) >> 2;
        out[1] = ((byte1 & 0x03) << 4) + (static_cast<unsigned>(byte2) >> 4);
        out[2] = ((byte2 & 0x0F) << 2) + (static_cast<unsigned>(byte3) >> 6);
        out[3] = byte3 & 0x3F;

        if (remain == 1) {
            os.put(out[0] = alphabet[out[0]]);
            os.put(out[1] = alphabet[out[1]]);
            os.put(makeupChar);
            os.put(makeupChar);
        } else if (remain == 2) {
            os.put(out[0] = alphabet[out[0]]);
            os.put(out[1] = alphabet[out[1]]);
            os.put(out[2] = alphabet[out[2]]);
            os.put(makeupChar);
        } else {
            os.put(out[0] = alphabet[out[0]]);
            os.put(out[1] = alphabet[out[1]]);
            os.put(out[2] = alphabet[out[2]]);
            os.put(out[3] = alphabet[out[3]]);
        }
    }
}

string Base64Enconde(const string& message) {
    istringstream iss(message);
    ostringstream oss;
    Base64Encoding(iss, oss, '=', "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    return oss.str();
}

} // namespace logtail
