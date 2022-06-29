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

#ifndef __LOG_LOGTAIL_DELIMITER_MODE_FSM_Parser_H__
#define __LOG_LOGTAIL_DELIMITER_MODE_FSM_Parser_H__

#include <string>
#include <vector>

/*
 * FSM(finite state machine) for csv parser
 *
 *  ------------------- ------------------- ------------------- ------------------- -------------------
 * |                   |  char comma       |  char quote       |  char data        |  eof              |
 *  ------------------- ------------------- ------------------- ------------------- -------------------
 * |state initial      |push_back, clear   |state quote        |state data, append |  push_back        |
 *  ------------------- ------------------- ------------------- ------------------- -------------------
 * |state quote        |append             |state double_quote |append             |  error            |
 *  ------------------- ------------------- ------------------- ------------------- -------------------
 * |state data         |state initial,     |error              |append             |  push_back        |
 * |                   |push_back, clear   |                   |                   |                   |
 *  ------------------- ------------------- ------------------- ------------------- -------------------
 * |state double_quote |state initial,     |state quote, append|error              |                   |
 * |                   |push_back, clear   |                   |                   |                   |
 *  ------------------- ------------------- ------------------- ------------------- -------------------
 */

namespace logtail {

enum DelimiterModeState { STATE_INITIAL = 0, STATE_QUOTE, STATE_DATA, STATE_DOUBLE_QUOTE };

struct DelimiterModeFsm {
    DelimiterModeState currentState;
    std::string field;

    DelimiterModeFsm(DelimiterModeState currentState, const std::string& field)
        : currentState(currentState), field(field) {}
};

class DelimiterModeFsmParser {
public:
    DelimiterModeFsmParser(char quote, char separator);
    ~DelimiterModeFsmParser();

public:
    static bool HandleSeparator(char ch, DelimiterModeFsm& fsm, std::vector<std::string>& columnValues);
    static bool HandleQuote(char ch, DelimiterModeFsm& fsm);
    static bool HandleData(char ch, DelimiterModeFsm& fsm);
    static bool HandleEOF(DelimiterModeFsm& fsm, std::vector<std::string>& columnValues);

public:
    bool ParseDelimiterLine(const char* buffer, int begin, int end, std::vector<std::string>& columnValues);

private:
    const char quote;
    const char separator;
};

} // namespace logtail

#endif //__LOG_LOGTAIL_DELIMITER_MODE_FSM_Parser_H__
