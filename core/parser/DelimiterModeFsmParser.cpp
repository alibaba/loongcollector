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

#include "DelimiterModeFsmParser.h"

namespace logtail {

DelimiterModeFsmParser::DelimiterModeFsmParser(char quote, char separator) : quote(quote), separator(separator) {
}

DelimiterModeFsmParser::~DelimiterModeFsmParser() {
}

bool DelimiterModeFsmParser::HandleSeparator(char ch, DelimiterModeFsm& fsm, std::vector<std::string>& columnValues) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
            columnValues.push_back(fsm.field);
            fsm.field.clear();
            return true;
        case STATE_QUOTE:
            fsm.field.append(1, ch);
            return true;
        case STATE_DATA:
            fsm.currentState = STATE_INITIAL;
            columnValues.push_back(fsm.field);
            fsm.field.clear();
            return true;
        case STATE_DOUBLE_QUOTE:
            fsm.currentState = STATE_INITIAL;
            columnValues.push_back(fsm.field);
            fsm.field.clear();
            return true;
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::HandleSeparator(const char* ch,
                                             const char quote,
                                             int& fieldStart,
                                             int& fieldEnd,
                                             DelimiterModeFsm& fsm,
                                             std::vector<StringView>& columnValues,
                                             int& doubleQuoteNum,
                                             LogEvent& event) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
            AddFieldWithUnQuote(ch, quote, fieldStart, fieldEnd, columnValues, doubleQuoteNum, event);
            fieldStart = ++fieldEnd;
            return true;
        case STATE_QUOTE:
            fieldEnd++;
            return true;
        case STATE_DATA:
            fsm.currentState = STATE_INITIAL;
            AddFieldWithUnQuote(ch, quote, fieldStart, fieldEnd, columnValues, doubleQuoteNum, event);
            fieldStart = ++fieldEnd;
            return true;
        case STATE_DOUBLE_QUOTE:
            fsm.currentState = STATE_INITIAL;
            doubleQuoteNum--;
            AddFieldWithUnQuote(ch, quote, fieldStart, fieldEnd, columnValues, doubleQuoteNum, event);
            // Skip the quote
            fieldEnd += 2;
            fieldStart = fieldEnd;
            return true;
        default:
            return false;
    }
}

void DelimiterModeFsmParser::AddFieldWithUnQuote(const char* ch,
                                                 const char quote,
                                                 int& fieldStart,
                                                 int& fieldEnd,
                                                 std::vector<StringView>& columnValues,
                                                 int& doubleQuoteNum,
                                                 LogEvent& event) {
    if (doubleQuoteNum == 0) {
        columnValues.emplace_back(ch + fieldStart, fieldEnd - fieldStart);
        return;
    }
    StringBuffer sb = event.GetSourceBuffer()->AllocateStringBuffer(fieldEnd - fieldStart - doubleQuoteNum);
    char* field = sb.data;
    int j = 0;
    for (int i = fieldStart; i < fieldEnd; ++i) {
        if (ch[i] == quote) {
            if (i + 1 < fieldEnd && ch[i + 1] == quote) {
                field[j] = quote;
                ++i;
                ++j;
                continue;
            }
        } else {
            field[j] = ch[i];
            ++j;
        }
    }

    columnValues.emplace_back(field, fieldEnd - fieldStart - doubleQuoteNum);
    doubleQuoteNum = 0;
}

bool DelimiterModeFsmParser::HandleQuote(char ch, DelimiterModeFsm& fsm) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
            fsm.currentState = STATE_QUOTE;
            return true;
        case STATE_QUOTE:
            fsm.currentState = STATE_DOUBLE_QUOTE;
            return true;
        case STATE_DATA:
            return false;
        case STATE_DOUBLE_QUOTE:
            fsm.currentState = STATE_QUOTE;
            fsm.field.append(1, ch);
            return true;
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::HandleQuote(int& fieldStart, int& fieldEnd, DelimiterModeFsm& fsm, int& doubleQuoteNum) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
            fsm.currentState = STATE_QUOTE;
            fieldStart++;
            return true;
        case STATE_QUOTE:
            fsm.currentState = STATE_DOUBLE_QUOTE;
            doubleQuoteNum++;
            fieldEnd++;
            return true;
        case STATE_DATA:
            return false;
        case STATE_DOUBLE_QUOTE:
            fsm.currentState = STATE_QUOTE;
            fieldEnd++;
            return true;
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::HandleData(char ch, DelimiterModeFsm& fsm) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
            fsm.currentState = STATE_DATA;
            fsm.field.append(1, ch);
            return true;
        case STATE_QUOTE:
        case STATE_DATA:
            fsm.field.append(1, ch);
            return true;
        case STATE_DOUBLE_QUOTE:
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::HandleData(int& fieldEnd, DelimiterModeFsm& fsm) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
            fsm.currentState = STATE_DATA;
            fieldEnd++;
            return true;
        case STATE_QUOTE:
        case STATE_DATA:
            fieldEnd++;
            return true;
        case STATE_DOUBLE_QUOTE:
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::HandleEOF(DelimiterModeFsm& fsm, std::vector<std::string>& columnValues) {
    switch (fsm.currentState) {
        case STATE_INITIAL:
        case STATE_DATA:
        case STATE_DOUBLE_QUOTE:
            columnValues.push_back(fsm.field);
            return true;
        case STATE_QUOTE:
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::HandleEOF(const char* ch,
                                       const char quote,
                                       int& fieldStart,
                                       int& fieldEnd,
                                       DelimiterModeFsm& fsm,
                                       std::vector<StringView>& columnValues,
                                       int& doubleQuoteNum,
                                       LogEvent& event) {
    if (fsm.currentState == STATE_DOUBLE_QUOTE) {
        doubleQuoteNum--;
    }
    switch (fsm.currentState) {
        case STATE_INITIAL:
        case STATE_DATA:
        case STATE_DOUBLE_QUOTE:
            AddFieldWithUnQuote(ch, quote, fieldStart, fieldEnd, columnValues, doubleQuoteNum, event);
            fieldStart = ++fieldEnd;
            return true;
        case STATE_QUOTE:
        default:
            return false;
    }
}

bool DelimiterModeFsmParser::ParseDelimiterLine(const char* buffer,
                                                int begin,
                                                int end,
                                                std::vector<std::string>& columnValues) {
    bool result = true;
    DelimiterModeFsm fsm(STATE_INITIAL, "");

    // here we won't check whether element in buffer is '\0',
    // because we consider that all element in this buffer is valid,
    // despite some '\0' elements which are brought from file system due to system crash
    for (int i = begin; i < end; ++i) {
        char ch = buffer[i];
        if (ch == separator) {
            result = HandleSeparator(ch, fsm, columnValues);
        } else if (ch == quote) {
            result = HandleQuote(ch, fsm);
        } else // data
        {
            result = HandleData(ch, fsm);
        }

        if (!result) {
            columnValues.clear();
            return false;
        }
    }

    result = HandleEOF(fsm, columnValues);
    // clear all columns if failed to parse
    if (!result) {
        columnValues.clear();
    }
    return result;
}

bool DelimiterModeFsmParser::ParseDelimiterLine(
    StringView buffer, int begin, int end, std::vector<StringView>& columnValues, LogEvent& event) {
    bool result = true;
    DelimiterModeFsm fsm(STATE_INITIAL, "");

    // when there is a double quote, we need to allocate a new buffer to store the unquoted field
    int doubleQuoteNum = 0;
    // here we won't check whether element in buffer is '\0',
    // because we consider that all element in this buffer is valid,
    // despite some '\0' elements which are brought from file system due to system crash
    const char* ch = buffer.data();
    int fieldStart = begin;
    int fieldEnd = begin;
    for (int i = begin; i < end; ++i) {
        if (ch[i] == separator) {
            result = HandleSeparator(ch, quote, fieldStart, fieldEnd, fsm, columnValues, doubleQuoteNum, event);
        } else if (ch[i] == quote) {
            result = HandleQuote(fieldStart, fieldEnd, fsm, doubleQuoteNum);
        } else // data
        {
            result = HandleData(fieldEnd, fsm);
        }

        if (!result) {
            columnValues.clear();
            return false;
        }
    }
    result = HandleEOF(ch, quote, fieldStart, fieldEnd, fsm, columnValues, doubleQuoteNum, event);
    // clear all columns if failed to parse
    if (!result) {
        columnValues.clear();
    }
    return result;
}

} // namespace logtail
