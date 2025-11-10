/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>

#include <memory>
#include <vector>

#include "common/memory/SourceBuffer.h"
#include "models/LogEvent.h"
#include "models/MetricEvent.h"
#include "models/PipelineEventGroup.h"
#include "models/SpanEvent.h"
#include "protobuf/models/ManualPBParser.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

// Helper class to access private methods of ManualPBParser for testing
class ManualPBParserTestHelper {
public:
    static bool TestReadVarint32(ManualPBParser& parser, uint32_t& value) { return parser.readVarint32(value); }

    static bool TestReadVarint64(ManualPBParser& parser, uint64_t& value) { return parser.readVarint64(value); }

    static bool TestReadFixed32(ManualPBParser& parser, uint32_t& value) { return parser.readFixed32(value); }

    static bool TestReadFixed64(ManualPBParser& parser, uint64_t& value) { return parser.readFixed64(value); }

    static bool TestReadLengthDelimited(ManualPBParser& parser, const uint8_t*& data, size_t& length) {
        return parser.readLengthDelimited(data, length);
    }

    static bool TestReadString(ManualPBParser& parser, std::string& str) { return parser.readString(str); }

    static bool TestSkipField(ManualPBParser& parser, uint32_t wireType) { return parser.skipField(wireType); }

    static std::string GetLastError(ManualPBParser& parser) { return parser.mLastError; }
};

// Test fixture for ManualPBParser
class ManualPBParserUnittest : public ::testing::Test {
public:
    // Category 1: Basic Data Type Reading Tests
    void TestReadVarint32Success();
    void TestReadVarint32MaxValue();
    void TestReadVarint32Invalid();
    void TestReadVarint64Success();
    void TestReadVarint64MaxValue();
    void TestReadFixed32Success();
    void TestReadFixed32InsufficientData();
    void TestReadFixed64Success();
    void TestReadLengthDelimitedSuccess();
    void TestReadLengthDelimitedInsufficientData();

    // Category 2: Error Input Handling Tests
    void TestParsePipelineEventGroupNullData();
    void TestParsePipelineEventGroupEmptyData();
    void TestParsePipelineEventGroupInvalidTag();
    void TestParsePipelineEventGroupInvalidWireType();
    void TestParseMetadataInvalidWireType();
    void TestParseTagsInvalidWireType();
    void TestParseLogEventsInvalidWireType();
    void TestParseMetricEventsInvalidWireType();

    // Category 3: LogEvent Complete Tests
    void TestParseLogEventComplete();
    void TestParseLogEventMinimalFields();
    void TestParseLogEventWithContents();
    void TestParseLogEventWithLevel();
    void TestParseLogEventWithFileOffset();
    void TestParseLogEventWithRawSize();
    void TestParseLogEventInvalidContentWireType();
    void TestParseLogEventMultipleEvents();

    // Category 4: MetricEvent Complete Tests
    void TestParseMetricEventComplete();
    void TestParseMetricEventMinimalFields();
    void TestParseMetricEventWithTags();
    void TestParseMetricEventWithValue();
    void TestParseMetricEventDoubleValue();
    void TestParseMetricEventInvalidTagsWireType();
    void TestParseMetricEventInvalidValueWireType();
    void TestParseMetricEventMultipleEvents();

    // Category 5: SpanEvent Complete Tests
    void TestParseSpanEventComplete();
    void TestParseSpanEventMinimalFields();
    void TestParseSpanEventWithTraceInfo();
    void TestParseSpanEventAllKinds();
    void TestParseSpanEventAllStatus();
    void TestParseSpanEventWithTags();
    void TestParseSpanEventWithScopeTags();
    void TestParseSpanEventWithInnerEvents();

    // Category 6: Map Field Parsing Tests
    void TestParseMapFieldEmpty();
    void TestParseMapFieldSingleEntry();
    void TestParseMapFieldMultipleEntries();
    void TestParseMapFieldKeyOnlyNoValue();
    void TestParseMapFieldValueOnlyNoKey();
    void TestParseMapFieldInvalidKeyWireType();
    void TestParseMapFieldInvalidValueWireType();
    void TestParseMapFieldUnknownFieldNumber();

    // Category 7: Unknown Field Handling Tests
    void TestSkipFieldVarint();
    void TestSkipFieldFixed32();
    void TestSkipFieldFixed64();
    void TestSkipFieldLengthDelimited();
    void TestUnknownFieldInPipelineEventGroup();

    // Category 8: Mixed Scenario Tests
    void TestMixedEventTypes();
    void TestMixedWithMetadataAndTags();
    void TestMixedEventsWithUnknownFields();
    void TestComplexNestedStructure();
    void TestLargeScaleMixedData();

    // Category 9: Boundary and Extreme Cases
    void TestEmptyStrings();
    void TestVeryLargeVarint();
    void TestMaximumNesting();
    void TestZeroValues();
    void TestLargeNumberOfFields();

    // Category 10: SpanLink Complete Tests
    void TestParseSpanLinkComplete();
    void TestParseSpanLinkMinimalFields();
    void TestParseSpanLinkWithTags();
    void TestParseSpanLinkMultipleLinks();
    void TestParseSpanLinkInvalidTraceIdWireType();
    void TestParseSpanLinkInvalidSpanIdWireType();
    void TestParseSpanLinkInvalidTagsWireType();
    void TestParseSpanLinkTagsKeyOnlyNoValue();
    void TestParseSpanLinkTagsInvalidKeyWireType();
    void TestParseSpanLinkWithUnknownFields();

    // Category 11: Data Corruption and Error Recovery Tests
    void TestReadVarint32Truncated();
    void TestReadVarint64Truncated();
    void TestReadFixed64InsufficientData();
    void TestReadStringLengthExceedsBounds();
    void TestParseLogEventReadNameFailed();
    void TestParseMetricEventReadNameFailed();
    void TestParseSpanEventReadTraceIdFailed();
    void TestParseMapEntryReadKeyFailed();
    void TestParseMapEntryReadValueFailed();
    void TestNestedMessageParseFailed();

    // Category 12: Boundary and Special Value Tests
    void TestVarint64MaxValueParsing();
    void TestVarint3210ByteEncoding();
    void TestLengthDelimitedZeroLength();
    void TestLengthDelimitedVeryLarge();
    void TestSpanEventAllFieldsMaxValues();
    void TestMetricEventNegativeDoubleValue();
    void TestLogEventTimestampZeroValue();
    void TestDeepNestedSpanEventStructure();

    // Category 13: skipField Detailed Tests
    void TestSkipFieldInvalidWireType();
    void TestSkipFieldNestedLengthDelimited();
    void TestSkipFieldMultipleConsecutive();
    void TestSkipFieldAfterPartialParse();
    void TestSkipFieldLargeVarint();

    // Category 14: readBytes Specialized Tests
    void TestReadBytesSuccess();
    void TestReadBytesZeroLength();
    void TestReadBytesInsufficientData();
    void TestReadBytesVeryLarge();
    void TestReadBytesInReadVarintFailed();

    // Category 15: WireType Validation Error Tests
    void TestLogEventInvalidTimestampWireType();
    void TestLogEventInvalidContentsWireType();
    void TestLogEventInvalidLevelWireType();
    void TestLogEventInvalidFileOffsetWireType();
    void TestLogEventInvalidRawSizeWireType();
    void TestMetricEventInvalidTimestampWireType();
    void TestMetricEventInvalidNameWireType();
    void TestMetricEventInvalidTagsWireType();
    void TestMetricEventInvalidValueWireType();
    void TestSpanEventInvalidTimestampWireType();
    void TestSpanEventInvalidStartTimeWireType();
    void TestSpanEventInvalidEndTimeWireType();
    void TestSpanEventInvalidTraceIdWireType();
    void TestSpanEventInvalidSpanIdWireType();
    void TestSpanEventInvalidParentSpanIdWireType();
    void TestSpanEventInvalidNameWireType();
    void TestSpanEventInvalidKindWireType();
    void TestSpanEventInvalidStatusWireType();
    void TestSpanEventInvalidTagsWireType();
    void TestSpanEventInvalidScopeTagsWireType();
    void TestSpanEventInvalidTraceStateWireType();
    void TestSpanInnerEventInvalidTimestampWireType();
    void TestSpanInnerEventInvalidNameWireType();
    void TestSpanInnerEventInvalidTagsWireType();
    void TestSpanLinkInvalidTraceStateWireType();
    void TestLogContentInvalidKeyWireType();
    void TestLogContentInvalidValueWireType();

    // Category 16-17: Read Failure & Enum Tests
    void TestSpanEventKindEnum();
    void TestSpanEventStatusEnum();
    void TestMetricEventFixed32Value();
    void TestMetricEventFixed64Value();
    void TestLogEventsListReadTagFailure();
    void TestMetricEventsListReadTagFailure();
    void TestSpanEventsListReadTagFailure();
    void TestLogEventReadLevelFailed();
    void TestLogEventReadTimestampFailed();
    void TestMetricEventReadTimestampFailed();
    void TestSpanEventReadTraceIdFailedPhase3();
    void TestSpanEventReadSpanIdFailed();
    void TestSpanEventReadNameFailed();
    void TestSpanEventReadStartTimeFailed();
    void TestSpanEventReadEndTimeFailed();
    void TestSpanInnerEventReadNameFailed();
    void TestLogContentReadTagFailed();
    void TestMapEntryReadKeyFailedPhase3();
    void TestMapEntryReadValueFailedPhase3();
    void TestVarint32MultiByteEncoding();
    void TestVarint64MultiByteEncoding();

    // Phase 5-8: Coverage improvement tests
    void TestSpanEventsListUnknownField();
    void TestLogEventsListUnknownField();
    void TestMetricEventsListUnknownField();
    void TestMetricValueReadLengthDelimitedFailed();
    void TestMetricValueReadTagFailed();
    void TestLogContentUnknownField();
    void TestMetricTagsUnknownField();
    void TestSpanTagsUnknownField();
    void TestSpanScopeTagsUnknownField();
    void TestSpanInnerEventReadTimestampFailed();
    void TestSpanLinkReadTraceIdFailed();

protected:
    void SetUp() override {}

    void TearDown() override {}

    // Wire type constants
    static constexpr uint32_t kVarint = 0;
    static constexpr uint32_t kFixed64 = 1;
    static constexpr uint32_t kLengthDelimited = 2;
    static constexpr uint32_t kFixed32 = 5;

protected:
    // Helper function to encode varint32
    static std::vector<uint8_t> encodeVarint32(uint32_t value) {
        std::vector<uint8_t> result;
        while (value >= 0x80) {
            result.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        result.push_back(static_cast<uint8_t>(value));
        return result;
    }

    // Helper function to encode varint64
    static std::vector<uint8_t> encodeVarint64(uint64_t value) {
        std::vector<uint8_t> result;
        while (value >= 0x80) {
            result.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
        result.push_back(static_cast<uint8_t>(value));
        return result;
    }

    // Helper function to encode fixed32 (little endian)
    static std::vector<uint8_t> encodeFixed32(uint32_t value) {
        std::vector<uint8_t> result(4);
        result[0] = static_cast<uint8_t>(value & 0xFF);
        result[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        result[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        result[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        return result;
    }

    // Helper function to encode fixed64 (little endian)
    static std::vector<uint8_t> encodeFixed64(uint64_t value) {
        std::vector<uint8_t> result(8);
        result[0] = static_cast<uint8_t>(value & 0xFF);
        result[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        result[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        result[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
        result[4] = static_cast<uint8_t>((value >> 32) & 0xFF);
        result[5] = static_cast<uint8_t>((value >> 40) & 0xFF);
        result[6] = static_cast<uint8_t>((value >> 48) & 0xFF);
        result[7] = static_cast<uint8_t>((value >> 56) & 0xFF);
        return result;
    }

    // Helper function to encode length-delimited data
    static std::vector<uint8_t> encodeLengthDelimited(const std::string& str) {
        std::vector<uint8_t> result = encodeVarint32(str.length());
        result.insert(result.end(), str.begin(), str.end());
        return result;
    }

    // Helper function to encode protobuf tag (field_number << 3 | wire_type)
    static uint32_t encodeTag(uint32_t fieldNumber, uint32_t wireType) { return (fieldNumber << 3) | wireType; }

    // Helper function to convert vector to string
    static std::string vectorToString(const std::vector<uint8_t>& vec) {
        return std::string(reinterpret_cast<const char*>(vec.data()), vec.size());
    }

    // String-based helper functions for easier test writing
    static std::string encodeVarint32Str(uint32_t value) { return vectorToString(encodeVarint32(value)); }

    static std::string encodeVarint64Str(uint64_t value) { return vectorToString(encodeVarint64(value)); }

    static std::string encodeTagStr(uint32_t fieldNumber, uint32_t wireType) {
        return vectorToString(encodeVarint32(encodeTag(fieldNumber, wireType)));
    }

    static std::string encodeFixed32Str(uint32_t value) { return vectorToString(encodeFixed32(value)); }

    static std::string encodeFixed64Str(uint64_t value) { return vectorToString(encodeFixed64(value)); }

    static std::string encodeLengthDelimitedStr(const std::string& str) {
        return vectorToString(encodeLengthDelimited(str));
    }

    // Helper to convert vec-based encoders to string
    static std::string encodePipelineEventGroupWithLogsStr(const std::vector<std::vector<uint8_t>>& logEvents) {
        return vectorToString(encodePipelineEventGroupWithLogs(logEvents));
    }

    static std::string encodePipelineEventGroupWithMetricsStr(const std::vector<std::vector<uint8_t>>& metricEvents) {
        return vectorToString(encodePipelineEventGroupWithMetrics(metricEvents));
    }

    static std::string encodePipelineEventGroupWithSpansStr(const std::vector<std::vector<uint8_t>>& spanEvents) {
        return vectorToString(encodePipelineEventGroupWithSpans(spanEvents));
    }

    // Helper function to encode LogEvent Content (key-value pair)
    static std::vector<uint8_t> encodeLogContent(const std::string& key, const std::string& value) {
        std::vector<uint8_t> result;

        // field 1: key (string)
        auto keyTag = encodeVarint32(encodeTag(1, 2)); // field 1, wire type 2 (length-delimited)
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 2: value (string)
        auto valueTag = encodeVarint32(encodeTag(2, 2)); // field 2, wire type 2 (length-delimited)
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode a single LogEvent
    static std::vector<uint8_t> encodeLogEvent(uint64_t timestamp,
                                               const std::vector<std::pair<std::string, std::string>>& contents,
                                               const std::string& level = "",
                                               uint64_t fileOffset = 0,
                                               uint64_t rawSize = 0) {
        std::vector<uint8_t> result;

        // field 1: timestamp (varint)
        auto timestampTag = encodeVarint32(encodeTag(1, 0)); // field 1, wire type 0 (varint)
        result.insert(result.end(), timestampTag.begin(), timestampTag.end());
        auto timestampData = encodeVarint64(timestamp);
        result.insert(result.end(), timestampData.begin(), timestampData.end());

        // field 2: contents (repeated, length-delimited)
        for (const auto& content : contents) {
            auto contentTag = encodeVarint32(encodeTag(2, 2)); // field 2, wire type 2 (length-delimited)
            result.insert(result.end(), contentTag.begin(), contentTag.end());

            auto contentData = encodeLogContent(content.first, content.second);
            auto contentLen = encodeVarint32(contentData.size());
            result.insert(result.end(), contentLen.begin(), contentLen.end());
            result.insert(result.end(), contentData.begin(), contentData.end());
        }

        // field 3: level (string, optional)
        if (!level.empty()) {
            auto levelTag = encodeVarint32(encodeTag(3, 2)); // field 3, wire type 2 (length-delimited)
            result.insert(result.end(), levelTag.begin(), levelTag.end());
            auto levelData = encodeLengthDelimited(level);
            result.insert(result.end(), levelData.begin(), levelData.end());
        }

        // field 4: file_offset (varint, optional)
        if (fileOffset > 0) {
            auto offsetTag = encodeVarint32(encodeTag(4, 0)); // field 4, wire type 0 (varint)
            result.insert(result.end(), offsetTag.begin(), offsetTag.end());
            auto offsetData = encodeVarint64(fileOffset);
            result.insert(result.end(), offsetData.begin(), offsetData.end());
        }

        // field 5: raw_size (varint, optional)
        if (rawSize > 0) {
            auto sizeTag = encodeVarint32(encodeTag(5, 0)); // field 5, wire type 0 (varint)
            result.insert(result.end(), sizeTag.begin(), sizeTag.end());
            auto sizeData = encodeVarint64(rawSize);
            result.insert(result.end(), sizeData.begin(), sizeData.end());
        }

        return result;
    }

    // Helper function to encode LogEvents message (wrapper for repeated LogEvent)
    static std::vector<uint8_t> encodeLogEvents(const std::vector<std::vector<uint8_t>>& logEvents) {
        std::vector<uint8_t> result;

        // Each LogEvent is in field 1 (repeated)
        for (const auto& logEvent : logEvents) {
            auto eventTag = encodeVarint32(encodeTag(1, 2)); // field 1, wire type 2 (length-delimited)
            result.insert(result.end(), eventTag.begin(), eventTag.end());
            auto eventLen = encodeVarint32(logEvent.size());
            result.insert(result.end(), eventLen.begin(), eventLen.end());
            result.insert(result.end(), logEvent.begin(), logEvent.end());
        }

        return result;
    }

    // Helper function to encode PipelineEventGroup with LogEvents
    static std::vector<uint8_t> encodePipelineEventGroupWithLogs(const std::vector<std::vector<uint8_t>>& logEvents) {
        std::vector<uint8_t> result;

        // field 3: Logs (LogEvents message)
        auto logsTag = encodeVarint32(encodeTag(3, 2)); // field 3, wire type 2 (length-delimited)
        result.insert(result.end(), logsTag.begin(), logsTag.end());

        auto logsData = encodeLogEvents(logEvents);
        auto logsLen = encodeVarint32(logsData.size());
        result.insert(result.end(), logsLen.begin(), logsLen.end());
        result.insert(result.end(), logsData.begin(), logsData.end());

        return result;
    }

    // Helper function to encode MetricEvent Tag (key-value pair)
    static std::vector<uint8_t> encodeMetricTag(const std::string& key, const std::string& value) {
        std::vector<uint8_t> result;

        // field 1: key (string)
        auto keyTag = encodeVarint32(encodeTag(1, 2)); // field 1, wire type 2 (length-delimited)
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 2: value (string)
        auto valueTag = encodeVarint32(encodeTag(2, 2)); // field 2, wire type 2 (length-delimited)
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode UntypedSingleValue (double)
    static std::vector<uint8_t> encodeUntypedSingleValue(double value) {
        std::vector<uint8_t> result;

        // field 1: value (double, wire type fixed64)
        auto valueTag = encodeVarint32(encodeTag(1, 1)); // field 1, wire type 1 (fixed64)
        result.insert(result.end(), valueTag.begin(), valueTag.end());

        // Convert double to uint64_t (IEEE 754)
        uint64_t doubleBytes = 0;
        memcpy(&doubleBytes, &value, sizeof(double));
        auto doubleData = encodeFixed64(doubleBytes);
        result.insert(result.end(), doubleData.begin(), doubleData.end());

        return result;
    }

    // Helper function to encode a single MetricEvent
    static std::vector<uint8_t> encodeMetricEvent(uint64_t timestamp,
                                                  const std::string& name,
                                                  const std::vector<std::pair<std::string, std::string>>& tags,
                                                  double value = 0.0,
                                                  bool hasValue = false) {
        std::vector<uint8_t> result;

        // field 1: timestamp (varint)
        auto timestampTag = encodeVarint32(encodeTag(1, 0)); // field 1, wire type 0 (varint)
        result.insert(result.end(), timestampTag.begin(), timestampTag.end());
        auto timestampData = encodeVarint64(timestamp);
        result.insert(result.end(), timestampData.begin(), timestampData.end());

        // field 2: name (string)
        auto nameTag = encodeVarint32(encodeTag(2, 2)); // field 2, wire type 2 (length-delimited)
        result.insert(result.end(), nameTag.begin(), nameTag.end());
        auto nameData = encodeLengthDelimited(name);
        result.insert(result.end(), nameData.begin(), nameData.end());

        // field 3: tags (repeated, length-delimited)
        for (const auto& tag : tags) {
            auto tagTag = encodeVarint32(encodeTag(3, 2)); // field 3, wire type 2 (length-delimited)
            result.insert(result.end(), tagTag.begin(), tagTag.end());

            auto tagData = encodeMetricTag(tag.first, tag.second);
            auto tagLen = encodeVarint32(tagData.size());
            result.insert(result.end(), tagLen.begin(), tagLen.end());
            result.insert(result.end(), tagData.begin(), tagData.end());
        }

        // field 4: untyped_single_value (UntypedSingleValue message, optional)
        if (hasValue) {
            auto valueTag = encodeVarint32(encodeTag(4, 2)); // field 4, wire type 2 (length-delimited)
            result.insert(result.end(), valueTag.begin(), valueTag.end());

            auto valueData = encodeUntypedSingleValue(value);
            auto valueLen = encodeVarint32(valueData.size());
            result.insert(result.end(), valueLen.begin(), valueLen.end());
            result.insert(result.end(), valueData.begin(), valueData.end());
        }

        return result;
    }

    // Helper function to encode MetricEvents message (wrapper for repeated MetricEvent)
    static std::vector<uint8_t> encodeMetricEvents(const std::vector<std::vector<uint8_t>>& metricEvents) {
        std::vector<uint8_t> result;

        // Each MetricEvent is in field 1 (repeated)
        for (const auto& metricEvent : metricEvents) {
            auto eventTag = encodeVarint32(encodeTag(1, 2)); // field 1, wire type 2 (length-delimited)
            result.insert(result.end(), eventTag.begin(), eventTag.end());
            auto eventLen = encodeVarint32(metricEvent.size());
            result.insert(result.end(), eventLen.begin(), eventLen.end());
            result.insert(result.end(), metricEvent.begin(), metricEvent.end());
        }

        return result;
    }

    // Helper function to encode PipelineEventGroup with MetricEvents
    static std::vector<uint8_t>
    encodePipelineEventGroupWithMetrics(const std::vector<std::vector<uint8_t>>& metricEvents) {
        std::vector<uint8_t> result;

        // field 4: Metrics (MetricEvents message)
        auto metricsTag = encodeVarint32(encodeTag(4, 2)); // field 4, wire type 2 (length-delimited)
        result.insert(result.end(), metricsTag.begin(), metricsTag.end());

        auto metricsData = encodeMetricEvents(metricEvents);
        auto metricsLen = encodeVarint32(metricsData.size());
        result.insert(result.end(), metricsLen.begin(), metricsLen.end());
        result.insert(result.end(), metricsData.begin(), metricsData.end());

        return result;
    }

    // Helper function to encode SpanEvent Tag (key-value pair)
    static std::vector<uint8_t> encodeSpanTag(const std::string& key, const std::string& value) {
        std::vector<uint8_t> result;

        // field 1: key (string)
        auto keyTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 2: value (string)
        auto valueTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode SpanEvent InnerEvent
    static std::vector<uint8_t> encodeSpanInnerEvent(uint64_t timestampNs,
                                                     const std::string& name,
                                                     const std::vector<std::pair<std::string, std::string>>& tags) {
        std::vector<uint8_t> result;

        // field 1: timestamp (varint)
        auto timestampTag = encodeVarint32(encodeTag(1, 0));
        result.insert(result.end(), timestampTag.begin(), timestampTag.end());
        auto timestampData = encodeVarint64(timestampNs);
        result.insert(result.end(), timestampData.begin(), timestampData.end());

        // field 2: name (string)
        auto nameTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), nameTag.begin(), nameTag.end());
        auto nameData = encodeLengthDelimited(name);
        result.insert(result.end(), nameData.begin(), nameData.end());

        // field 3: tags (repeated)
        for (const auto& tag : tags) {
            auto tagTag = encodeVarint32(encodeTag(3, 2));
            result.insert(result.end(), tagTag.begin(), tagTag.end());
            auto tagData = encodeSpanTag(tag.first, tag.second);
            auto tagLen = encodeVarint32(tagData.size());
            result.insert(result.end(), tagLen.begin(), tagLen.end());
            result.insert(result.end(), tagData.begin(), tagData.end());
        }

        return result;
    }

    // Helper function to encode a single SpanEvent
    static std::vector<uint8_t> encodeSpanEvent(uint64_t timestamp,
                                                const std::string& traceId,
                                                const std::string& spanId,
                                                const std::string& name,
                                                uint32_t kind,
                                                uint64_t startTimeNs,
                                                uint64_t endTimeNs,
                                                const std::vector<std::pair<std::string, std::string>>& tags = {},
                                                const std::vector<std::pair<std::string, std::string>>& scopeTags = {},
                                                const std::string& traceState = "",
                                                const std::string& parentSpanId = "",
                                                uint32_t status = 0,
                                                const std::vector<std::vector<uint8_t>>& innerEvents = {}) {
        std::vector<uint8_t> result;

        // field 1: timestamp (varint)
        auto timestampTag = encodeVarint32(encodeTag(1, 0));
        result.insert(result.end(), timestampTag.begin(), timestampTag.end());
        auto timestampData = encodeVarint64(timestamp);
        result.insert(result.end(), timestampData.begin(), timestampData.end());

        // field 2: trace_id (string)
        auto traceIdTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeLengthDelimited(traceId);
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        // field 3: span_id (string)
        auto spanIdTag = encodeVarint32(encodeTag(3, 2));
        result.insert(result.end(), spanIdTag.begin(), spanIdTag.end());
        auto spanIdData = encodeLengthDelimited(spanId);
        result.insert(result.end(), spanIdData.begin(), spanIdData.end());

        // field 4: trace_state (string, optional)
        if (!traceState.empty()) {
            auto traceStateTag = encodeVarint32(encodeTag(4, 2));
            result.insert(result.end(), traceStateTag.begin(), traceStateTag.end());
            auto traceStateData = encodeLengthDelimited(traceState);
            result.insert(result.end(), traceStateData.begin(), traceStateData.end());
        }

        // field 5: parent_span_id (string, optional)
        if (!parentSpanId.empty()) {
            auto parentSpanIdTag = encodeVarint32(encodeTag(5, 2));
            result.insert(result.end(), parentSpanIdTag.begin(), parentSpanIdTag.end());
            auto parentSpanIdData = encodeLengthDelimited(parentSpanId);
            result.insert(result.end(), parentSpanIdData.begin(), parentSpanIdData.end());
        }

        // field 6: name (string)
        auto nameTag = encodeVarint32(encodeTag(6, 2));
        result.insert(result.end(), nameTag.begin(), nameTag.end());
        auto nameData = encodeLengthDelimited(name);
        result.insert(result.end(), nameData.begin(), nameData.end());

        // field 7: kind (varint)
        auto kindTag = encodeVarint32(encodeTag(7, 0));
        result.insert(result.end(), kindTag.begin(), kindTag.end());
        auto kindData = encodeVarint32(kind);
        result.insert(result.end(), kindData.begin(), kindData.end());

        // field 8: start_time (varint)
        auto startTimeTag = encodeVarint32(encodeTag(8, 0));
        result.insert(result.end(), startTimeTag.begin(), startTimeTag.end());
        auto startTimeData = encodeVarint64(startTimeNs);
        result.insert(result.end(), startTimeData.begin(), startTimeData.end());

        // field 9: end_time (varint)
        auto endTimeTag = encodeVarint32(encodeTag(9, 0));
        result.insert(result.end(), endTimeTag.begin(), endTimeTag.end());
        auto endTimeData = encodeVarint64(endTimeNs);
        result.insert(result.end(), endTimeData.begin(), endTimeData.end());

        // field 10: tags (repeated)
        for (const auto& tag : tags) {
            auto tagTag = encodeVarint32(encodeTag(10, 2));
            result.insert(result.end(), tagTag.begin(), tagTag.end());
            auto tagData = encodeSpanTag(tag.first, tag.second);
            auto tagLen = encodeVarint32(tagData.size());
            result.insert(result.end(), tagLen.begin(), tagLen.end());
            result.insert(result.end(), tagData.begin(), tagData.end());
        }

        // field 11: events (InnerEvents, repeated)
        for (const auto& innerEvent : innerEvents) {
            auto eventTag = encodeVarint32(encodeTag(11, 2));
            result.insert(result.end(), eventTag.begin(), eventTag.end());
            auto eventLen = encodeVarint32(innerEvent.size());
            result.insert(result.end(), eventLen.begin(), eventLen.end());
            result.insert(result.end(), innerEvent.begin(), innerEvent.end());
        }

        // field 13: status (varint)
        if (status > 0) {
            auto statusTag = encodeVarint32(encodeTag(13, 0));
            result.insert(result.end(), statusTag.begin(), statusTag.end());
            auto statusData = encodeVarint32(status);
            result.insert(result.end(), statusData.begin(), statusData.end());
        }

        // field 14: scope_tags (repeated)
        for (const auto& scopeTag : scopeTags) {
            auto scopeTagTag = encodeVarint32(encodeTag(14, 2));
            result.insert(result.end(), scopeTagTag.begin(), scopeTagTag.end());
            auto scopeTagData = encodeSpanTag(scopeTag.first, scopeTag.second);
            auto scopeTagLen = encodeVarint32(scopeTagData.size());
            result.insert(result.end(), scopeTagLen.begin(), scopeTagLen.end());
            result.insert(result.end(), scopeTagData.begin(), scopeTagData.end());
        }

        return result;
    }

    // Helper function to encode SpanEvents message (wrapper for repeated SpanEvent)
    static std::vector<uint8_t> encodeSpanEvents(const std::vector<std::vector<uint8_t>>& spanEvents) {
        std::vector<uint8_t> result;

        // Each SpanEvent is in field 1 (repeated)
        for (const auto& spanEvent : spanEvents) {
            auto eventTag = encodeVarint32(encodeTag(1, 2));
            result.insert(result.end(), eventTag.begin(), eventTag.end());
            auto eventLen = encodeVarint32(spanEvent.size());
            result.insert(result.end(), eventLen.begin(), eventLen.end());
            result.insert(result.end(), spanEvent.begin(), spanEvent.end());
        }

        return result;
    }

    // Helper function to encode PipelineEventGroup with SpanEvents
    static std::vector<uint8_t> encodePipelineEventGroupWithSpans(const std::vector<std::vector<uint8_t>>& spanEvents) {
        std::vector<uint8_t> result;

        // field 5: Spans (SpanEvents message)
        auto spansTag = encodeVarint32(encodeTag(5, 2)); // field 5, wire type 2 (length-delimited)
        result.insert(result.end(), spansTag.begin(), spansTag.end());

        auto spansData = encodeSpanEvents(spanEvents);
        auto spansLen = encodeVarint32(spansData.size());
        result.insert(result.end(), spansLen.begin(), spansLen.end());
        result.insert(result.end(), spansData.begin(), spansData.end());

        return result;
    }

    // Helper function to encode a map entry (key-value pair)
    static std::vector<uint8_t> encodeMapEntry(const std::string& key, const std::string& value) {
        std::vector<uint8_t> result;

        // field 1: key (string)
        auto keyTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 2: value (string)
        auto valueTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode a map entry with only key
    static std::vector<uint8_t> encodeMapEntryKeyOnly(const std::string& key) {
        std::vector<uint8_t> result;

        // field 1: key (string)
        auto keyTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        return result;
    }

    // Helper function to encode a map entry with only value
    static std::vector<uint8_t> encodeMapEntryValueOnly(const std::string& value) {
        std::vector<uint8_t> result;

        // field 2: value (string)
        auto valueTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode a map entry with invalid wire type for key
    static std::vector<uint8_t> encodeMapEntryInvalidKeyWireType() {
        std::vector<uint8_t> result;

        // field 1: key with wire type 0 (varint) instead of 2 (length-delimited)
        auto keyTag = encodeVarint32(encodeTag(1, 0));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeVarint32(123);
        result.insert(result.end(), keyData.begin(), keyData.end());

        return result;
    }

    // Helper function to encode a map entry with invalid wire type for value
    static std::vector<uint8_t> encodeMapEntryInvalidValueWireType() {
        std::vector<uint8_t> result;

        // field 1: key (correct)
        auto keyTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited("key");
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 2: value with wire type 0 (varint) instead of 2 (length-delimited)
        auto valueTag = encodeVarint32(encodeTag(2, 0));
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeVarint32(456);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode a map entry with unknown field number
    static std::vector<uint8_t> encodeMapEntryWithUnknownField(const std::string& key, const std::string& value) {
        std::vector<uint8_t> result;

        // field 1: key
        auto keyTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 3: unknown field (should be skipped)
        auto unknownTag = encodeVarint32(encodeTag(3, 0));
        result.insert(result.end(), unknownTag.begin(), unknownTag.end());
        auto unknownData = encodeVarint32(999);
        result.insert(result.end(), unknownData.begin(), unknownData.end());

        // field 2: value
        auto valueTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode PipelineEventGroup with metadata
    static std::vector<uint8_t>
    encodePipelineEventGroupWithMetadata(const std::vector<std::pair<std::string, std::string>>& metadata) {
        std::vector<uint8_t> result;

        // field 1: Metadata (repeated map entries)
        for (const auto& entry : metadata) {
            auto metadataTag = encodeVarint32(encodeTag(1, 2));
            result.insert(result.end(), metadataTag.begin(), metadataTag.end());
            auto entryData = encodeMapEntry(entry.first, entry.second);
            auto entryLen = encodeVarint32(entryData.size());
            result.insert(result.end(), entryLen.begin(), entryLen.end());
            result.insert(result.end(), entryData.begin(), entryData.end());
        }

        return result;
    }

    // Helper function to encode PipelineEventGroup with tags
    static std::vector<uint8_t>
    encodePipelineEventGroupWithTags(const std::vector<std::pair<std::string, std::string>>& tags) {
        std::vector<uint8_t> result;

        // field 2: Tags (repeated map entries)
        for (const auto& entry : tags) {
            auto tagsTag = encodeVarint32(encodeTag(2, 2));
            result.insert(result.end(), tagsTag.begin(), tagsTag.end());
            auto entryData = encodeMapEntry(entry.first, entry.second);
            auto entryLen = encodeVarint32(entryData.size());
            result.insert(result.end(), entryLen.begin(), entryLen.end());
            result.insert(result.end(), entryData.begin(), entryData.end());
        }

        return result;
    }

    // Helper function to encode SpanLink Tag (key-value pair, same as SpanTag)
    static std::vector<uint8_t> encodeSpanLinkTag(const std::string& key, const std::string& value) {
        std::vector<uint8_t> result;

        // field 1: key (string)
        auto keyTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), keyTag.begin(), keyTag.end());
        auto keyData = encodeLengthDelimited(key);
        result.insert(result.end(), keyData.begin(), keyData.end());

        // field 2: value (string)
        auto valueTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), valueTag.begin(), valueTag.end());
        auto valueData = encodeLengthDelimited(value);
        result.insert(result.end(), valueData.begin(), valueData.end());

        return result;
    }

    // Helper function to encode a single SpanLink
    static std::vector<uint8_t> encodeSpanLink(const std::string& traceId,
                                               const std::string& spanId,
                                               const std::string& traceState = "",
                                               const std::vector<std::pair<std::string, std::string>>& tags = {}) {
        std::vector<uint8_t> result;

        // field 1: trace_id (string)
        auto traceIdTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeLengthDelimited(traceId);
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        // field 2: span_id (string)
        auto spanIdTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), spanIdTag.begin(), spanIdTag.end());
        auto spanIdData = encodeLengthDelimited(spanId);
        result.insert(result.end(), spanIdData.begin(), spanIdData.end());

        // field 3: trace_state (string, optional)
        if (!traceState.empty()) {
            auto traceStateTag = encodeVarint32(encodeTag(3, 2));
            result.insert(result.end(), traceStateTag.begin(), traceStateTag.end());
            auto traceStateData = encodeLengthDelimited(traceState);
            result.insert(result.end(), traceStateData.begin(), traceStateData.end());
        }

        // field 4: tags (repeated)
        for (const auto& tag : tags) {
            auto tagTag = encodeVarint32(encodeTag(4, 2));
            result.insert(result.end(), tagTag.begin(), tagTag.end());
            auto tagData = encodeSpanLinkTag(tag.first, tag.second);
            auto tagLen = encodeVarint32(tagData.size());
            result.insert(result.end(), tagLen.begin(), tagLen.end());
            result.insert(result.end(), tagData.begin(), tagData.end());
        }

        return result;
    }

    // Helper function to encode SpanLink with invalid trace_id wire type
    static std::vector<uint8_t> encodeSpanLinkInvalidTraceIdWireType() {
        std::vector<uint8_t> result;

        // field 1: trace_id with wire type 0 (varint) instead of 2 (length-delimited)
        auto traceIdTag = encodeVarint32(encodeTag(1, 0));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeVarint32(123);
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        return result;
    }

    // Helper function to encode SpanLink with invalid span_id wire type
    static std::vector<uint8_t> encodeSpanLinkInvalidSpanIdWireType() {
        std::vector<uint8_t> result;

        // field 1: trace_id (correct)
        auto traceIdTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeLengthDelimited("trace-id");
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        // field 2: span_id with wire type 0 (varint) instead of 2 (length-delimited)
        auto spanIdTag = encodeVarint32(encodeTag(2, 0));
        result.insert(result.end(), spanIdTag.begin(), spanIdTag.end());
        auto spanIdData = encodeVarint32(456);
        result.insert(result.end(), spanIdData.begin(), spanIdData.end());

        return result;
    }

    // Helper function to encode SpanLink with invalid tags wire type
    static std::vector<uint8_t> encodeSpanLinkInvalidTagsWireType() {
        std::vector<uint8_t> result;

        // field 1: trace_id (correct)
        auto traceIdTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeLengthDelimited("trace-id");
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        // field 2: span_id (correct)
        auto spanIdTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), spanIdTag.begin(), spanIdTag.end());
        auto spanIdData = encodeLengthDelimited("span-id");
        result.insert(result.end(), spanIdData.begin(), spanIdData.end());

        // field 4: tags with wire type 0 (varint) instead of 2 (length-delimited)
        auto tagsTag = encodeVarint32(encodeTag(4, 0));
        result.insert(result.end(), tagsTag.begin(), tagsTag.end());
        auto tagsData = encodeVarint32(789);
        result.insert(result.end(), tagsData.begin(), tagsData.end());

        return result;
    }

    // Helper function to encode SpanLink with unknown field
    static std::vector<uint8_t> encodeSpanLinkWithUnknownField(const std::string& traceId, const std::string& spanId) {
        std::vector<uint8_t> result;

        // field 1: trace_id
        auto traceIdTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeLengthDelimited(traceId);
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        // field 99: unknown field (should be skipped)
        auto unknownTag = encodeVarint32(encodeTag(99, 0));
        result.insert(result.end(), unknownTag.begin(), unknownTag.end());
        auto unknownData = encodeVarint32(999);
        result.insert(result.end(), unknownData.begin(), unknownData.end());

        // field 2: span_id
        auto spanIdTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), spanIdTag.begin(), spanIdTag.end());
        auto spanIdData = encodeLengthDelimited(spanId);
        result.insert(result.end(), spanIdData.begin(), spanIdData.end());

        return result;
    }

    // Helper function to encode SpanEvent with links
    static std::vector<uint8_t> encodeSpanEventWithLinks(uint64_t timestamp,
                                                         const std::string& traceId,
                                                         const std::string& spanId,
                                                         const std::string& name,
                                                         uint32_t kind,
                                                         uint64_t startTimeNs,
                                                         uint64_t endTimeNs,
                                                         const std::vector<std::vector<uint8_t>>& links) {
        std::vector<uint8_t> result;

        // field 1: timestamp (varint)
        auto timestampTag = encodeVarint32(encodeTag(1, 0));
        result.insert(result.end(), timestampTag.begin(), timestampTag.end());
        auto timestampData = encodeVarint64(timestamp);
        result.insert(result.end(), timestampData.begin(), timestampData.end());

        // field 2: trace_id (string)
        auto traceIdTag = encodeVarint32(encodeTag(2, 2));
        result.insert(result.end(), traceIdTag.begin(), traceIdTag.end());
        auto traceIdData = encodeLengthDelimited(traceId);
        result.insert(result.end(), traceIdData.begin(), traceIdData.end());

        // field 3: span_id (string)
        auto spanIdTag = encodeVarint32(encodeTag(3, 2));
        result.insert(result.end(), spanIdTag.begin(), spanIdTag.end());
        auto spanIdData = encodeLengthDelimited(spanId);
        result.insert(result.end(), spanIdData.begin(), spanIdData.end());

        // field 6: name (string)
        auto nameTag = encodeVarint32(encodeTag(6, 2));
        result.insert(result.end(), nameTag.begin(), nameTag.end());
        auto nameData = encodeLengthDelimited(name);
        result.insert(result.end(), nameData.begin(), nameData.end());

        // field 7: kind (varint)
        auto kindTag = encodeVarint32(encodeTag(7, 0));
        result.insert(result.end(), kindTag.begin(), kindTag.end());
        auto kindData = encodeVarint32(kind);
        result.insert(result.end(), kindData.begin(), kindData.end());

        // field 8: start_time (varint)
        auto startTimeTag = encodeVarint32(encodeTag(8, 0));
        result.insert(result.end(), startTimeTag.begin(), startTimeTag.end());
        auto startTimeData = encodeVarint64(startTimeNs);
        result.insert(result.end(), startTimeData.begin(), startTimeData.end());

        // field 9: end_time (varint)
        auto endTimeTag = encodeVarint32(encodeTag(9, 0));
        result.insert(result.end(), endTimeTag.begin(), endTimeTag.end());
        auto endTimeData = encodeVarint64(endTimeNs);
        result.insert(result.end(), endTimeData.begin(), endTimeData.end());

        // field 12: links (repeated)
        for (const auto& link : links) {
            auto linkTag = encodeVarint32(encodeTag(12, 2));
            result.insert(result.end(), linkTag.begin(), linkTag.end());
            auto linkLen = encodeVarint32(link.size());
            result.insert(result.end(), linkLen.begin(), linkLen.end());
            result.insert(result.end(), link.begin(), link.end());
        }

        return result;
    }
};

// ============================================================================
// Category 1: Basic Data Type Reading Tests (10 test cases)
// ============================================================================

// Test case 1: ReadVarint32_Success - Normal reading of varint32
void ManualPBParserUnittest::TestReadVarint32Success() {
    // Test various valid varint32 values
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> testCases = {{0, {0x00}},
                                                                        {1, {0x01}},
                                                                        {127, {0x7F}},
                                                                        {128, {0x80, 0x01}},
                                                                        {300, {0xAC, 0x02}},
                                                                        {16384, {0x80, 0x80, 0x01}},
                                                                        {0xFFFFFFFF, {0xFF, 0xFF, 0xFF, 0xFF, 0x0F}}};

    for (const auto& testCase : testCases) {
        const uint8_t* data = testCase.second.data();
        ManualPBParser parser(data, testCase.second.size(), false);
        uint32_t value = 0;

        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, value),
                              "Should successfully read varint32");
        APSARA_TEST_EQUAL_DESC(
            testCase.first, value, std::string("Value should match expected: ") + std::to_string(testCase.first));
    }
}

// Test case 2: ReadVarint32_MaxValue - Read maximum value
void ManualPBParserUnittest::TestReadVarint32MaxValue() {
    // Maximum 32-bit value: 0xFFFFFFFF
    auto encoded = encodeVarint32(0xFFFFFFFF);
    ManualPBParser parser(encoded.data(), encoded.size(), false);
    uint32_t value = 0;

    APSARA_TEST_TRUE(ManualPBParserTestHelper::TestReadVarint32(parser, value));
    APSARA_TEST_EQUAL(0xFFFFFFFF, value);
}

// Test case 3: ReadVarint32_Invalid - Invalid varint32 encoding
void ManualPBParserUnittest::TestReadVarint32Invalid() {
    // Test case 1: Truncated varint (missing bytes)
    {
        std::vector<uint8_t> data = {0x80, 0x80}; // Incomplete varint
        ManualPBParser parser(data.data(), data.size(), false);
        uint32_t value = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, value),
                               "Should fail on incomplete varint");
    }

    // Test case 2: Varint with too many bytes (exceeds 32-bit limit)
    {
        std::vector<uint8_t> data = {0x80, 0x80, 0x80, 0x80, 0x80, 0x01}; // More than 5 bytes
        ManualPBParser parser(data.data(), data.size(), false);
        uint32_t value = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, value),
                               "Should fail on oversized varint");
    }

    // Test case 3: Empty data
    {
        ManualPBParser parser(nullptr, 0, false);
        uint32_t value = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, value), "Should fail on empty data");
    }
}

// Test case 4: ReadVarint64_Success - Normal reading of varint64
void ManualPBParserUnittest::TestReadVarint64Success() {
    std::vector<std::pair<uint64_t, std::vector<uint8_t>>> testCases
        = {{0, {0x00}},
           {1, {0x01}},
           {127, {0x7F}},
           {128, {0x80, 0x01}},
           {0x7FFFFFFF, {0xFF, 0xFF, 0xFF, 0xFF, 0x07}},
           {0xFFFFFFFF, {0xFF, 0xFF, 0xFF, 0xFF, 0x0F}},
           {0x100000000ULL, {0x80, 0x80, 0x80, 0x80, 0x10}}};

    for (const auto& testCase : testCases) {
        const uint8_t* data = testCase.second.data();
        ManualPBParser parser(data, testCase.second.size(), false);
        uint64_t value = 0;

        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadVarint64(parser, value),
                              "Should successfully read varint64");
        APSARA_TEST_EQUAL_DESC(
            testCase.first, value, std::string("Value should match expected: ") + std::to_string(testCase.first));
    }
}

// Test case 5: ReadVarint64_MaxValue - Read maximum value
void ManualPBParserUnittest::TestReadVarint64MaxValue() {
    // Maximum 64-bit value: 0xFFFFFFFFFFFFFFFF
    auto encoded = encodeVarint64(0xFFFFFFFFFFFFFFFFULL);
    ManualPBParser parser(encoded.data(), encoded.size(), false);
    uint64_t value = 0;

    APSARA_TEST_TRUE(ManualPBParserTestHelper::TestReadVarint64(parser, value));
    APSARA_TEST_EQUAL(0xFFFFFFFFFFFFFFFFULL, value);
}

// Test case 6: ReadFixed32_Success - Normal reading of fixed32
void ManualPBParserUnittest::TestReadFixed32Success() {
    std::vector<uint32_t> testValues = {0x00000000, 0x00000001, 0x12345678, 0xABCDEF00, 0xFFFFFFFF};

    for (uint32_t expectedValue : testValues) {
        auto encoded = encodeFixed32(expectedValue);
        ManualPBParser parser(encoded.data(), encoded.size(), false);
        uint32_t value = 0;

        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadFixed32(parser, value),
                              "Should successfully read fixed32");
        APSARA_TEST_EQUAL_DESC(
            expectedValue, value, std::string("Value should match expected: 0x") + std::to_string(expectedValue));
    }
}

// Test case 7: ReadFixed32_InsufficientData - Insufficient data
void ManualPBParserUnittest::TestReadFixed32InsufficientData() {
    // Test case 1: Only 3 bytes available
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        ManualPBParser parser(data.data(), data.size(), false);
        uint32_t value = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadFixed32(parser, value),
                               "Should fail with only 3 bytes");
    }

    // Test case 2: No data available
    {
        ManualPBParser parser(nullptr, 0, false);
        uint32_t value = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadFixed32(parser, value), "Should fail with no data");
    }

    // Test case 3: Only 1 byte available
    {
        std::vector<uint8_t> data = {0x01};
        ManualPBParser parser(data.data(), data.size(), false);
        uint32_t value = 0;

        APSARA_TEST_FALSE(ManualPBParserTestHelper::TestReadFixed32(parser, value));
    }
}

// Test case 8: ReadFixed64_Success - Normal reading of fixed64
void ManualPBParserUnittest::TestReadFixed64Success() {
    std::vector<uint64_t> testValues
        = {0x0000000000000000ULL, 0x0000000000000001ULL, 0x123456789ABCDEF0ULL, 0xFFFFFFFFFFFFFFFFULL};

    for (uint64_t expectedValue : testValues) {
        auto encoded = encodeFixed64(expectedValue);
        ManualPBParser parser(encoded.data(), encoded.size(), false);
        uint64_t value = 0;

        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadFixed64(parser, value),
                              "Should successfully read fixed64");
        APSARA_TEST_EQUAL_DESC(
            expectedValue, value, std::string("Value should match expected: 0x") + std::to_string(expectedValue));
    }
}

// Test case 9: ReadLengthDelimited_Success - Normal reading of length-delimited data
void ManualPBParserUnittest::TestReadLengthDelimitedSuccess() {
    std::vector<std::string> testStrings = {
        "",
        "a",
        "Hello",
        "Hello, World!",
        std::string(100, 'x'), // 100-character string
        std::string(1000, 'y') // 1000-character string
    };

    for (const auto& testStr : testStrings) {
        auto encoded = encodeLengthDelimited(testStr);
        ManualPBParser parser(encoded.data(), encoded.size(), false);

        const uint8_t* data = nullptr;
        size_t length = 0;

        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadLengthDelimited(parser, data, length),
                              "Should successfully read length-delimited data");
        APSARA_TEST_EQUAL_DESC(
            testStr.length(), length, std::string("Length should match: ") + std::to_string(testStr.length()));

        if (length > 0 && data != nullptr) {
            std::string result(static_cast<const char*>(static_cast<const void*>(data)), length);
            APSARA_TEST_EQUAL_DESC(testStr, result, std::string("Data content should match"));
        }
    }
}

// Test case 10: ReadLengthDelimited_InsufficientData - Insufficient data
void ManualPBParserUnittest::TestReadLengthDelimitedInsufficientData() {
    // Test case 1: Length indicates 10 bytes but only 5 available
    {
        std::vector<uint8_t> data;
        data.push_back(10); // Length = 10
        data.insert(data.end(), 5, 'x'); // Only 5 bytes of data

        ManualPBParser parser(data.data(), data.size(), false);
        const uint8_t* resultData = nullptr;
        size_t length = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadLengthDelimited(parser, resultData, length),
                               "Should fail when data is insufficient");
    }

    // Test case 2: Incomplete length field
    {
        std::vector<uint8_t> data = {0x80}; // Incomplete varint length
        ManualPBParser parser(data.data(), data.size(), false);
        const uint8_t* resultData = nullptr;
        size_t length = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadLengthDelimited(parser, resultData, length),
                               "Should fail with incomplete length field");
    }

    // Test case 3: Length field present but no data
    {
        std::vector<uint8_t> data = {0x05}; // Length = 5, but no data follows
        ManualPBParser parser(data.data(), data.size(), false);
        const uint8_t* resultData = nullptr;
        size_t length = 0;

        APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadLengthDelimited(parser, resultData, length),
                               "Should fail when length specified but no data available");
    }
}

// ============================================================================
// Category 2: Error Input Handling Tests (8 test cases)
// ============================================================================

// Test case 11: ParsePipelineEventGroup_NullData - null input
void ManualPBParserUnittest::TestParsePipelineEventGroupNullData() {
    ManualPBParser parser(nullptr, 0, false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg), "Should fail with null data");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
    APSARA_TEST_TRUE_DESC(errMsg.find("Empty or null") != string::npos, "Error message should mention empty or null");
}

// Test case 12: ParsePipelineEventGroup_EmptyData - empty data
void ManualPBParserUnittest::TestParsePipelineEventGroupEmptyData() {
    vector<uint8_t> data; // Empty data
    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg), "Should fail with empty data");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
}

// Test case 13: ParsePipelineEventGroup_InvalidTag - invalid tag
void ManualPBParserUnittest::TestParsePipelineEventGroupInvalidTag() {
    // Create data with incomplete varint tag
    vector<uint8_t> data = {0x80, 0x80, 0x80, 0x80, 0x80}; // Invalid varint (too many bytes)
    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg), "Should fail with invalid tag");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
    APSARA_TEST_TRUE_DESC(errMsg.find("Failed to read field tag") != string::npos,
                          "Error message should mention failed to read field tag");
}

// Test case 14: ParsePipelineEventGroup_InvalidWireType - wrong wire type for unknown field
void ManualPBParserUnittest::TestParsePipelineEventGroupInvalidWireType() {
    // Use an unknown field number (999) with deprecated wire type (3 - start group)
    vector<uint8_t> data;
    uint32_t tag = encodeTag(999, 3); // field 999, wire type 3 (deprecated group)
    auto tagBytes = encodeVarint32(tag);
    data.insert(data.end(), tagBytes.begin(), tagBytes.end());

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg), "Should fail with unknown wire type");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
}

// Test case 15: ParseMetadata_InvalidWireType - Metadata field with wrong wire type
void ManualPBParserUnittest::TestParseMetadataInvalidWireType() {
    // Metadata field (field number 1) should be length-delimited (wire type 2)
    // but we'll use varint (wire type 0)
    vector<uint8_t> data;
    uint32_t tag = encodeTag(1, 0); // field 1 (Metadata), wire type 0 (varint) - WRONG!
    auto tagBytes = encodeVarint32(tag);
    data.insert(data.end(), tagBytes.begin(), tagBytes.end());
    data.push_back(0x00); // Some varint value

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid wire type for Metadata");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
    APSARA_TEST_TRUE_DESC(errMsg.find("Invalid wire type for Metadata") != string::npos,
                          "Error message should mention Metadata wire type");
}

// Test case 16: ParseTags_InvalidWireType - Tags field with wrong wire type
void ManualPBParserUnittest::TestParseTagsInvalidWireType() {
    // Tags field (field number 2) should be length-delimited (wire type 2)
    // but we'll use fixed32 (wire type 5)
    vector<uint8_t> data;
    uint32_t tag = encodeTag(2, 5); // field 2 (Tags), wire type 5 (fixed32) - WRONG!
    auto tagBytes = encodeVarint32(tag);
    data.insert(data.end(), tagBytes.begin(), tagBytes.end());
    auto fixedBytes = encodeFixed32(0x12345678);
    data.insert(data.end(), fixedBytes.begin(), fixedBytes.end());

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid wire type for Tags");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
    APSARA_TEST_TRUE_DESC(errMsg.find("Invalid wire type for Tags") != string::npos,
                          "Error message should mention Tags wire type");
}

// Test case 17: ParseLogEvents_InvalidWireType - Logs field with wrong wire type
void ManualPBParserUnittest::TestParseLogEventsInvalidWireType() {
    // Logs field (field number 3) should be length-delimited (wire type 2)
    // but we'll use fixed64 (wire type 1)
    vector<uint8_t> data;
    uint32_t tag = encodeTag(3, 1); // field 3 (Logs), wire type 1 (fixed64) - WRONG!
    auto tagBytes = encodeVarint32(tag);
    data.insert(data.end(), tagBytes.begin(), tagBytes.end());
    auto fixedBytes = encodeFixed64(0x123456789ABCDEF0ULL);
    data.insert(data.end(), fixedBytes.begin(), fixedBytes.end());

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid wire type for Logs");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
    APSARA_TEST_TRUE_DESC(errMsg.find("Invalid wire type for Logs") != string::npos,
                          "Error message should mention Logs wire type");
}

// Test case 18: ParseMetricEvents_InvalidWireType - Metrics field with wrong wire type
void ManualPBParserUnittest::TestParseMetricEventsInvalidWireType() {
    // Metrics field (field number 4) should be length-delimited (wire type 2)
    // but we'll use varint (wire type 0)
    vector<uint8_t> data;
    uint32_t tag = encodeTag(4, 0); // field 4 (Metrics), wire type 0 (varint) - WRONG!
    auto tagBytes = encodeVarint32(tag);
    data.insert(data.end(), tagBytes.begin(), tagBytes.end());
    data.push_back(0x01); // Some varint value

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid wire type for Metrics");
    APSARA_TEST_TRUE_DESC(!errMsg.empty(), "Error message should not be empty");
    APSARA_TEST_TRUE_DESC(errMsg.find("Invalid wire type for Metrics") != string::npos,
                          "Error message should mention Metrics wire type");
}

// ============================================================================
// Category 3: LogEvent Complete Tests (8 test cases)
// ============================================================================

// Test case 19: ParseLogEvent_Complete - Complete LogEvent with all fields
void ManualPBParserUnittest::TestParseLogEventComplete() {
    // Create a complete LogEvent with all fields
    vector<pair<string, string>> contents = {{"key1", "value1"}, {"key2", "value2"}, {"message", "test log message"}};
    auto logEventData = encodeLogEvent(1234567890, contents, "INFO", 1024, 512);

    vector<vector<uint8_t>> logEvents = {logEventData};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse complete LogEvent");
    APSARA_TEST_EQUAL_DESC(1U, eventGroup.GetEvents().size(), std::string("Should have 1 event"));

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(1234567890ULL, logEvent.GetTimestamp());
    APSARA_TEST_EQUAL("INFO", logEvent.GetLevel().to_string());

    // Verify contents
    APSARA_TEST_TRUE(logEvent.HasContent("key1"));
    APSARA_TEST_EQUAL("value1", logEvent.GetContent("key1").to_string());
    APSARA_TEST_TRUE(logEvent.HasContent("key2"));
    APSARA_TEST_EQUAL("value2", logEvent.GetContent("key2").to_string());
    APSARA_TEST_TRUE(logEvent.HasContent("message"));
    APSARA_TEST_EQUAL("test log message", logEvent.GetContent("message").to_string());
}

// Test case 20: ParseLogEvent_MinimalFields - LogEvent with only timestamp
void ManualPBParserUnittest::TestParseLogEventMinimalFields() {
    // Create a minimal LogEvent with only timestamp (no contents)
    vector<pair<string, string>> contents; // Empty contents
    auto logEventData = encodeLogEvent(9876543210ULL, contents);

    vector<vector<uint8_t>> logEvents = {logEventData};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse minimal LogEvent");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(9876543210ULL, logEvent.GetTimestamp());
}

// Test case 21: ParseLogEvent_WithContents - LogEvent with multiple contents
void ManualPBParserUnittest::TestParseLogEventWithContents() {
    // Create LogEvent with multiple content key-value pairs
    vector<pair<string, string>> contents = {{"host", "192.168.1.100"},
                                             {"method", "GET"},
                                             {"path", "/api/v1/users"},
                                             {"status", "200"},
                                             {"response_time", "25ms"}};
    auto logEventData = encodeLogEvent(1111111111ULL, contents);

    vector<vector<uint8_t>> logEvents = {logEventData};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();

    // Verify all contents
    APSARA_TEST_EQUAL("192.168.1.100", logEvent.GetContent("host").to_string());
    APSARA_TEST_EQUAL("GET", logEvent.GetContent("method").to_string());
    APSARA_TEST_EQUAL("/api/v1/users", logEvent.GetContent("path").to_string());
    APSARA_TEST_EQUAL("200", logEvent.GetContent("status").to_string());
    APSARA_TEST_EQUAL("25ms", logEvent.GetContent("response_time").to_string());
}

// Test case 22: ParseLogEvent_WithLevel - LogEvent with level field
void ManualPBParserUnittest::TestParseLogEventWithLevel() {
    vector<string> levels = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

    for (const auto& level : levels) {
        vector<pair<string, string>> contents = {{"message", "Test message"}};
        auto logEventData = encodeLogEvent(1000000000ULL, contents, level);

        vector<vector<uint8_t>> logEvents = {logEventData};
        auto data = encodePipelineEventGroupWithLogs(logEvents);

        ManualPBParser parser(data.data(), data.size(), false);
        auto sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        string errMsg;

        APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
        APSARA_TEST_EQUAL_DESC(level, logEvent.GetLevel().to_string(), std::string("Level should match: ") + level);
    }
}

// Test case 23: ParseLogEvent_WithFileOffset - LogEvent with file offset
void ManualPBParserUnittest::TestParseLogEventWithFileOffset() {
    vector<pair<string, string>> contents = {{"line", "1"}};
    uint64_t fileOffset = 123456789ULL;
    auto logEventData = encodeLogEvent(1500000000ULL, contents, "", fileOffset, 0);

    vector<vector<uint8_t>> logEvents = {logEventData};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    // Note: The actual verification depends on LogEvent API for getting file offset
    // We just verify the event was created successfully
    (void)logEvent; // Suppress unused variable warning
}

// Test case 24: ParseLogEvent_WithRawSize - LogEvent with raw size
void ManualPBParserUnittest::TestParseLogEventWithRawSize() {
    vector<pair<string, string>> contents = {{"data", "sample"}};
    uint64_t rawSize = 4096ULL;
    auto logEventData = encodeLogEvent(1600000000ULL, contents, "", 0, rawSize);

    vector<vector<uint8_t>> logEvents = {logEventData};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    // Event was created successfully
    (void)logEvent; // Suppress unused variable warning
}

// Test case 25: ParseLogEvent_InvalidContentWireType - Content with wrong wire type
void ManualPBParserUnittest::TestParseLogEventInvalidContentWireType() {
    // Manually construct LogEvent with invalid content wire type
    vector<uint8_t> logEventData;

    // field 1: timestamp
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    logEventData.insert(logEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(1234567890ULL);
    logEventData.insert(logEventData.end(), timestampData.begin(), timestampData.end());

    // field 2: content with WRONG wire type (varint instead of length-delimited)
    auto contentTag = encodeVarint32(encodeTag(2, 0)); // wire type 0 (varint) - WRONG!
    logEventData.insert(logEventData.end(), contentTag.begin(), contentTag.end());
    logEventData.push_back(0x01); // Some varint value

    vector<vector<uint8_t>> logEvents = {logEventData};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid content wire type");
    APSARA_TEST_TRUE(!errMsg.empty());
}

// Test case 26: ParseLogEvent_MultipleEvents - Multiple LogEvents
void ManualPBParserUnittest::TestParseLogEventMultipleEvents() {
    // Create multiple LogEvents
    vector<pair<string, string>> contents1 = {{"event", "first"}};
    auto logEvent1Data = encodeLogEvent(1000000000ULL, contents1, "INFO");

    vector<pair<string, string>> contents2 = {{"event", "second"}};
    auto logEvent2Data = encodeLogEvent(2000000000ULL, contents2, "WARN");

    vector<pair<string, string>> contents3 = {{"event", "third"}};
    auto logEvent3Data = encodeLogEvent(3000000000ULL, contents3, "ERROR");

    vector<vector<uint8_t>> logEvents = {logEvent1Data, logEvent2Data, logEvent3Data};
    auto data = encodePipelineEventGroupWithLogs(logEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL_DESC(3U, eventGroup.GetEvents().size(), std::string("Should have 3 events"));

    // Verify first event
    const auto& logEvent1 = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(1000000000ULL, logEvent1.GetTimestamp());
    APSARA_TEST_EQUAL("INFO", logEvent1.GetLevel().to_string());
    APSARA_TEST_EQUAL("first", logEvent1.GetContent("event").to_string());

    // Verify second event
    const auto& logEvent2 = eventGroup.GetEvents()[1].Cast<LogEvent>();
    APSARA_TEST_EQUAL(2000000000ULL, logEvent2.GetTimestamp());
    APSARA_TEST_EQUAL("WARN", logEvent2.GetLevel().to_string());
    APSARA_TEST_EQUAL("second", logEvent2.GetContent("event").to_string());

    // Verify third event
    const auto& logEvent3 = eventGroup.GetEvents()[2].Cast<LogEvent>();
    APSARA_TEST_EQUAL(3000000000ULL, logEvent3.GetTimestamp());
    APSARA_TEST_EQUAL("ERROR", logEvent3.GetLevel().to_string());
    APSARA_TEST_EQUAL("third", logEvent3.GetContent("event").to_string());
}

// ============================================================================
// Category 4: MetricEvent Complete Tests (8 test cases)
// ============================================================================

// Test case 27: ParseMetricEvent_Complete - Complete MetricEvent with all fields
void ManualPBParserUnittest::TestParseMetricEventComplete() {
    // Create a complete MetricEvent with all fields
    vector<pair<string, string>> tags = {{"host", "server1"}, {"region", "us-west"}, {"env", "prod"}};
    auto metricEventData = encodeMetricEvent(1234567890ULL, "cpu_usage", tags, 75.5, true);

    vector<vector<uint8_t>> metricEvents = {metricEventData};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse complete MetricEvent");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(1234567890ULL, metricEvent.GetTimestamp());
    APSARA_TEST_EQUAL("cpu_usage", metricEvent.GetName().to_string());

    // Verify tags
    APSARA_TEST_TRUE(metricEvent.HasTag("host"));
    APSARA_TEST_EQUAL("server1", metricEvent.GetTag("host").to_string());
    APSARA_TEST_TRUE(metricEvent.HasTag("region"));
    APSARA_TEST_EQUAL("us-west", metricEvent.GetTag("region").to_string());
    APSARA_TEST_TRUE(metricEvent.HasTag("env"));
    APSARA_TEST_EQUAL("prod", metricEvent.GetTag("env").to_string());

    // Verify value
    APSARA_TEST_EQUAL(75.5, metricEvent.GetValue<UntypedSingleValue>()->mValue);
}

// Test case 28: ParseMetricEvent_MinimalFields - MetricEvent with only timestamp and name
void ManualPBParserUnittest::TestParseMetricEventMinimalFields() {
    // Create a minimal MetricEvent with only timestamp and name
    vector<pair<string, string>> tags; // Empty tags
    auto metricEventData = encodeMetricEvent(9876543210ULL, "request_count", tags, 0.0, false);

    vector<vector<uint8_t>> metricEvents = {metricEventData};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(9876543210ULL, metricEvent.GetTimestamp());
    APSARA_TEST_EQUAL("request_count", metricEvent.GetName().to_string());
}

// Test case 29: ParseMetricEvent_WithTags - MetricEvent with multiple tags
void ManualPBParserUnittest::TestParseMetricEventWithTags() {
    // Create MetricEvent with multiple tags
    vector<pair<string, string>> tags = {{"service", "web-api"},
                                         {"instance", "i-1234567"},
                                         {"az", "az-1a"},
                                         {"version", "v2.1.0"},
                                         {"team", "platform"}};
    auto metricEventData = encodeMetricEvent(1111111111ULL, "response_time", tags, 0.0, false);

    vector<vector<uint8_t>> metricEvents = {metricEventData};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();

    // Verify all tags
    APSARA_TEST_EQUAL("web-api", metricEvent.GetTag("service").to_string());
    APSARA_TEST_EQUAL("i-1234567", metricEvent.GetTag("instance").to_string());
    APSARA_TEST_EQUAL("az-1a", metricEvent.GetTag("az").to_string());
    APSARA_TEST_EQUAL("v2.1.0", metricEvent.GetTag("version").to_string());
    APSARA_TEST_EQUAL("platform", metricEvent.GetTag("team").to_string());
}

// Test case 30: ParseMetricEvent_WithValue - MetricEvent with untyped_single_value
void ManualPBParserUnittest::TestParseMetricEventWithValue() {
    vector<pair<string, string>> tags = {{"metric", "test"}};
    auto metricEventData = encodeMetricEvent(2000000000ULL, "memory_usage", tags, 85.25, true);

    vector<vector<uint8_t>> metricEvents = {metricEventData};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL("memory_usage", metricEvent.GetName().to_string());
    APSARA_TEST_EQUAL(85.25, metricEvent.GetValue<UntypedSingleValue>()->mValue);
}

// Test case 31: ParseMetricEvent_DoubleValue - Test double value conversion (IEEE 754)
void ManualPBParserUnittest::TestParseMetricEventDoubleValue() {
    // Test various double values including edge cases
    vector<double> testValues = {0.0, 1.0, -1.0, 3.14159265358979, 123.456789, -999.999, 1e10, 1e-10, 99999.99999};

    for (double expectedValue : testValues) {
        vector<pair<string, string>> tags = {{"test", "value"}};
        auto metricEventData = encodeMetricEvent(3000000000ULL, "test_metric", tags, expectedValue, true);

        vector<vector<uint8_t>> metricEvents = {metricEventData};
        auto data = encodePipelineEventGroupWithMetrics(metricEvents);

        ManualPBParser parser(data.data(), data.size(), false);
        auto sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        string errMsg;

        APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();
        APSARA_TEST_EQUAL_DESC(expectedValue,
                               metricEvent.GetValue<UntypedSingleValue>()->mValue,
                               std::string("Value should match: ") + std::to_string(expectedValue));
    }
}

// Test case 32: ParseMetricEvent_InvalidTagsWireType - Tags with wrong wire type
void ManualPBParserUnittest::TestParseMetricEventInvalidTagsWireType() {
    // Manually construct MetricEvent with invalid tags wire type
    vector<uint8_t> metricEventData;

    // field 1: timestamp
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    metricEventData.insert(metricEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(1234567890ULL);
    metricEventData.insert(metricEventData.end(), timestampData.begin(), timestampData.end());

    // field 2: name
    auto nameTag = encodeVarint32(encodeTag(2, 2));
    metricEventData.insert(metricEventData.end(), nameTag.begin(), nameTag.end());
    auto nameData = encodeLengthDelimited("test_metric");
    metricEventData.insert(metricEventData.end(), nameData.begin(), nameData.end());

    // field 3: tag with WRONG wire type (varint instead of length-delimited)
    auto tagTag = encodeVarint32(encodeTag(3, 0)); // wire type 0 (varint) - WRONG!
    metricEventData.insert(metricEventData.end(), tagTag.begin(), tagTag.end());
    metricEventData.push_back(0x01); // Some varint value

    vector<vector<uint8_t>> metricEvents = {metricEventData};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid tags wire type");
    APSARA_TEST_TRUE(!errMsg.empty());
}

// Test case 33: ParseMetricEvent_InvalidValueWireType - Value with wrong wire type
void ManualPBParserUnittest::TestParseMetricEventInvalidValueWireType() {
    // Manually construct MetricEvent with invalid value wire type
    vector<uint8_t> metricEventData;

    // field 1: timestamp
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    metricEventData.insert(metricEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(1234567890ULL);
    metricEventData.insert(metricEventData.end(), timestampData.begin(), timestampData.end());

    // field 2: name
    auto nameTag = encodeVarint32(encodeTag(2, 2));
    metricEventData.insert(metricEventData.end(), nameTag.begin(), nameTag.end());
    auto nameData = encodeLengthDelimited("test_metric");
    metricEventData.insert(metricEventData.end(), nameData.begin(), nameData.end());

    // field 4: untyped_single_value with WRONG wire type (varint instead of length-delimited)
    auto valueTag = encodeVarint32(encodeTag(4, 0)); // wire type 0 (varint) - WRONG!
    metricEventData.insert(metricEventData.end(), valueTag.begin(), valueTag.end());
    metricEventData.push_back(0x01); // Some varint value

    vector<vector<uint8_t>> metricEvents = {metricEventData};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid value wire type");
    APSARA_TEST_TRUE(!errMsg.empty());
}

// Test case 34: ParseMetricEvent_MultipleEvents - Multiple MetricEvents
void ManualPBParserUnittest::TestParseMetricEventMultipleEvents() {
    // Create multiple MetricEvents
    vector<pair<string, string>> tags1 = {{"host", "server1"}};
    auto metricEvent1Data = encodeMetricEvent(1000000000ULL, "cpu_usage", tags1, 50.0, true);

    vector<pair<string, string>> tags2 = {{"host", "server2"}};
    auto metricEvent2Data = encodeMetricEvent(2000000000ULL, "memory_usage", tags2, 70.0, true);

    vector<pair<string, string>> tags3 = {{"host", "server3"}};
    auto metricEvent3Data = encodeMetricEvent(3000000000ULL, "disk_usage", tags3, 90.0, true);

    vector<vector<uint8_t>> metricEvents = {metricEvent1Data, metricEvent2Data, metricEvent3Data};
    auto data = encodePipelineEventGroupWithMetrics(metricEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(3U, eventGroup.GetEvents().size());

    // Verify first event
    const auto& metricEvent1 = eventGroup.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(1000000000ULL, metricEvent1.GetTimestamp());
    APSARA_TEST_EQUAL("cpu_usage", metricEvent1.GetName().to_string());
    APSARA_TEST_EQUAL("server1", metricEvent1.GetTag("host").to_string());
    APSARA_TEST_EQUAL(50.0, metricEvent1.GetValue<UntypedSingleValue>()->mValue);

    // Verify second event
    const auto& metricEvent2 = eventGroup.GetEvents()[1].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(2000000000ULL, metricEvent2.GetTimestamp());
    APSARA_TEST_EQUAL("memory_usage", metricEvent2.GetName().to_string());
    APSARA_TEST_EQUAL("server2", metricEvent2.GetTag("host").to_string());
    APSARA_TEST_EQUAL(70.0, metricEvent2.GetValue<UntypedSingleValue>()->mValue);

    // Verify third event
    const auto& metricEvent3 = eventGroup.GetEvents()[2].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(3000000000ULL, metricEvent3.GetTimestamp());
    APSARA_TEST_EQUAL("disk_usage", metricEvent3.GetName().to_string());
    APSARA_TEST_EQUAL("server3", metricEvent3.GetTag("host").to_string());
    APSARA_TEST_EQUAL(90.0, metricEvent3.GetValue<UntypedSingleValue>()->mValue);
}

// ============================================================================
// Category 5: SpanEvent Complete Tests (8 test cases)
// ============================================================================

// Test case 35: ParseSpanEvent_Complete - Complete SpanEvent with all fields
void ManualPBParserUnittest::TestParseSpanEventComplete() {
    // Create a complete SpanEvent with all fields
    vector<pair<string, string>> tags = {{"http.method", "GET"}, {"http.url", "/api/users"}};
    vector<pair<string, string>> scopeTags = {{"service.name", "api-service"}};
    auto spanEventData = encodeSpanEvent(1234567890ULL, // timestamp
                                         "trace-123", // trace_id
                                         "span-456", // span_id
                                         "HTTP GET /api/users", // name
                                         3, // kind (Client)
                                         1000000000ULL, // start_time_ns
                                         2000000000ULL, // end_time_ns
                                         tags, // tags
                                         scopeTags, // scope_tags
                                         "congo=abcde", // trace_state
                                         "parent-789", // parent_span_id
                                         1); // status (Ok)

    vector<vector<uint8_t>> spanEvents = {spanEventData};
    auto data = encodePipelineEventGroupWithSpans(spanEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse complete SpanEvent");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL("trace-123", spanEvent.GetTraceId().to_string());
    APSARA_TEST_EQUAL("span-456", spanEvent.GetSpanId().to_string());
    APSARA_TEST_EQUAL("HTTP GET /api/users", spanEvent.GetName().to_string());
    APSARA_TEST_EQUAL(SpanEvent::Kind::Client, spanEvent.GetKind());
    APSARA_TEST_EQUAL(1000000000ULL, spanEvent.GetStartTimeNs());
    APSARA_TEST_EQUAL(2000000000ULL, spanEvent.GetEndTimeNs());
    APSARA_TEST_EQUAL("congo=abcde", spanEvent.GetTraceState().to_string());
    APSARA_TEST_EQUAL("parent-789", spanEvent.GetParentSpanId().to_string());
    APSARA_TEST_EQUAL(SpanEvent::StatusCode::Ok, spanEvent.GetStatus());

    // Verify tags
    APSARA_TEST_EQUAL("GET", spanEvent.GetTag("http.method").to_string());
    APSARA_TEST_EQUAL("/api/users", spanEvent.GetTag("http.url").to_string());

    // Verify scope tags
    APSARA_TEST_EQUAL("api-service", spanEvent.GetScopeTag("service.name").to_string());
}

// Test case 36: ParseSpanEvent_MinimalFields - SpanEvent with minimal fields
void ManualPBParserUnittest::TestParseSpanEventMinimalFields() {
    // Create a minimal SpanEvent
    auto spanEventData = encodeSpanEvent(9876543210ULL, // timestamp
                                         "trace-abc", // trace_id
                                         "span-def", // span_id
                                         "minimal-span", // name
                                         0, // kind (Unspecified)
                                         500000000ULL, // start_time_ns
                                         600000000ULL); // end_time_ns

    vector<vector<uint8_t>> spanEvents = {spanEventData};
    auto data = encodePipelineEventGroupWithSpans(spanEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL("trace-abc", spanEvent.GetTraceId().to_string());
    APSARA_TEST_EQUAL("span-def", spanEvent.GetSpanId().to_string());
    APSARA_TEST_EQUAL("minimal-span", spanEvent.GetName().to_string());
}

// Test case 37: ParseSpanEvent_WithTraceInfo - SpanEvent with full trace information
void ManualPBParserUnittest::TestParseSpanEventWithTraceInfo() {
    auto spanEventData = encodeSpanEvent(1111111111ULL,
                                         "00000000000000000000000000000001",
                                         "0000000000000001",
                                         "test-span",
                                         1, // Internal
                                         100000000ULL,
                                         200000000ULL,
                                         {}, // no tags
                                         {}, // no scope tags
                                         "rojo=00,congo=1234", // trace_state
                                         "0000000000000000"); // parent_span_id

    vector<vector<uint8_t>> spanEvents = {spanEventData};
    auto data = encodePipelineEventGroupWithSpans(spanEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL("00000000000000000000000000000001", spanEvent.GetTraceId().to_string());
    APSARA_TEST_EQUAL("0000000000000001", spanEvent.GetSpanId().to_string());
    APSARA_TEST_EQUAL("rojo=00,congo=1234", spanEvent.GetTraceState().to_string());
    APSARA_TEST_EQUAL("0000000000000000", spanEvent.GetParentSpanId().to_string());
}

// Test case 38: ParseSpanEvent_AllKinds - Test all SpanEvent Kind enum values
void ManualPBParserUnittest::TestParseSpanEventAllKinds() {
    // Test all Kind enum values: Unspecified=0, Internal=1, Server=2, Client=3, Producer=4, Consumer=5
    vector<pair<uint32_t, SpanEvent::Kind>> kindTests = {{0, SpanEvent::Kind::Unspecified},
                                                         {1, SpanEvent::Kind::Internal},
                                                         {2, SpanEvent::Kind::Server},
                                                         {3, SpanEvent::Kind::Client},
                                                         {4, SpanEvent::Kind::Producer},
                                                         {5, SpanEvent::Kind::Consumer}};

    for (const auto& kindTest : kindTests) {
        auto spanEventData
            = encodeSpanEvent(2000000000ULL, "trace-kind", "span-kind", "kind-test", kindTest.first, 100ULL, 200ULL);

        vector<vector<uint8_t>> spanEvents = {spanEventData};
        auto data = encodePipelineEventGroupWithSpans(spanEvents);

        ManualPBParser parser(data.data(), data.size(), false);
        auto sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        string errMsg;

        APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
        APSARA_TEST_EQUAL(kindTest.second, spanEvent.GetKind());
    }
}

// Test case 39: ParseSpanEvent_AllStatus - Test all SpanEvent Status enum values
void ManualPBParserUnittest::TestParseSpanEventAllStatus() {
    // Test all Status enum values: Unset=0, Ok=1, Error=2
    vector<pair<uint32_t, SpanEvent::StatusCode>> statusTests
        = {{0, SpanEvent::StatusCode::Unset}, {1, SpanEvent::StatusCode::Ok}, {2, SpanEvent::StatusCode::Error}};

    for (const auto& statusTest : statusTests) {
        auto spanEventData = encodeSpanEvent(3000000000ULL,
                                             "trace-status",
                                             "span-status",
                                             "status-test",
                                             0,
                                             100ULL,
                                             200ULL,
                                             {}, // no tags
                                             {}, // no scope tags
                                             "", // no trace state
                                             "", // no parent span id
                                             statusTest.first);

        vector<vector<uint8_t>> spanEvents = {spanEventData};
        auto data = encodePipelineEventGroupWithSpans(spanEvents);

        ManualPBParser parser(data.data(), data.size(), false);
        auto sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        string errMsg;

        APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
        APSARA_TEST_EQUAL(statusTest.second, spanEvent.GetStatus());
    }
}

// Test case 40: ParseSpanEvent_WithTags - SpanEvent with multiple tags
void ManualPBParserUnittest::TestParseSpanEventWithTags() {
    vector<pair<string, string>> tags = {{"http.method", "POST"},
                                         {"http.status_code", "201"},
                                         {"http.url", "/api/orders"},
                                         {"db.system", "postgresql"},
                                         {"db.name", "mydb"}};

    auto spanEventData = encodeSpanEvent(4000000000ULL,
                                         "trace-tags",
                                         "span-tags",
                                         "tag-test",
                                         2, // Server
                                         100ULL,
                                         200ULL,
                                         tags);

    vector<vector<uint8_t>> spanEvents = {spanEventData};
    auto data = encodePipelineEventGroupWithSpans(spanEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();

    APSARA_TEST_EQUAL("POST", spanEvent.GetTag("http.method").to_string());
    APSARA_TEST_EQUAL("201", spanEvent.GetTag("http.status_code").to_string());
    APSARA_TEST_EQUAL("/api/orders", spanEvent.GetTag("http.url").to_string());
    APSARA_TEST_EQUAL("postgresql", spanEvent.GetTag("db.system").to_string());
    APSARA_TEST_EQUAL("mydb", spanEvent.GetTag("db.name").to_string());
}

// Test case 41: ParseSpanEvent_WithScopeTags - SpanEvent with scope tags
void ManualPBParserUnittest::TestParseSpanEventWithScopeTags() {
    vector<pair<string, string>> scopeTags = {{"service.name", "my-service"},
                                              {"service.namespace", "production"},
                                              {"service.version", "1.2.3"},
                                              {"telemetry.sdk.name", "opentelemetry"},
                                              {"telemetry.sdk.language", "cpp"}};

    auto spanEventData = encodeSpanEvent(5000000000ULL,
                                         "trace-scope",
                                         "span-scope",
                                         "scope-test",
                                         0,
                                         100ULL,
                                         200ULL,
                                         {}, // no tags
                                         scopeTags);

    vector<vector<uint8_t>> spanEvents = {spanEventData};
    auto data = encodePipelineEventGroupWithSpans(spanEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();

    APSARA_TEST_EQUAL("my-service", spanEvent.GetScopeTag("service.name").to_string());
    APSARA_TEST_EQUAL("production", spanEvent.GetScopeTag("service.namespace").to_string());
    APSARA_TEST_EQUAL("1.2.3", spanEvent.GetScopeTag("service.version").to_string());
    APSARA_TEST_EQUAL("opentelemetry", spanEvent.GetScopeTag("telemetry.sdk.name").to_string());
    APSARA_TEST_EQUAL("cpp", spanEvent.GetScopeTag("telemetry.sdk.language").to_string());
}

// Test case 42: ParseSpanEvent_WithInnerEvents - SpanEvent with inner events
void ManualPBParserUnittest::TestParseSpanEventWithInnerEvents() {
    // Create inner events
    vector<pair<string, string>> innerEventTags1 = {{"error", "timeout"}};
    auto innerEvent1 = encodeSpanInnerEvent(150000000ULL, "connection-timeout", innerEventTags1);

    vector<pair<string, string>> innerEventTags2 = {{"retry", "true"}};
    auto innerEvent2 = encodeSpanInnerEvent(160000000ULL, "retry-attempt", innerEventTags2);

    vector<vector<uint8_t>> innerEvents = {innerEvent1, innerEvent2};

    auto spanEventData = encodeSpanEvent(6000000000ULL,
                                         "trace-events",
                                         "span-events",
                                         "events-test",
                                         0,
                                         100000000ULL,
                                         200000000ULL,
                                         {}, // no tags
                                         {}, // no scope tags
                                         "", // no trace state
                                         "", // no parent span id
                                         0, // no status
                                         innerEvents);

    vector<vector<uint8_t>> spanEvents = {spanEventData};
    auto data = encodePipelineEventGroupWithSpans(spanEvents);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();

    APSARA_TEST_EQUAL(2U, spanEvent.GetEvents().size());

    // Verify first inner event
    const auto& event1 = spanEvent.GetEvents()[0];
    APSARA_TEST_EQUAL(150000000ULL, event1.GetTimestampNs());
    APSARA_TEST_EQUAL("connection-timeout", event1.GetName().to_string());
    APSARA_TEST_EQUAL("timeout", event1.GetTag("error").to_string());

    // Verify second inner event
    const auto& event2 = spanEvent.GetEvents()[1];
    APSARA_TEST_EQUAL(160000000ULL, event2.GetTimestampNs());
    APSARA_TEST_EQUAL("retry-attempt", event2.GetName().to_string());
    APSARA_TEST_EQUAL("true", event2.GetTag("retry").to_string());
}

// ============================================================================
// Category 6: Map Field Parsing Tests (8 test cases)
// ============================================================================

// Test case 43: ParseMapField_Empty - Empty map field (no entries)
void ManualPBParserUnittest::TestParseMapFieldEmpty() {
    // Create a PipelineEventGroup with a LogEvent but no metadata/tags
    // An empty byte array would be invalid, so we need at least one valid field
    auto logEventData = encodeLogEvent(1000ULL, {{"data", "test"}});
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse without metadata/tags fields");
    // No metadata/tags fields should result in empty tags
    APSARA_TEST_TRUE(eventGroup.GetTags().empty());
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test case 44: ParseMapField_SingleEntry - Map field with single entry
void ManualPBParserUnittest::TestParseMapFieldSingleEntry() {
    vector<pair<string, string>> metadata = {{"env", "production"}};
    auto data = encodePipelineEventGroupWithMetadata(metadata);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("production", eventGroup.GetTag("env").to_string());
}

// Test case 45: ParseMapField_MultipleEntries - Map field with multiple entries
void ManualPBParserUnittest::TestParseMapFieldMultipleEntries() {
    vector<pair<string, string>> tags = {{"region", "us-west-2"},
                                         {"cluster", "prod-cluster"},
                                         {"version", "v1.2.3"},
                                         {"team", "backend"},
                                         {"priority", "high"}};
    auto data = encodePipelineEventGroupWithTags(tags);

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(5U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("us-west-2", eventGroup.GetTag("region").to_string());
    APSARA_TEST_EQUAL("prod-cluster", eventGroup.GetTag("cluster").to_string());
    APSARA_TEST_EQUAL("v1.2.3", eventGroup.GetTag("version").to_string());
    APSARA_TEST_EQUAL("backend", eventGroup.GetTag("team").to_string());
    APSARA_TEST_EQUAL("high", eventGroup.GetTag("priority").to_string());
}

// Test case 46: ParseMapField_KeyOnlyNoValue - Map entry with only key, no value
void ManualPBParserUnittest::TestParseMapFieldKeyOnlyNoValue() {
    vector<uint8_t> result;

    // field 1: Metadata with only key
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());
    auto entryData = encodeMapEntryKeyOnly("key-only");
    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    // Map entry without both key and value should not be added
    APSARA_TEST_TRUE(eventGroup.GetTags().empty());
}

// Test case 47: ParseMapField_ValueOnlyNoKey - Map entry with only value, no key
void ManualPBParserUnittest::TestParseMapFieldValueOnlyNoKey() {
    vector<uint8_t> result;

    // field 2: Tags with only value
    auto tagsTag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tagsTag.begin(), tagsTag.end());
    auto entryData = encodeMapEntryValueOnly("value-only");
    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    // Map entry without both key and value should not be added
    APSARA_TEST_TRUE(eventGroup.GetTags().empty());
}

// Test case 48: ParseMapField_InvalidKeyWireType - Map entry with invalid wire type for key
void ManualPBParserUnittest::TestParseMapFieldInvalidKeyWireType() {
    vector<uint8_t> result;

    // field 1: Metadata with invalid key wire type
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());
    auto entryData = encodeMapEntryInvalidKeyWireType();
    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail to parse map entry with invalid key wire type");
}

// Test case 49: ParseMapField_InvalidValueWireType - Map entry with invalid wire type for value
void ManualPBParserUnittest::TestParseMapFieldInvalidValueWireType() {
    vector<uint8_t> result;

    // field 2: Tags with invalid value wire type
    auto tagsTag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tagsTag.begin(), tagsTag.end());
    auto entryData = encodeMapEntryInvalidValueWireType();
    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail to parse map entry with invalid value wire type");
}

// Test case 50: ParseMapField_UnknownFieldNumber - Map entry with unknown field number
void ManualPBParserUnittest::TestParseMapFieldUnknownFieldNumber() {
    vector<uint8_t> result;

    // field 1: Metadata with unknown field number (should be skipped)
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());
    auto entryData = encodeMapEntryWithUnknownField("service", "api-server");
    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse map entry with unknown field (skip it)");
    APSARA_TEST_EQUAL(1U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("api-server", eventGroup.GetTag("service").to_string());
}

// ============================================================================
// Category 7: Unknown Field Handling Tests (5 test cases)
// ============================================================================

// Test case 51: SkipField_Varint - Test skipping varint wire type
void ManualPBParserUnittest::TestSkipFieldVarint() {
    vector<uint8_t> result;

    // field 999: unknown field with varint wire type (should be skipped)
    auto unknownTag = encodeVarint32(encodeTag(999, 0)); // wire type 0 = varint
    result.insert(result.end(), unknownTag.begin(), unknownTag.end());
    auto unknownData = encodeVarint64(0xFFFFFFFFFFFFFFFFULL); // large varint
    result.insert(result.end(), unknownData.begin(), unknownData.end());

    // field 1: valid metadata entry after unknown field
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());
    auto metadataData = encodeMapEntry("test", "value");
    auto metadataLen = encodeVarint32(metadataData.size());
    result.insert(result.end(), metadataLen.begin(), metadataLen.end());
    result.insert(result.end(), metadataData.begin(), metadataData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip unknown varint field and parse valid data");
    APSARA_TEST_EQUAL(1U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("value", eventGroup.GetTag("test").to_string());
}

// Test case 52: SkipField_Fixed32 - Test skipping fixed32 wire type
void ManualPBParserUnittest::TestSkipFieldFixed32() {
    vector<uint8_t> result;

    // field 888: unknown field with fixed32 wire type (should be skipped)
    auto unknownTag = encodeVarint32(encodeTag(888, 5)); // wire type 5 = fixed32
    result.insert(result.end(), unknownTag.begin(), unknownTag.end());
    auto unknownData = encodeFixed32(0x12345678);
    result.insert(result.end(), unknownData.begin(), unknownData.end());

    // field 2: valid tags entry after unknown field
    auto tagsTag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tagsTag.begin(), tagsTag.end());
    auto tagsData = encodeMapEntry("env", "prod");
    auto tagsLen = encodeVarint32(tagsData.size());
    result.insert(result.end(), tagsLen.begin(), tagsLen.end());
    result.insert(result.end(), tagsData.begin(), tagsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip unknown fixed32 field and parse valid data");
    APSARA_TEST_EQUAL(1U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("prod", eventGroup.GetTag("env").to_string());
}

// Test case 53: SkipField_Fixed64 - Test skipping fixed64 wire type
void ManualPBParserUnittest::TestSkipFieldFixed64() {
    vector<uint8_t> result;

    // field 777: unknown field with fixed64 wire type (should be skipped)
    auto unknownTag = encodeVarint32(encodeTag(777, 1)); // wire type 1 = fixed64
    result.insert(result.end(), unknownTag.begin(), unknownTag.end());
    auto unknownData = encodeFixed64(0x123456789ABCDEF0ULL);
    result.insert(result.end(), unknownData.begin(), unknownData.end());

    // field 1: valid metadata entry after unknown field
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());
    auto metadataData = encodeMapEntry("region", "us-east-1");
    auto metadataLen = encodeVarint32(metadataData.size());
    result.insert(result.end(), metadataLen.begin(), metadataLen.end());
    result.insert(result.end(), metadataData.begin(), metadataData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip unknown fixed64 field and parse valid data");
    APSARA_TEST_EQUAL(1U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("us-east-1", eventGroup.GetTag("region").to_string());
}

// Test case 54: SkipField_LengthDelimited - Test skipping length-delimited wire type
void ManualPBParserUnittest::TestSkipFieldLengthDelimited() {
    vector<uint8_t> result;

    // field 666: unknown field with length-delimited wire type (should be skipped)
    auto unknownTag = encodeVarint32(encodeTag(666, 2)); // wire type 2 = length-delimited
    result.insert(result.end(), unknownTag.begin(), unknownTag.end());
    auto unknownData = encodeLengthDelimited("this is a long unknown string that should be skipped");
    result.insert(result.end(), unknownData.begin(), unknownData.end());

    // field 2: valid tags entry after unknown field
    auto tagsTag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tagsTag.begin(), tagsTag.end());
    auto tagsData = encodeMapEntry("service", "web-api");
    auto tagsLen = encodeVarint32(tagsData.size());
    result.insert(result.end(), tagsLen.begin(), tagsLen.end());
    result.insert(result.end(), tagsData.begin(), tagsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip unknown length-delimited field and parse valid data");
    APSARA_TEST_EQUAL(1U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("web-api", eventGroup.GetTag("service").to_string());
}

// Test case 55: UnknownField_InPipelineEventGroup - Multiple unknown fields interspersed with known fields
void ManualPBParserUnittest::TestUnknownFieldInPipelineEventGroup() {
    vector<uint8_t> result;

    // field 100: unknown varint field
    auto unknown1Tag = encodeVarint32(encodeTag(100, 0));
    result.insert(result.end(), unknown1Tag.begin(), unknown1Tag.end());
    auto unknown1Data = encodeVarint32(12345);
    result.insert(result.end(), unknown1Data.begin(), unknown1Data.end());

    // field 1: valid metadata
    auto metadata1Tag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadata1Tag.begin(), metadata1Tag.end());
    auto metadata1Data = encodeMapEntry("key1", "value1");
    auto metadata1Len = encodeVarint32(metadata1Data.size());
    result.insert(result.end(), metadata1Len.begin(), metadata1Len.end());
    result.insert(result.end(), metadata1Data.begin(), metadata1Data.end());

    // field 200: unknown fixed64 field
    auto unknown2Tag = encodeVarint32(encodeTag(200, 1));
    result.insert(result.end(), unknown2Tag.begin(), unknown2Tag.end());
    auto unknown2Data = encodeFixed64(0xDEADBEEFCAFEBABEULL);
    result.insert(result.end(), unknown2Data.begin(), unknown2Data.end());

    // field 2: valid tags
    auto tags1Tag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tags1Tag.begin(), tags1Tag.end());
    auto tags1Data = encodeMapEntry("key2", "value2");
    auto tags1Len = encodeVarint32(tags1Data.size());
    result.insert(result.end(), tags1Len.begin(), tags1Len.end());
    result.insert(result.end(), tags1Data.begin(), tags1Data.end());

    // field 300: unknown length-delimited field
    auto unknown3Tag = encodeVarint32(encodeTag(300, 2));
    result.insert(result.end(), unknown3Tag.begin(), unknown3Tag.end());
    auto unknown3Data = encodeLengthDelimited("unknown message");
    result.insert(result.end(), unknown3Data.begin(), unknown3Data.end());

    // field 1: another valid metadata
    auto metadata2Tag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadata2Tag.begin(), metadata2Tag.end());
    auto metadata2Data = encodeMapEntry("key3", "value3");
    auto metadata2Len = encodeVarint32(metadata2Data.size());
    result.insert(result.end(), metadata2Len.begin(), metadata2Len.end());
    result.insert(result.end(), metadata2Data.begin(), metadata2Data.end());

    // field 400: unknown fixed32 field
    auto unknown4Tag = encodeVarint32(encodeTag(400, 5));
    result.insert(result.end(), unknown4Tag.begin(), unknown4Tag.end());
    auto unknown4Data = encodeFixed32(0xABCDEF01);
    result.insert(result.end(), unknown4Data.begin(), unknown4Data.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip all unknown fields and parse all valid fields");
    APSARA_TEST_EQUAL(3U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL("value1", eventGroup.GetTag("key1").to_string());
    APSARA_TEST_EQUAL("value2", eventGroup.GetTag("key2").to_string());
    APSARA_TEST_EQUAL("value3", eventGroup.GetTag("key3").to_string());
}

// ============================================================================
// Category 8: Mixed Scenario Tests (5 test cases)
// ============================================================================

// Test case 56: MixedEventTypes - PipelineEventGroup with LogEvents, MetricEvents, and SpanEvents
void ManualPBParserUnittest::TestMixedEventTypes() {
    vector<uint8_t> result;

    // Add LogEvent
    auto logEventData = encodeLogEvent(1000000000ULL, {{"message", "log entry"}});
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2)); // field 3 = LogEvents
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    // Add MetricEvent
    auto metricEventData = encodeMetricEvent(2000000000ULL, "cpu_usage", {{"host", "server1"}}, 75.5, true);
    auto metricEventsData = encodeMetricEvents({metricEventData});
    auto metricsTag = encodeVarint32(encodeTag(4, 2)); // field 4 = Metrics
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    // Add SpanEvent
    auto spanEventData = encodeSpanEvent(3000000000ULL, "trace-id", "span-id", "operation", 1, 100ULL, 200ULL);
    auto spanEventsData = encodeSpanEvents({spanEventData});
    auto spansTag = encodeVarint32(encodeTag(5, 2)); // field 5 = Spans
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse mixed event types");
    APSARA_TEST_EQUAL(3U, eventGroup.GetEvents().size());

    // Verify LogEvent
    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("log entry", logEvent.GetContent("message").to_string());

    // Verify MetricEvent
    const auto& metricEvent = eventGroup.GetEvents()[1].Cast<MetricEvent>();
    APSARA_TEST_EQUAL("cpu_usage", metricEvent.GetName().to_string());
    APSARA_TEST_EQUAL(75.5, metricEvent.GetValue<UntypedSingleValue>()->mValue);

    // Verify SpanEvent
    const auto& spanEvent = eventGroup.GetEvents()[2].Cast<SpanEvent>();
    APSARA_TEST_EQUAL("trace-id", spanEvent.GetTraceId().to_string());
}

// Test case 57: MixedWithMetadataAndTags - Mixed events with metadata and tags
void ManualPBParserUnittest::TestMixedWithMetadataAndTags() {
    vector<uint8_t> result;

    // Add metadata
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());
    auto metadataData = encodeMapEntry("env", "production");
    auto metadataLen = encodeVarint32(metadataData.size());
    result.insert(result.end(), metadataLen.begin(), metadataLen.end());
    result.insert(result.end(), metadataData.begin(), metadataData.end());

    // Add tags
    auto tagsTag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tagsTag.begin(), tagsTag.end());
    auto tagsData = encodeMapEntry("region", "us-west");
    auto tagsLen = encodeVarint32(tagsData.size());
    result.insert(result.end(), tagsLen.begin(), tagsLen.end());
    result.insert(result.end(), tagsData.begin(), tagsData.end());

    // Add LogEvent
    auto logEventData = encodeLogEvent(1000ULL, {{"key", "value"}});
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    // Add MetricEvent
    auto metricEventData = encodeMetricEvent(2000ULL, "memory", {}, 1024.0, true);
    auto metricEventsData = encodeMetricEvents({metricEventData});
    auto metricsTag = encodeVarint32(encodeTag(4, 2));
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(2U, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL("production", eventGroup.GetTag("env").to_string());
    APSARA_TEST_EQUAL("us-west", eventGroup.GetTag("region").to_string());
}

// Test case 58: MixedEventsWithUnknownFields - Mixed events with unknown fields interspersed
void ManualPBParserUnittest::TestMixedEventsWithUnknownFields() {
    vector<uint8_t> result;

    // Unknown field 100
    auto unknown1Tag = encodeVarint32(encodeTag(100, 0));
    result.insert(result.end(), unknown1Tag.begin(), unknown1Tag.end());
    auto unknown1Data = encodeVarint32(999);
    result.insert(result.end(), unknown1Data.begin(), unknown1Data.end());

    // LogEvent
    auto logEventData = encodeLogEvent(1000ULL, {{"log", "data"}});
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    // Unknown field 200
    auto unknown2Tag = encodeVarint32(encodeTag(200, 2));
    result.insert(result.end(), unknown2Tag.begin(), unknown2Tag.end());
    auto unknown2Data = encodeLengthDelimited("unknown");
    result.insert(result.end(), unknown2Data.begin(), unknown2Data.end());

    // MetricEvent
    auto metricEventData = encodeMetricEvent(2000ULL, "requests", {}, 100.0, true);
    auto metricEventsData = encodeMetricEvents({metricEventData});
    auto metricsTag = encodeVarint32(encodeTag(4, 2));
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should parse mixed events and skip unknown fields");
    APSARA_TEST_EQUAL(2U, eventGroup.GetEvents().size());
}

// Test case 59: ComplexNestedStructure - Complex nested structure with all features
void ManualPBParserUnittest::TestComplexNestedStructure() {
    vector<uint8_t> result;

    // Metadata with multiple entries
    auto metadata1Tag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadata1Tag.begin(), metadata1Tag.end());
    auto metadata1Data = encodeMapEntry("version", "1.0");
    auto metadata1Len = encodeVarint32(metadata1Data.size());
    result.insert(result.end(), metadata1Len.begin(), metadata1Len.end());
    result.insert(result.end(), metadata1Data.begin(), metadata1Data.end());

    auto metadata2Tag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadata2Tag.begin(), metadata2Tag.end());
    auto metadata2Data = encodeMapEntry("source", "test");
    auto metadata2Len = encodeVarint32(metadata2Data.size());
    result.insert(result.end(), metadata2Len.begin(), metadata2Len.end());
    result.insert(result.end(), metadata2Data.begin(), metadata2Data.end());

    // LogEvent with multiple contents
    auto logEventData = encodeLogEvent(
        1000ULL, {{"level", "INFO"}, {"message", "test"}, {"module", "parser"}}, "INFO", 100ULL, 200ULL);
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    // SpanEvent with inner events
    vector<pair<string, string>> innerEventTags = {{"type", "exception"}};
    auto innerEvent = encodeSpanInnerEvent(500ULL, "error-event", innerEventTags);
    auto spanEventData = encodeSpanEvent(2000ULL,
                                         "trace-123",
                                         "span-456",
                                         "complex-operation",
                                         2,
                                         1000ULL,
                                         3000ULL,
                                         {{"http.method", "POST"}},
                                         {{"service.name", "api"}},
                                         "",
                                         "",
                                         0,
                                         {innerEvent});
    auto spanEventsData = encodeSpanEvents({spanEventData});
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse complex nested structure");
    APSARA_TEST_EQUAL(2U, eventGroup.GetEvents().size());
    APSARA_TEST_EQUAL(2U, eventGroup.GetTags().size());

    const auto& spanEvent = eventGroup.GetEvents()[1].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(1U, spanEvent.GetEvents().size());
}

// Test case 60: LargeScaleMixedData - Large scale mixed data with many events
void ManualPBParserUnittest::TestLargeScaleMixedData() {
    vector<uint8_t> result;

    // Create 10 LogEvents
    vector<vector<uint8_t>> logEvents;
    for (int i = 0; i < 10; i++) {
        auto logData = encodeLogEvent(1000ULL + i, {{"index", to_string(i)}});
        logEvents.push_back(logData);
    }
    auto logEventsData = encodeLogEvents(logEvents);
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    // Create 10 MetricEvents
    vector<vector<uint8_t>> metricEvents;
    for (int i = 0; i < 10; i++) {
        auto metricData = encodeMetricEvent(2000ULL + i, "metric" + to_string(i), {}, i * 10.0, true);
        metricEvents.push_back(metricData);
    }
    auto metricEventsData = encodeMetricEvents(metricEvents);
    auto metricsTag = encodeVarint32(encodeTag(4, 2));
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse large scale mixed data");
    APSARA_TEST_EQUAL(20U, eventGroup.GetEvents().size());

    // Verify first and last events
    const auto& firstLog = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("0", firstLog.GetContent("index").to_string());

    const auto& lastMetric = eventGroup.GetEvents()[19].Cast<MetricEvent>();
    APSARA_TEST_EQUAL("metric9", lastMetric.GetName().to_string());
}

// ============================================================================
// Category 9: Boundary and Extreme Cases (5 test cases)
// ============================================================================

// Test case 61: EmptyStrings - Test empty string handling
void ManualPBParserUnittest::TestEmptyStrings() {
    // LogEvent with empty strings - note: duplicate keys will be overwritten
    // First {"", "first"} then {"", "second"} means final value for "" is "second"
    auto logEventData = encodeLogEvent(1000ULL, {{"emptyKey", ""}, {"", "value"}, {"key", ""}});
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse empty strings");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    // Verify empty string entries are still present
    APSARA_TEST_EQUAL("", logEvent.GetContent("emptyKey").to_string());
    APSARA_TEST_EQUAL("value", logEvent.GetContent("").to_string());
    APSARA_TEST_EQUAL("", logEvent.GetContent("key").to_string());
}

// Test case 62: VeryLargeVarint - Test very large varint values
void ManualPBParserUnittest::TestVeryLargeVarint() {
    // Create LogEvent with maximum timestamp value
    auto logEventData = encodeLogEvent(0xFFFFFFFFFFFFFFFFULL, {{"data", "test"}});
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse very large varint");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(0xFFFFFFFFFFFFFFFFULL, logEvent.GetTimestamp());
}

// Test case 63: MaximumNesting - Test maximum nesting depth
void ManualPBParserUnittest::TestMaximumNesting() {
    // Create SpanEvent with multiple nested inner events
    vector<pair<string, string>> tags1 = {{"event", "1"}};
    auto innerEvent1 = encodeSpanInnerEvent(100ULL, "event-1", tags1);

    vector<pair<string, string>> tags2 = {{"event", "2"}};
    auto innerEvent2 = encodeSpanInnerEvent(200ULL, "event-2", tags2);

    vector<pair<string, string>> tags3 = {{"event", "3"}};
    auto innerEvent3 = encodeSpanInnerEvent(300ULL, "event-3", tags3);

    auto spanEventData = encodeSpanEvent(1000ULL,
                                         "trace",
                                         "span",
                                         "nested-test",
                                         0,
                                         100ULL,
                                         400ULL,
                                         {},
                                         {},
                                         "",
                                         "",
                                         0,
                                         {innerEvent1, innerEvent2, innerEvent3});

    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse maximum nesting");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(3U, spanEvent.GetEvents().size());
}

// Test case 64: ZeroValues - Test zero values for all numeric fields
void ManualPBParserUnittest::TestZeroValues() {
    // LogEvent with zero timestamp
    auto logEventData = encodeLogEvent(0ULL, {{"data", "zero"}}, "", 0ULL, 0ULL);
    auto logEventsData = encodeLogEvents({logEventData});

    // MetricEvent with zero value
    auto metricEventData = encodeMetricEvent(0ULL, "zero-metric", {}, 0.0, true);
    auto metricEventsData = encodeMetricEvents({metricEventData});

    // SpanEvent with zero timestamps
    auto spanEventData = encodeSpanEvent(0ULL, "trace", "span", "zero-span", 0, 0ULL, 0ULL);
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;

    // Add all events
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    auto metricsTag = encodeVarint32(encodeTag(4, 2));
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg), "Should successfully parse zero values");
    APSARA_TEST_EQUAL(3U, eventGroup.GetEvents().size());

    // Verify zero values
    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(0ULL, logEvent.GetTimestamp());

    const auto& metricEvent = eventGroup.GetEvents()[1].Cast<MetricEvent>();
    APSARA_TEST_EQUAL(0.0, metricEvent.GetValue<UntypedSingleValue>()->mValue);

    const auto& spanEvent = eventGroup.GetEvents()[2].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(0ULL, spanEvent.GetTimestamp());
}

// Test case 65: LargeNumberOfFields - Test handling large number of map fields
void ManualPBParserUnittest::TestLargeNumberOfFields() {
    vector<uint8_t> result;

    // Add 50 metadata entries
    for (int i = 0; i < 50; i++) {
        auto metadataTag = encodeVarint32(encodeTag(1, 2));
        result.insert(result.end(), metadataTag.begin(), metadataTag.end());
        auto metadataData = encodeMapEntry("key" + to_string(i), "value" + to_string(i));
        auto metadataLen = encodeVarint32(metadataData.size());
        result.insert(result.end(), metadataLen.begin(), metadataLen.end());
        result.insert(result.end(), metadataData.begin(), metadataData.end());
    }

    // Add LogEvent with 30 contents
    vector<pair<string, string>> contents;
    for (int i = 0; i < 30; i++) {
        contents.push_back({"field" + to_string(i), "data" + to_string(i)});
    }
    auto logEventData = encodeLogEvent(1000ULL, contents);
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse large number of fields");
    APSARA_TEST_EQUAL(50U, eventGroup.GetTags().size());
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    // Verify some random entries
    APSARA_TEST_EQUAL("value0", eventGroup.GetTag("key0").to_string());
    APSARA_TEST_EQUAL("value25", eventGroup.GetTag("key25").to_string());
    APSARA_TEST_EQUAL("value49", eventGroup.GetTag("key49").to_string());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("data0", logEvent.GetContent("field0").to_string());
    APSARA_TEST_EQUAL("data29", logEvent.GetContent("field29").to_string());
}

// ============================================================================
// Category 10: SpanLink Complete Tests (10 test cases)
// ============================================================================

// Test case 66: ParseSpanLink_Complete - Complete SpanLink with all fields
void ManualPBParserUnittest::TestParseSpanLinkComplete() {
    vector<pair<string, string>> linkTags = {{"link.type", "parent"}, {"sampled", "true"}};
    auto spanLinkData = encodeSpanLink("link-trace-123", "link-span-456", "link-state=abc", linkTags);

    auto spanEventData = encodeSpanEventWithLinks(
        1000000ULL, "trace-id", "span-id", "span-with-link", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse SpanEvent with complete SpanLink");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(1U, spanEvent.GetLinks().size());

    const auto& link = spanEvent.GetLinks()[0];
    APSARA_TEST_EQUAL("link-trace-123", link.GetTraceId().to_string());
    APSARA_TEST_EQUAL("link-span-456", link.GetSpanId().to_string());
    APSARA_TEST_EQUAL("link-state=abc", link.GetTraceState().to_string());
    APSARA_TEST_EQUAL("parent", link.GetTag("link.type").to_string());
    APSARA_TEST_EQUAL("true", link.GetTag("sampled").to_string());
}

// Test case 67: ParseSpanLink_MinimalFields - SpanLink with minimal fields
void ManualPBParserUnittest::TestParseSpanLinkMinimalFields() {
    auto spanLinkData = encodeSpanLink("minimal-trace", "minimal-span");

    auto spanEventData = encodeSpanEventWithLinks(
        2000000ULL, "trace-id", "span-id", "minimal-link-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(1U, spanEvent.GetLinks().size());

    const auto& link = spanEvent.GetLinks()[0];
    APSARA_TEST_EQUAL("minimal-trace", link.GetTraceId().to_string());
    APSARA_TEST_EQUAL("minimal-span", link.GetSpanId().to_string());
}

// Test case 68: ParseSpanLink_WithTags - SpanLink with multiple tags
void ManualPBParserUnittest::TestParseSpanLinkWithTags() {
    vector<pair<string, string>> linkTags
        = {{"relationship", "follows"}, {"reason", "retry"}, {"priority", "high"}, {"source", "external"}};
    auto spanLinkData = encodeSpanLink("tagged-trace", "tagged-span", "", linkTags);

    auto spanEventData = encodeSpanEventWithLinks(
        3000000ULL, "trace-id", "span-id", "tagged-link-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    const auto& link = spanEvent.GetLinks()[0];

    APSARA_TEST_EQUAL("follows", link.GetTag("relationship").to_string());
    APSARA_TEST_EQUAL("retry", link.GetTag("reason").to_string());
    APSARA_TEST_EQUAL("high", link.GetTag("priority").to_string());
    APSARA_TEST_EQUAL("external", link.GetTag("source").to_string());
}

// Test case 69: ParseSpanLink_MultipleLinks - SpanEvent with multiple links
void ManualPBParserUnittest::TestParseSpanLinkMultipleLinks() {
    auto spanLink1 = encodeSpanLink("trace-1", "span-1", "state-1");
    auto spanLink2 = encodeSpanLink("trace-2", "span-2", "state-2");
    auto spanLink3 = encodeSpanLink("trace-3", "span-3", "state-3");

    auto spanEventData = encodeSpanEventWithLinks(
        4000000ULL, "main-trace", "main-span", "multi-link-span", 0, 100ULL, 200ULL, {spanLink1, spanLink2, spanLink3});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg), "Should parse multiple SpanLinks");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(3U, spanEvent.GetLinks().size());

    APSARA_TEST_EQUAL("trace-1", spanEvent.GetLinks()[0].GetTraceId().to_string());
    APSARA_TEST_EQUAL("span-1", spanEvent.GetLinks()[0].GetSpanId().to_string());
    APSARA_TEST_EQUAL("state-1", spanEvent.GetLinks()[0].GetTraceState().to_string());

    APSARA_TEST_EQUAL("trace-2", spanEvent.GetLinks()[1].GetTraceId().to_string());
    APSARA_TEST_EQUAL("span-2", spanEvent.GetLinks()[1].GetSpanId().to_string());
    APSARA_TEST_EQUAL("state-2", spanEvent.GetLinks()[1].GetTraceState().to_string());

    APSARA_TEST_EQUAL("trace-3", spanEvent.GetLinks()[2].GetTraceId().to_string());
    APSARA_TEST_EQUAL("span-3", spanEvent.GetLinks()[2].GetSpanId().to_string());
    APSARA_TEST_EQUAL("state-3", spanEvent.GetLinks()[2].GetTraceState().to_string());
}

// Test case 70: ParseSpanLink_InvalidTraceIdWireType - Invalid wire type for trace_id
void ManualPBParserUnittest::TestParseSpanLinkInvalidTraceIdWireType() {
    auto spanLinkData = encodeSpanLinkInvalidTraceIdWireType();

    auto spanEventData = encodeSpanEventWithLinks(
        5000000ULL, "trace-id", "span-id", "invalid-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid trace_id wire type");
}

// Test case 71: ParseSpanLink_InvalidSpanIdWireType - Invalid wire type for span_id
void ManualPBParserUnittest::TestParseSpanLinkInvalidSpanIdWireType() {
    auto spanLinkData = encodeSpanLinkInvalidSpanIdWireType();

    auto spanEventData = encodeSpanEventWithLinks(
        6000000ULL, "trace-id", "span-id", "invalid-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid span_id wire type");
}

// Test case 72: ParseSpanLink_InvalidTagsWireType - Invalid wire type for tags
void ManualPBParserUnittest::TestParseSpanLinkInvalidTagsWireType() {
    auto spanLinkData = encodeSpanLinkInvalidTagsWireType();

    auto spanEventData = encodeSpanEventWithLinks(
        7000000ULL, "trace-id", "span-id", "invalid-tags-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid tags wire type");
}

// Test case 73: ParseSpanLinkTags_KeyOnlyNoValue - SpanLink tag with only key
void ManualPBParserUnittest::TestParseSpanLinkTagsKeyOnlyNoValue() {
    // Create SpanLink with tag that has only key, no value
    vector<uint8_t> spanLinkData;

    // field 1: trace_id
    auto traceIdTag = encodeVarint32(encodeTag(1, 2));
    spanLinkData.insert(spanLinkData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceIdData = encodeLengthDelimited("trace-id");
    spanLinkData.insert(spanLinkData.end(), traceIdData.begin(), traceIdData.end());

    // field 2: span_id
    auto spanIdTag = encodeVarint32(encodeTag(2, 2));
    spanLinkData.insert(spanLinkData.end(), spanIdTag.begin(), spanIdTag.end());
    auto spanIdData = encodeLengthDelimited("span-id");
    spanLinkData.insert(spanLinkData.end(), spanIdData.begin(), spanIdData.end());

    // field 4: tags with only key
    auto tagsTag = encodeVarint32(encodeTag(4, 2));
    spanLinkData.insert(spanLinkData.end(), tagsTag.begin(), tagsTag.end());
    auto tagData = encodeMapEntryKeyOnly("key-only");
    auto tagLen = encodeVarint32(tagData.size());
    spanLinkData.insert(spanLinkData.end(), tagLen.begin(), tagLen.end());
    spanLinkData.insert(spanLinkData.end(), tagData.begin(), tagData.end());

    auto spanEventData = encodeSpanEventWithLinks(
        8000000ULL, "trace-id", "span-id", "key-only-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    const auto& link = spanEvent.GetLinks()[0];

    // Tag without both key and value should not be added
    APSARA_TEST_TRUE(link.TagsSize() == 0 || link.GetTag("key-only").to_string().empty());
}

// Test case 74: ParseSpanLinkTags_InvalidKeyWireType - Invalid wire type for tag key
void ManualPBParserUnittest::TestParseSpanLinkTagsInvalidKeyWireType() {
    // Create SpanLink with tag that has invalid key wire type
    vector<uint8_t> spanLinkData;

    // field 1: trace_id
    auto traceIdTag = encodeVarint32(encodeTag(1, 2));
    spanLinkData.insert(spanLinkData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceIdData = encodeLengthDelimited("trace-id");
    spanLinkData.insert(spanLinkData.end(), traceIdData.begin(), traceIdData.end());

    // field 2: span_id
    auto spanIdTag = encodeVarint32(encodeTag(2, 2));
    spanLinkData.insert(spanLinkData.end(), spanIdTag.begin(), spanIdTag.end());
    auto spanIdData = encodeLengthDelimited("span-id");
    spanLinkData.insert(spanLinkData.end(), spanIdData.begin(), spanIdData.end());

    // field 4: tags with invalid key wire type
    auto tagsTag = encodeVarint32(encodeTag(4, 2));
    spanLinkData.insert(spanLinkData.end(), tagsTag.begin(), tagsTag.end());
    auto tagData = encodeMapEntryInvalidKeyWireType();
    auto tagLen = encodeVarint32(tagData.size());
    spanLinkData.insert(spanLinkData.end(), tagLen.begin(), tagLen.end());
    spanLinkData.insert(spanLinkData.end(), tagData.begin(), tagData.end());

    auto spanEventData = encodeSpanEventWithLinks(
        9000000ULL, "trace-id", "span-id", "invalid-key-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail with invalid tag key wire type");
}

// Test case 75: ParseSpanLink_WithUnknownFields - SpanLink with unknown fields
void ManualPBParserUnittest::TestParseSpanLinkWithUnknownFields() {
    auto spanLinkData = encodeSpanLinkWithUnknownField("unknown-trace", "unknown-span");

    auto spanEventData = encodeSpanEventWithLinks(
        10000000ULL, "trace-id", "span-id", "unknown-field-span", 0, 100ULL, 200ULL, {spanLinkData});
    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should skip unknown field and parse SpanLink successfully");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(1U, spanEvent.GetLinks().size());

    const auto& link = spanEvent.GetLinks()[0];
    APSARA_TEST_EQUAL("unknown-trace", link.GetTraceId().to_string());
    APSARA_TEST_EQUAL("unknown-span", link.GetSpanId().to_string());
}

// ============================================================================
// Category 11: Data Corruption and Error Recovery Tests (10 test cases)
// ============================================================================

// Test case 76: ReadVarint32Truncated - Truncated varint32 data
void ManualPBParserUnittest::TestReadVarint32Truncated() {
    // Create a varint32 that starts but is truncated (missing continuation bytes)
    vector<uint8_t> data;

    // Start of a multi-byte varint but truncated
    data.push_back(0x80); // Continuation bit set but no following byte

    ManualPBParser parser(data.data(), data.size(), false);
    uint32_t value = 0;

    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, value),
                           "Should fail to read truncated varint32");
}

// Test case 77: ReadVarint64Truncated - Truncated varint64 data
void ManualPBParserUnittest::TestReadVarint64Truncated() {
    // Create a varint64 that starts but is truncated
    vector<uint8_t> data;

    // Start of a multi-byte varint64 but truncated
    data.push_back(0x80); // Continuation bit set
    data.push_back(0x80); // Continuation bit set
    data.push_back(0x80); // Continuation bit set but missing more bytes

    ManualPBParser parser(data.data(), data.size(), false);
    uint64_t value = 0;

    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadVarint64(parser, value),
                           "Should fail to read truncated varint64");
}

// Test case 78: ReadFixed64InsufficientData - Fixed64 with insufficient data
void ManualPBParserUnittest::TestReadFixed64InsufficientData() {
    // Create data with less than 8 bytes for fixed64
    vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05}; // Only 5 bytes

    ManualPBParser parser(data.data(), data.size(), false);
    uint64_t value = 0;

    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadFixed64(parser, value),
                           "Should fail to read fixed64 with insufficient data");
}

// Test case 79: ReadStringLengthExceedsBounds - String length exceeds available data
void ManualPBParserUnittest::TestReadStringLengthExceedsBounds() {
    vector<uint8_t> data;

    // Encode a length that's larger than actual data
    auto lengthData = encodeVarint32(100); // Claim 100 bytes
    data.insert(data.end(), lengthData.begin(), lengthData.end());
    // But only provide 5 bytes
    data.insert(data.end(), {'h', 'e', 'l', 'l', 'o'});

    ManualPBParser parser(data.data(), data.size(), false);
    string str;

    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadString(parser, str),
                           "Should fail when string length exceeds bounds");
}

// Test case 80: ParseLogEventReadNameFailed - LogEvent content key read failure
void ManualPBParserUnittest::TestParseLogEventReadNameFailed() {
    vector<uint8_t> logEventData;

    // field 1: timestamp (valid)
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    logEventData.insert(logEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(1000ULL);
    logEventData.insert(logEventData.end(), timestampData.begin(), timestampData.end());

    // field 2: content (malformed - key length exceeds bounds)
    auto contentTag = encodeVarint32(encodeTag(2, 2));
    logEventData.insert(logEventData.end(), contentTag.begin(), contentTag.end());

    // Content message with corrupted key
    vector<uint8_t> contentData;
    // field 1: key with length that exceeds bounds
    auto keyTag = encodeVarint32(encodeTag(1, 2));
    contentData.insert(contentData.end(), keyTag.begin(), keyTag.end());
    auto keyLenData = encodeVarint32(1000); // Claim 1000 bytes
    contentData.insert(contentData.end(), keyLenData.begin(), keyLenData.end());
    contentData.insert(contentData.end(), {'k', 'e', 'y'}); // Only 3 bytes

    auto contentLen = encodeVarint32(contentData.size());
    logEventData.insert(logEventData.end(), contentLen.begin(), contentLen.end());
    logEventData.insert(logEventData.end(), contentData.begin(), contentData.end());

    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail when content key read fails");
}

// Test case 81: ParseMetricEventReadNameFailed - MetricEvent name read failure
void ManualPBParserUnittest::TestParseMetricEventReadNameFailed() {
    vector<uint8_t> metricEventData;

    // field 1: timestamp (valid)
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    metricEventData.insert(metricEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(2000ULL);
    metricEventData.insert(metricEventData.end(), timestampData.begin(), timestampData.end());

    // field 2: name (corrupted - length exceeds bounds)
    auto nameTag = encodeVarint32(encodeTag(2, 2));
    metricEventData.insert(metricEventData.end(), nameTag.begin(), nameTag.end());
    auto nameLenData = encodeVarint32(500); // Claim 500 bytes
    metricEventData.insert(metricEventData.end(), nameLenData.begin(), nameLenData.end());
    metricEventData.insert(metricEventData.end(), {'n', 'a', 'm', 'e'}); // Only 4 bytes

    auto metricEventsData = encodeMetricEvents({metricEventData});

    vector<uint8_t> result;
    auto metricsTag = encodeVarint32(encodeTag(4, 2));
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail when metric name read fails");
}

// Test case 82: ParseSpanEventReadTraceIdFailed - SpanEvent trace_id read failure
void ManualPBParserUnittest::TestParseSpanEventReadTraceIdFailed() {
    vector<uint8_t> spanEventData;

    // field 1: timestamp (valid)
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    spanEventData.insert(spanEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(3000ULL);
    spanEventData.insert(spanEventData.end(), timestampData.begin(), timestampData.end());

    // field 2: trace_id (corrupted - length exceeds bounds)
    auto traceIdTag = encodeVarint32(encodeTag(2, 2));
    spanEventData.insert(spanEventData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceIdLenData = encodeVarint32(200); // Claim 200 bytes
    spanEventData.insert(spanEventData.end(), traceIdLenData.begin(), traceIdLenData.end());
    spanEventData.insert(spanEventData.end(), {'t', 'r', 'a', 'c', 'e'}); // Only 5 bytes

    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail when span trace_id read fails");
}

// Test case 83: ParseMapEntryReadKeyFailed - Map entry key read failure
void ManualPBParserUnittest::TestParseMapEntryReadKeyFailed() {
    vector<uint8_t> result;

    // field 1: Metadata with corrupted key
    auto metadataTag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadataTag.begin(), metadataTag.end());

    // Map entry with corrupted key (length exceeds bounds)
    vector<uint8_t> entryData;
    auto keyTag = encodeVarint32(encodeTag(1, 2));
    entryData.insert(entryData.end(), keyTag.begin(), keyTag.end());
    auto keyLenData = encodeVarint32(300); // Claim 300 bytes
    entryData.insert(entryData.end(), keyLenData.begin(), keyLenData.end());
    entryData.insert(entryData.end(), {'k', 'e', 'y'}); // Only 3 bytes

    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail when map entry key read fails");
}

// Test case 84: ParseMapEntryReadValueFailed - Map entry value read failure
void ManualPBParserUnittest::TestParseMapEntryReadValueFailed() {
    vector<uint8_t> result;

    // field 2: Tags with corrupted value
    auto tagsTag = encodeVarint32(encodeTag(2, 2));
    result.insert(result.end(), tagsTag.begin(), tagsTag.end());

    // Map entry with corrupted value (length exceeds bounds)
    vector<uint8_t> entryData;
    // field 1: key (valid)
    auto keyTag = encodeVarint32(encodeTag(1, 2));
    entryData.insert(entryData.end(), keyTag.begin(), keyTag.end());
    auto keyData = encodeLengthDelimited("valid-key");
    entryData.insert(entryData.end(), keyData.begin(), keyData.end());

    // field 2: value (corrupted)
    auto valueTag = encodeVarint32(encodeTag(2, 2));
    entryData.insert(entryData.end(), valueTag.begin(), valueTag.end());
    auto valueLenData = encodeVarint32(400); // Claim 400 bytes
    entryData.insert(entryData.end(), valueLenData.begin(), valueLenData.end());
    entryData.insert(entryData.end(), {'v', 'a', 'l'}); // Only 3 bytes

    auto entryLen = encodeVarint32(entryData.size());
    result.insert(result.end(), entryLen.begin(), entryLen.end());
    result.insert(result.end(), entryData.begin(), entryData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail when map entry value read fails");
}

// Test case 85: NestedMessageParseFailed - Nested message parse failure and state recovery
void ManualPBParserUnittest::TestNestedMessageParseFailed() {
    vector<uint8_t> result;

    // Add valid metadata first
    auto metadata1Tag = encodeVarint32(encodeTag(1, 2));
    result.insert(result.end(), metadata1Tag.begin(), metadata1Tag.end());
    auto metadata1Data = encodeMapEntry("key1", "value1");
    auto metadata1Len = encodeVarint32(metadata1Data.size());
    result.insert(result.end(), metadata1Len.begin(), metadata1Len.end());
    result.insert(result.end(), metadata1Data.begin(), metadata1Data.end());

    // Add corrupted LogEvent (will cause parse failure)
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());

    // LogEvents message with corrupted LogEvent
    vector<uint8_t> logEventsData;
    auto logEventTag = encodeVarint32(encodeTag(1, 2));
    logEventsData.insert(logEventsData.end(), logEventTag.begin(), logEventTag.end());

    // Corrupted LogEvent
    vector<uint8_t> logEventData;
    auto timestampTag = encodeVarint32(encodeTag(1, 0));
    logEventData.insert(logEventData.end(), timestampTag.begin(), timestampTag.end());
    // Corrupted timestamp (truncated varint)
    logEventData.push_back(0x80); // Continuation bit set but no following byte

    auto logEventLen = encodeVarint32(logEventData.size());
    logEventsData.insert(logEventsData.end(), logEventLen.begin(), logEventLen.end());
    logEventsData.insert(logEventsData.end(), logEventData.begin(), logEventData.end());

    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                           "Should fail when nested message parse fails");

    // Verify that valid metadata before the error was still processed
    // (This depends on the implementation - if it rolls back on error, tags might be empty)
    // For now, we just verify that parsing failed as expected
}

// ============================================================================
// Category 12: Boundary and Special Value Tests (8 test cases)
// ============================================================================

// Test case 86: Varint64MaxValue - Parse maximum varint64 value
void ManualPBParserUnittest::TestVarint64MaxValueParsing() {
    // Create LogEvent with maximum timestamp value (0xFFFFFFFFFFFFFFFF)
    auto logEventData = encodeLogEvent(0xFFFFFFFFFFFFFFFFULL, {{"data", "max-timestamp"}});
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse maximum varint64 value");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(0xFFFFFFFFFFFFFFFFULL, logEvent.GetTimestamp());
    APSARA_TEST_EQUAL("max-timestamp", logEvent.GetContent("data").to_string());
}

// Test case 87: Varint32_10ByteEncoding - Test 10-byte varint32 (invalid/overflow)
void ManualPBParserUnittest::TestVarint3210ByteEncoding() {
    // Create a 10-byte varint (all continuation bits set for first 9 bytes)
    // This should be invalid for varint32 as it can overflow
    vector<uint8_t> data;

    // Create a varint that's too long (10 bytes with continuation bits)
    for (int i = 0; i < 9; i++) {
        data.push_back(0x80); // All have continuation bit
    }
    data.push_back(0x01); // Final byte

    ManualPBParser parser(data.data(), data.size(), false);
    uint32_t value = 0;

    // This should fail as varint32 should only be up to 5 bytes maximum
    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, value),
                           "Should fail on overly long varint32 encoding");
}

// Test case 88: LengthDelimitedZeroLength - String with zero length
void ManualPBParserUnittest::TestLengthDelimitedZeroLength() {
    // Create LogEvent with empty string content
    auto logEventData = encodeLogEvent(1000ULL, {{"empty-key", ""}, {"", "empty-value"}});
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse zero-length strings");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("", logEvent.GetContent("empty-key").to_string());
    APSARA_TEST_EQUAL("empty-value", logEvent.GetContent("").to_string());
}

// Test case 89: LengthDelimitedVeryLarge - Very large string (1MB)
void ManualPBParserUnittest::TestLengthDelimitedVeryLarge() {
    // Create a 1MB string
    string largeString(1024 * 1024, 'A'); // 1MB of 'A's

    auto logEventData = encodeLogEvent(2000ULL, {{"large", largeString}});
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse very large string (1MB)");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(1024U * 1024U, logEvent.GetContent("large").to_string().size());
}

// Test case 90: SpanEventAllFieldsMaxValues - SpanEvent with maximum values
void ManualPBParserUnittest::TestSpanEventAllFieldsMaxValues() {
    // Create SpanEvent with maximum numeric values
    auto spanEventData = encodeSpanEvent(0xFFFFFFFFFFFFFFFFULL, // max timestamp
                                         "max-trace",
                                         "max-span",
                                         "max-values-span",
                                         5, // Consumer (max kind)
                                         0xFFFFFFFFFFFFFFFFULL, // max start_time
                                         0xFFFFFFFFFFFFFFFFULL, // max end_time
                                         {{"key", "value"}},
                                         {},
                                         "",
                                         "",
                                         2); // Error (max status)

    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse SpanEvent with max values");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(0xFFFFFFFFFFFFFFFFULL, spanEvent.GetTimestamp());
    APSARA_TEST_EQUAL(0xFFFFFFFFFFFFFFFFULL, spanEvent.GetStartTimeNs());
    APSARA_TEST_EQUAL(0xFFFFFFFFFFFFFFFFULL, spanEvent.GetEndTimeNs());
    APSARA_TEST_EQUAL(SpanEvent::Kind::Consumer, spanEvent.GetKind());
    APSARA_TEST_EQUAL(SpanEvent::StatusCode::Error, spanEvent.GetStatus());
}

// Test case 91: MetricEventNegativeDoubleValue - MetricEvent with negative double
void ManualPBParserUnittest::TestMetricEventNegativeDoubleValue() {
    // Create MetricEvent with negative double value
    vector<pair<string, string>> tags = {{"type", "temperature"}};
    auto metricEventData = encodeMetricEvent(3000ULL, "temperature", tags, -273.15, true); // Absolute zero

    auto metricEventsData = encodeMetricEvents({metricEventData});

    vector<uint8_t> result;
    auto metricsTag = encodeVarint32(encodeTag(4, 2));
    result.insert(result.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(metricEventsData.size());
    result.insert(result.end(), metricsLen.begin(), metricsLen.end());
    result.insert(result.end(), metricEventsData.begin(), metricEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse negative double value");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL("temperature", metricEvent.GetName().to_string());

    // Check negative value (with small epsilon for floating point comparison)
    double value = metricEvent.GetValue<UntypedSingleValue>()->mValue;
    APSARA_TEST_TRUE(value < -273.14 && value > -273.16);
}

// Test case 92: LogEventTimestampZeroValue - LogEvent with timestamp zero
void ManualPBParserUnittest::TestLogEventTimestampZeroValue() {
    // Create LogEvent with zero timestamp
    auto logEventData = encodeLogEvent(0ULL, {{"event", "zero-time"}}, "", 0ULL, 0ULL);
    auto logEventsData = encodeLogEvents({logEventData});

    vector<uint8_t> result;
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse zero timestamp");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL(0ULL, logEvent.GetTimestamp());

    // Check position (file offset and raw size)
    auto position = logEvent.GetPosition();
    APSARA_TEST_EQUAL(0ULL, position.first); // file offset
    APSARA_TEST_EQUAL(0ULL, position.second); // raw size

    APSARA_TEST_EQUAL("zero-time", logEvent.GetContent("event").to_string());
}

// Test case 93: DeepNestedSpanEventStructure - Deeply nested SpanEvent with multiple levels
void ManualPBParserUnittest::TestDeepNestedSpanEventStructure() {
    // Create SpanEvent with deep nesting: multiple inner events, each with multiple tags
    vector<pair<string, string>> innerTags1 = {{"level", "1"}, {"detail", "first"}};
    vector<pair<string, string>> innerTags2 = {{"level", "2"}, {"detail", "second"}};
    vector<pair<string, string>> innerTags3 = {{"level", "3"}, {"detail", "third"}};
    vector<pair<string, string>> innerTags4 = {{"level", "4"}, {"detail", "fourth"}};
    vector<pair<string, string>> innerTags5 = {{"level", "5"}, {"detail", "fifth"}};

    auto innerEvent1 = encodeSpanInnerEvent(100ULL, "inner-1", innerTags1);
    auto innerEvent2 = encodeSpanInnerEvent(200ULL, "inner-2", innerTags2);
    auto innerEvent3 = encodeSpanInnerEvent(300ULL, "inner-3", innerTags3);
    auto innerEvent4 = encodeSpanInnerEvent(400ULL, "inner-4", innerTags4);
    auto innerEvent5 = encodeSpanInnerEvent(500ULL, "inner-5", innerTags5);

    vector<pair<string, string>> spanTags = {{"span-level", "1"}, {"span-detail", "root"}};
    vector<pair<string, string>> scopeTags = {{"scope-level", "1"}, {"scope-detail", "global"}};

    auto spanEventData = encodeSpanEvent(4000ULL,
                                         "deep-trace",
                                         "deep-span",
                                         "deep-nested-span",
                                         0,
                                         100ULL,
                                         600ULL,
                                         spanTags,
                                         scopeTags,
                                         "state=deep",
                                         "",
                                         0,
                                         {innerEvent1, innerEvent2, innerEvent3, innerEvent4, innerEvent5});

    auto spanEventsData = encodeSpanEvents({spanEventData});

    vector<uint8_t> result;
    auto spansTag = encodeVarint32(encodeTag(5, 2));
    result.insert(result.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(spanEventsData.size());
    result.insert(result.end(), spansLen.begin(), spansLen.end());
    result.insert(result.end(), spanEventsData.begin(), spanEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse deeply nested SpanEvent structure");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
    APSARA_TEST_EQUAL(5U, spanEvent.GetEvents().size());
    APSARA_TEST_EQUAL(2U, spanEvent.TagsSize());
    APSARA_TEST_EQUAL(2U, spanEvent.ScopeTagsSize());

    // Verify all inner events
    for (size_t i = 0; i < 5; i++) {
        const auto& innerEvent = spanEvent.GetEvents()[i];
        APSARA_TEST_EQUAL(to_string(i + 1), innerEvent.GetTag("level").to_string());
    }
}

// ============================================================================
// Category 13: skipField Detailed Tests (5 test cases)
// ============================================================================

// Test case 94: SkipFieldInvalidWireType - Test skipping invalid wire types
void ManualPBParserUnittest::TestSkipFieldInvalidWireType() {
    // Wire types: 0=varint, 1=fixed64, 2=length-delimited, 3=start group, 4=end group, 5=fixed32
    // Wire types 3 and 4 (group) are deprecated, 6 and 7 are invalid

    // Create data with invalid wire type (6)
    vector<uint8_t> data;
    uint32_t invalidTag = encodeTag(99, 6); // field 99 with invalid wire type 6
    auto tagData = encodeVarint32(invalidTag);
    data.insert(data.end(), tagData.begin(), tagData.end());

    ManualPBParser parser(data.data(), data.size(), false);

    // Try to skip the invalid wire type
    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestSkipField(parser, 6),
                           "Should fail to skip invalid wire type 6");

    // Create data with invalid wire type (7)
    vector<uint8_t> data2;
    uint32_t invalidTag2 = encodeTag(100, 7); // field 100 with invalid wire type 7
    auto tagData2 = encodeVarint32(invalidTag2);
    data2.insert(data2.end(), tagData2.begin(), tagData2.end());

    ManualPBParser parser2(data2.data(), data2.size(), false);

    // Try to skip the invalid wire type
    APSARA_TEST_FALSE_DESC(ManualPBParserTestHelper::TestSkipField(parser2, 7),
                           "Should fail to skip invalid wire type 7");
}

// Test case 95: SkipFieldNestedLengthDelimited - Test skipping nested length-delimited fields
void ManualPBParserUnittest::TestSkipFieldNestedLengthDelimited() {
    // Create a PipelineEventGroup with an unknown length-delimited field containing nested data
    vector<uint8_t> result;

    // Add valid LogEvent first
    auto logEventData = encodeLogEvent(1000ULL, {{"key", "value"}});
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    // Add an unknown nested length-delimited field (field 99 with nested structure)
    vector<uint8_t> nestedData;
    // Inner nested data: multiple fields
    auto innerTag1 = encodeVarint32(encodeTag(1, 0));
    nestedData.insert(nestedData.end(), innerTag1.begin(), innerTag1.end());
    auto innerValue1 = encodeVarint32(12345);
    nestedData.insert(nestedData.end(), innerValue1.begin(), innerValue1.end());

    auto innerTag2 = encodeVarint32(encodeTag(2, 2));
    nestedData.insert(nestedData.end(), innerTag2.begin(), innerTag2.end());
    string innerStr = "nested-string";
    auto innerStrData = encodeLengthDelimited(innerStr);
    nestedData.insert(nestedData.end(), innerStrData.begin(), innerStrData.end());

    // Wrap in unknown field 99
    auto unknownTag = encodeVarint32(encodeTag(99, 2));
    result.insert(result.end(), unknownTag.begin(), unknownTag.end());
    auto nestedLen = encodeVarint32(nestedData.size());
    result.insert(result.end(), nestedLen.begin(), nestedLen.end());
    result.insert(result.end(), nestedData.begin(), nestedData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    // Should successfully parse and skip the nested unknown field
    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse with nested unknown field");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test case 96: SkipFieldMultipleConsecutive - Test skipping multiple consecutive unknown fields
void ManualPBParserUnittest::TestSkipFieldMultipleConsecutive() {
    // Create a PipelineEventGroup with multiple consecutive unknown fields of different types
    vector<uint8_t> result;

    // Unknown field 1: varint (wire type 0)
    auto unknownTag1 = encodeVarint32(encodeTag(90, 0));
    result.insert(result.end(), unknownTag1.begin(), unknownTag1.end());
    auto unknownValue1 = encodeVarint32(999);
    result.insert(result.end(), unknownValue1.begin(), unknownValue1.end());

    // Unknown field 2: fixed64 (wire type 1)
    auto unknownTag2 = encodeVarint32(encodeTag(91, 1));
    result.insert(result.end(), unknownTag2.begin(), unknownTag2.end());
    auto unknownValue2 = encodeFixed64(0x1122334455667788ULL);
    result.insert(result.end(), unknownValue2.begin(), unknownValue2.end());

    // Unknown field 3: length-delimited (wire type 2)
    auto unknownTag3 = encodeVarint32(encodeTag(92, 2));
    result.insert(result.end(), unknownTag3.begin(), unknownTag3.end());
    string unknownStr = "unknown-data";
    auto unknownStrData = encodeLengthDelimited(unknownStr);
    result.insert(result.end(), unknownStrData.begin(), unknownStrData.end());

    // Unknown field 4: fixed32 (wire type 5)
    auto unknownTag4 = encodeVarint32(encodeTag(93, 5));
    result.insert(result.end(), unknownTag4.begin(), unknownTag4.end());
    auto unknownValue4 = encodeFixed32(0x11223344);
    result.insert(result.end(), unknownValue4.begin(), unknownValue4.end());

    // Add valid LogEvent
    auto logEventData = encodeLogEvent(2000ULL, {{"after", "unknowns"}});
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    // Should successfully skip all unknown fields and parse the LogEvent
    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip multiple consecutive unknown fields");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("unknowns", logEvent.GetContent("after").to_string());
}

// Test case 97: SkipFieldAfterPartialParse - Test skipping fields after partial parsing
void ManualPBParserUnittest::TestSkipFieldAfterPartialParse() {
    // Create PipelineEventGroup with a known field, then unknown fields, then more known fields
    vector<uint8_t> result;

    // Add first LogEvent (known field)
    auto logEventData1 = encodeLogEvent(2000ULL, {{"first", "event"}});
    auto logEventsData1 = encodeLogEvents({logEventData1});
    auto logEventsTag1 = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag1.begin(), logEventsTag1.end());
    auto logEventsLen1 = encodeVarint32(logEventsData1.size());
    result.insert(result.end(), logEventsLen1.begin(), logEventsLen1.end());
    result.insert(result.end(), logEventsData1.begin(), logEventsData1.end());

    // Add unknown fields in the middle
    auto unknownTag1 = encodeVarint32(encodeTag(88, 0));
    result.insert(result.end(), unknownTag1.begin(), unknownTag1.end());
    auto unknownValue1 = encodeVarint64(0x123456789ABCDEFULL);
    result.insert(result.end(), unknownValue1.begin(), unknownValue1.end());

    auto unknownTag2 = encodeVarint32(encodeTag(89, 2));
    result.insert(result.end(), unknownTag2.begin(), unknownTag2.end());
    string unknownData = "skip-this-data";
    auto unknownDataEncoded = encodeLengthDelimited(unknownData);
    result.insert(result.end(), unknownDataEncoded.begin(), unknownDataEncoded.end());

    // Add second LogEvent (known field 3)
    auto logEventData2 = encodeLogEvent(3000ULL, {{"second", "event"}});
    auto logEventsData2 = encodeLogEvents({logEventData2});
    auto logEventsTag2 = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag2.begin(), logEventsTag2.end());
    auto logEventsLen2 = encodeVarint32(logEventsData2.size());
    result.insert(result.end(), logEventsLen2.begin(), logEventsLen2.end());
    result.insert(result.end(), logEventsData2.begin(), logEventsData2.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    // Should successfully parse first LogEvent, skip unknowns, and parse second LogEvent
    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully parse with unknown fields in the middle");
    APSARA_TEST_EQUAL(2U, eventGroup.GetEvents().size());

    const auto& logEvent1 = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("event", logEvent1.GetContent("first").to_string());

    const auto& logEvent2 = eventGroup.GetEvents()[1].Cast<LogEvent>();
    APSARA_TEST_EQUAL("event", logEvent2.GetContent("second").to_string());
}

// Test case 98: SkipFieldLargeVarint - Test skipping large varint values
void ManualPBParserUnittest::TestSkipFieldLargeVarint() {
    // Create data with very large varint values in unknown fields
    vector<uint8_t> result;

    // Unknown field with maximum varint64 value
    auto unknownTag1 = encodeVarint32(encodeTag(95, 0));
    result.insert(result.end(), unknownTag1.begin(), unknownTag1.end());
    auto largeVarint1 = encodeVarint64(0xFFFFFFFFFFFFFFFFULL);
    result.insert(result.end(), largeVarint1.begin(), largeVarint1.end());

    // Unknown field with another large varint value
    auto unknownTag2 = encodeVarint32(encodeTag(96, 0));
    result.insert(result.end(), unknownTag2.begin(), unknownTag2.end());
    auto largeVarint2 = encodeVarint64(0x7FFFFFFFFFFFFFFFULL);
    result.insert(result.end(), largeVarint2.begin(), largeVarint2.end());

    // Add a valid LogEvent to verify parsing continues correctly
    auto logEventData = encodeLogEvent(4000ULL, {{"data", "after-large-varints"}});
    auto logEventsData = encodeLogEvents({logEventData});
    auto logEventsTag = encodeVarint32(encodeTag(3, 2));
    result.insert(result.end(), logEventsTag.begin(), logEventsTag.end());
    auto logEventsLen = encodeVarint32(logEventsData.size());
    result.insert(result.end(), logEventsLen.begin(), logEventsLen.end());
    result.insert(result.end(), logEventsData.begin(), logEventsData.end());

    ManualPBParser parser(result.data(), result.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    // Should successfully skip large varints and parse the LogEvent
    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should successfully skip large varint values");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& logEvent = eventGroup.GetEvents()[0].Cast<LogEvent>();
    APSARA_TEST_EQUAL("after-large-varints", logEvent.GetContent("data").to_string());
}

// ============================================================================
// Phase 1: Category 14 - readBytes Specialized Tests (5 test cases)
// ============================================================================

void ManualPBParserUnittest::TestReadBytesSuccess() {
    // Create a LogEvent with binary content field
    std::string binaryData = "\x01\x02\x03\x04\x05";
    std::string data;

    // Encode a simple byte field
    data += encodeVarint32Str(encodeTag(1, kLengthDelimited));
    data += encodeVarint32Str(binaryData.size());
    data += binaryData;

    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    // Test readBytes through readLengthDelimited
    uint32_t tag;
    APSARA_TEST_TRUE(parser.readVarint32(tag));

    const uint8_t* bytesData = nullptr;
    size_t bytesLength = 0;
    APSARA_TEST_TRUE(parser.readBytes(bytesData, bytesLength));
    APSARA_TEST_EQUAL(binaryData.size(), bytesLength);
    APSARA_TEST_EQUAL(0, memcmp(binaryData.data(), bytesData, bytesLength));
}

void ManualPBParserUnittest::TestReadBytesZeroLength() {
    std::string data;
    data += encodeTagStr(1, kLengthDelimited);
    data += encodeVarint32Str(0); // Zero length

    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    uint32_t tag;
    APSARA_TEST_TRUE(parser.readVarint32(tag));

    const uint8_t* bytesData = nullptr;
    size_t bytesLength = 0;
    APSARA_TEST_TRUE(parser.readBytes(bytesData, bytesLength));
    APSARA_TEST_EQUAL(0UL, bytesLength);
}

void ManualPBParserUnittest::TestReadBytesInsufficientData() {
    std::string data;
    data += encodeTagStr(1, kLengthDelimited);
    data += encodeVarint32Str(100); // Claim 100 bytes
    data += "short"; // Only provide 5 bytes

    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    uint32_t tag;
    APSARA_TEST_TRUE(parser.readVarint32(tag));

    const uint8_t* bytesData = nullptr;
    size_t bytesLength = 0;
    APSARA_TEST_FALSE(parser.readBytes(bytesData, bytesLength));
}

void ManualPBParserUnittest::TestReadBytesVeryLarge() {
    // Create a large byte array (10KB)
    std::string binaryData(10240, 'X');
    std::string data;

    data += encodeTagStr(1, kLengthDelimited);
    data += encodeVarint32Str(binaryData.size());
    data += binaryData;

    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    uint32_t tag;
    APSARA_TEST_TRUE(parser.readVarint32(tag));

    const uint8_t* bytesData = nullptr;
    size_t bytesLength = 0;
    APSARA_TEST_TRUE(parser.readBytes(bytesData, bytesLength));
    APSARA_TEST_EQUAL(binaryData.size(), bytesLength);
}

void ManualPBParserUnittest::TestReadBytesInReadVarintFailed() {
    // Create truncated length encoding
    std::string data;
    data += encodeTag(1, kLengthDelimited);
    data += '\x80'; // Start of multi-byte varint but incomplete

    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    uint32_t tag;
    APSARA_TEST_TRUE(parser.readVarint32(tag));

    const uint8_t* bytesData = nullptr;
    size_t bytesLength = 0;
    APSARA_TEST_FALSE(parser.readBytes(bytesData, bytesLength));
}

// ============================================================================
// Phase 2: Category 15 - WireType Validation Error Tests (30 test cases)
// ============================================================================

// LogEvent wireType error tests
void ManualPBParserUnittest::TestLogEventInvalidTimestampWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(1, kFixed32));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithLogs({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for timestamp") != std::string::npos);
}

void ManualPBParserUnittest::TestLogEventInvalidContentsWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(2, kVarint));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithLogs({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for contents") != std::string::npos);
}

void ManualPBParserUnittest::TestLogEventInvalidLevelWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(3, kFixed64));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed64(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithLogs({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for level") != std::string::npos);
}

void ManualPBParserUnittest::TestLogEventInvalidFileOffsetWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(4, kLengthDelimited));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto lengthData = encodeLengthDelimited("12345");
    eventData.insert(eventData.end(), lengthData.begin(), lengthData.end());

    auto data = encodePipelineEventGroupWithLogs({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for file offset") != std::string::npos);
}

void ManualPBParserUnittest::TestLogEventInvalidRawSizeWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(5, kFixed32));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithLogs({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for raw size") != std::string::npos);
}

// MetricEvent wireType error tests
void ManualPBParserUnittest::TestMetricEventInvalidTimestampWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(1, kFixed64));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed64(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithMetrics({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for timestamp") != std::string::npos);
}

void ManualPBParserUnittest::TestMetricEventInvalidNameWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(2, kVarint));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithMetrics({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for name") != std::string::npos);
}

void ManualPBParserUnittest::TestMetricEventInvalidTagsWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(3, kFixed32));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithMetrics({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for tags") != std::string::npos);
}

void ManualPBParserUnittest::TestMetricEventInvalidValueWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(4, kVarint));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithMetrics({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for untyped single value") != std::string::npos);
}

// SpanEvent wireType error tests
void ManualPBParserUnittest::TestSpanEventInvalidTimestampWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(1, kLengthDelimited));
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto lengthData = encodeLengthDelimited("invalid");
    eventData.insert(eventData.end(), lengthData.begin(), lengthData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for timestamp") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidStartTimeWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(8, kFixed32)); // field 8 is start_time in SpanEvent
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type for start time") != std::string::npos);
}

// Parameterized test for all SpanEvent field invalid wire types
void ManualPBParserUnittest::TestSpanEventInvalidEndTimeWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(9, kFixed32)); // field 9 (end_time) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidTraceIdWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(2, kVarint)); // field 2 (trace_id) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidSpanIdWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(3, kVarint)); // field 3 (span_id) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidParentSpanIdWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(5, kFixed64)); // field 5 (parent_span_id) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed64(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidNameWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(6, kVarint)); // field 6 (name) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidKindWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(7, kLengthDelimited)); // field 7 (kind) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto lengthData = encodeLengthDelimited("test");
    eventData.insert(eventData.end(), lengthData.begin(), lengthData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidStatusWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(13, kFixed32)); // field 13 (status) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidTagsWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(10, kVarint)); // field 10 (tags) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidScopeTagsWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(14, kFixed64)); // field 14 (scope_tags) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed64(12345);
    eventData.insert(eventData.end(), fixedData.begin(), fixedData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanEventInvalidTraceStateWireType() {
    std::vector<uint8_t> eventData;
    auto tagData = encodeVarint32(encodeTag(4, kVarint)); // field 4 (trace_state) with wrong type
    eventData.insert(eventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    eventData.insert(eventData.end(), varintData.begin(), varintData.end());

    auto data = encodePipelineEventGroupWithSpans({eventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

// SpanInnerEvent wireType tests - consolidated into minimal implementations
void ManualPBParserUnittest::TestSpanInnerEventInvalidTimestampWireType() {
    // Create SpanEvent with InnerEvent that has invalid timestamp wire type
    std::vector<uint8_t> innerEventData;
    auto tagData = encodeVarint32(encodeTag(1, kLengthDelimited)); // field 1 (timestamp) with wrong type
    innerEventData.insert(innerEventData.end(), tagData.begin(), tagData.end());
    auto lengthData = encodeLengthDelimited("invalid");
    innerEventData.insert(innerEventData.end(), lengthData.begin(), lengthData.end());

    auto spanEventData = encodeSpanEvent(
        123456789ULL, "trace123", "span456", "test", 0, 100ULL, 200ULL, {}, {}, "", "", 0, {innerEventData});
    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanInnerEventInvalidNameWireType() {
    std::vector<uint8_t> innerEventData;
    auto tagData = encodeVarint32(encodeTag(2, kVarint)); // field 2 (name) with wrong type
    innerEventData.insert(innerEventData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    innerEventData.insert(innerEventData.end(), varintData.begin(), varintData.end());

    auto spanEventData = encodeSpanEvent(
        123456789ULL, "trace123", "span456", "test", 0, 100ULL, 200ULL, {}, {}, "", "", 0, {innerEventData});
    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanInnerEventInvalidTagsWireType() {
    std::vector<uint8_t> innerEventData;
    auto tagData = encodeVarint32(encodeTag(3, kFixed32)); // field 3 (tags) with wrong type
    innerEventData.insert(innerEventData.end(), tagData.begin(), tagData.end());
    auto fixedData = encodeFixed32(12345);
    innerEventData.insert(innerEventData.end(), fixedData.begin(), fixedData.end());

    auto spanEventData = encodeSpanEvent(
        123456789ULL, "trace123", "span456", "test", 0, 100ULL, 200ULL, {}, {}, "", "", 0, {innerEventData});
    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestSpanLinkInvalidTraceStateWireType() {
    std::vector<uint8_t> linkData;
    // Add valid trace_id and span_id first
    auto traceIdTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    linkData.insert(linkData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceIdData = encodeLengthDelimited("trace123");
    linkData.insert(linkData.end(), traceIdData.begin(), traceIdData.end());

    auto spanIdTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    linkData.insert(linkData.end(), spanIdTag.begin(), spanIdTag.end());
    auto spanIdData = encodeLengthDelimited("span456");
    linkData.insert(linkData.end(), spanIdData.begin(), spanIdData.end());

    // Add invalid trace_state wire type
    auto traceStateTag = encodeVarint32(encodeTag(3, kVarint)); // field 3 with wrong type
    linkData.insert(linkData.end(), traceStateTag.begin(), traceStateTag.end());
    auto varintData = encodeVarint32(12345);
    linkData.insert(linkData.end(), varintData.begin(), varintData.end());

    auto spanEventData
        = encodeSpanEventWithLinks(123456789ULL, "trace1", "span1", "test", 0, 100ULL, 200ULL, {linkData});
    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

// LogContent wireType error tests
void ManualPBParserUnittest::TestLogContentInvalidKeyWireType() {
    std::vector<uint8_t> contentData;
    auto tagData = encodeVarint32(encodeTag(1, kVarint)); // field 1 (key) with wrong type
    contentData.insert(contentData.end(), tagData.begin(), tagData.end());
    auto varintData = encodeVarint32(12345);
    contentData.insert(contentData.end(), varintData.begin(), varintData.end());

    std::vector<uint8_t> logEventData;
    auto timestampTag = encodeVarint32(encodeTag(1, kVarint));
    logEventData.insert(logEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(123456789ULL);
    logEventData.insert(logEventData.end(), timestampData.begin(), timestampData.end());

    auto contentTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    logEventData.insert(logEventData.end(), contentTag.begin(), contentTag.end());
    auto contentLen = encodeVarint32(contentData.size());
    logEventData.insert(logEventData.end(), contentLen.begin(), contentLen.end());
    logEventData.insert(logEventData.end(), contentData.begin(), contentData.end());

    auto data = encodePipelineEventGroupWithLogs({logEventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

void ManualPBParserUnittest::TestLogContentInvalidValueWireType() {
    std::vector<uint8_t> contentData;
    // Add valid key
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    contentData.insert(contentData.end(), keyTag.begin(), keyTag.end());
    auto keyData = encodeLengthDelimited("key");
    contentData.insert(contentData.end(), keyData.begin(), keyData.end());

    // Add invalid value wire type
    auto valueTag = encodeVarint32(encodeTag(2, kVarint)); // field 2 (value) with wrong type
    contentData.insert(contentData.end(), valueTag.begin(), valueTag.end());
    auto varintData = encodeVarint32(12345);
    contentData.insert(contentData.end(), varintData.begin(), varintData.end());

    std::vector<uint8_t> logEventData;
    auto timestampTag = encodeVarint32(encodeTag(1, kVarint));
    logEventData.insert(logEventData.end(), timestampTag.begin(), timestampTag.end());
    auto timestampData = encodeVarint64(123456789ULL);
    logEventData.insert(logEventData.end(), timestampData.begin(), timestampData.end());

    auto contentTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    logEventData.insert(logEventData.end(), contentTag.begin(), contentTag.end());
    auto contentLen = encodeVarint32(contentData.size());
    logEventData.insert(logEventData.end(), contentLen.begin(), contentLen.end());
    logEventData.insert(logEventData.end(), contentData.begin(), contentData.end());

    auto data = encodePipelineEventGroupWithLogs({logEventData});
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_TRUE(errMsg.find("Invalid wire type") != std::string::npos);
}

// ============================================================================
// Category 16: Enum Tests (Phase 3 & 4) - Consolidated
// ============================================================================

// Test SpanEvent with all Kind enum values
void ManualPBParserUnittest::TestSpanEventKindEnum() {
    const std::vector<uint32_t> kindValues = {0, 1, 2, 3, 4, 5}; // All Kind enum values
    const std::vector<SpanEvent::Kind> expectedKinds = {SpanEvent::Kind::Unspecified,
                                                        SpanEvent::Kind::Internal,
                                                        SpanEvent::Kind::Server,
                                                        SpanEvent::Kind::Client,
                                                        SpanEvent::Kind::Producer,
                                                        SpanEvent::Kind::Consumer};

    for (size_t i = 0; i < kindValues.size(); ++i) {
        auto spanEventData
            = encodeSpanEvent(123456789ULL, "trace123", "span456", "test", kindValues[i], 100ULL, 200ULL);
        auto data = encodePipelineEventGroupWithSpans({spanEventData});

        auto sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string errMsg;
        ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

        APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        const SpanEvent& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
        APSARA_TEST_EQUAL_DESC(static_cast<uint32_t>(expectedKinds[i]),
                               static_cast<uint32_t>(spanEvent.GetKind()),
                               "SpanEvent Kind should match");
    }
}

// Test SpanEvent with all Status enum values
void ManualPBParserUnittest::TestSpanEventStatusEnum() {
    const std::vector<uint32_t> statusValues = {0, 1, 2}; // All Status enum values
    const std::vector<SpanEvent::StatusCode> expectedStatuses
        = {SpanEvent::StatusCode::Unset, SpanEvent::StatusCode::Ok, SpanEvent::StatusCode::Error};

    for (size_t i = 0; i < statusValues.size(); ++i) {
        auto spanEventData = encodeSpanEvent(
            123456789ULL, "trace123", "span456", "test", 0, 100ULL, 200ULL, {}, {}, "", "", statusValues[i]);
        auto data = encodePipelineEventGroupWithSpans({spanEventData});

        auto sourceBuffer = make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string errMsg;
        ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

        APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        const SpanEvent& spanEvent = eventGroup.GetEvents()[0].Cast<SpanEvent>();
        APSARA_TEST_EQUAL_DESC(static_cast<uint32_t>(expectedStatuses[i]),
                               static_cast<uint32_t>(spanEvent.GetStatus()),
                               "SpanEvent Status should match");
    }
}

// Test MetricEvent with Fixed32 value (test float value encoded as fixed32)
void ManualPBParserUnittest::TestMetricEventFixed32Value() {
    // Create MetricEvent with Fixed32 value (float converted to double)
    // Although the protobuf parser currently only handles Fixed64 for UntypedSingleValue,
    // we can test Fixed32 by encoding a float value
    vector<pair<string, string>> tags = {{"type", "cpu"}};

    // Create UntypedSingleValue with fixed32 (field 1, wire type 5)
    std::vector<uint8_t> untypedValue;
    auto valueTag = encodeVarint32(encodeTag(1, kFixed32)); // field 1, wire type 5 (fixed32)
    untypedValue.insert(untypedValue.end(), valueTag.begin(), valueTag.end());

    // Encode float as fixed32
    float floatValue = 42.5F;
    uint32_t floatBytes = 0;
    memcpy(&floatBytes, &floatValue, sizeof(float));
    auto floatData = encodeFixed32(floatBytes);
    untypedValue.insert(untypedValue.end(), floatData.begin(), floatData.end());

    // Build MetricEvent
    std::vector<uint8_t> metricEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    metricEventData.insert(metricEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1234567890ULL);
    metricEventData.insert(metricEventData.end(), tsData.begin(), tsData.end());

    // field 2: name
    auto nameTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), nameTag.begin(), nameTag.end());
    std::string name = "cpu_usage";
    auto nameData = encodeLengthDelimited(name);
    metricEventData.insert(metricEventData.end(), nameData.begin(), nameData.end());

    // field 4: value (UntypedSingleValue)
    auto valueFieldTag = encodeVarint32(encodeTag(4, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), valueFieldTag.begin(), valueFieldTag.end());
    auto valueLen = encodeVarint32(untypedValue.size());
    metricEventData.insert(metricEventData.end(), valueLen.begin(), valueLen.end());
    metricEventData.insert(metricEventData.end(), untypedValue.begin(), untypedValue.end());

    auto data = encodePipelineEventGroupWithMetrics({metricEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    // Note: The current parser only handles Fixed64 for UntypedSingleValue
    // This test verifies that Fixed32 is properly skipped (not causing errors)
    APSARA_TEST_TRUE_DESC(parser.ParsePipelineEventGroup(eventGroup, errMsg),
                          "Should parse MetricEvent even with Fixed32 value field");
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test MetricEvent with Fixed64 value (explicit test with double encoding)
void ManualPBParserUnittest::TestMetricEventFixed64Value() {
    // Test explicit Fixed64 encoding for UntypedSingleValue
    double testValue = 123.456;
    vector<pair<string, string>> tags = {{"type", "memory"}};
    auto metricEventData = encodeMetricEvent(9876543210ULL, "mem_usage", tags, testValue, true);

    auto data = encodePipelineEventGroupWithMetrics({metricEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

    const auto& metricEvent = eventGroup.GetEvents()[0].Cast<MetricEvent>();
    APSARA_TEST_EQUAL("mem_usage", metricEvent.GetName().to_string());
    APSARA_TEST_EQUAL(testValue, metricEvent.GetValue<UntypedSingleValue>()->mValue);
}

// ============================================================================
// Category 17: Read Failure Tests - Consolidated (Phase 3 & 4)
// ============================================================================

// Test nested readVarint32 failure in Events list (consolidated for Log/Metric/Span)
void ManualPBParserUnittest::TestLogEventsListReadTagFailure() {
    // Create truncated tag in LogEvents list
    std::vector<uint8_t> eventsListData = {0x80, 0x80, 0x80}; // Incomplete varint tag
    std::vector<uint8_t> data;
    auto logsTag = encodeVarint32(encodeTag(3, kLengthDelimited)); // field 3 (Logs)
    data.insert(data.end(), logsTag.begin(), logsTag.end());
    auto logsLen = encodeVarint32(eventsListData.size());
    data.insert(data.end(), logsLen.begin(), logsLen.end());
    data.insert(data.end(), eventsListData.begin(), eventsListData.end());

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestMetricEventsListReadTagFailure() {
    // Create truncated tag in MetricEvents list
    std::vector<uint8_t> eventsListData = {0x80, 0x80, 0x80}; // Incomplete varint tag
    std::vector<uint8_t> data;
    auto metricsTag = encodeVarint32(encodeTag(4, kLengthDelimited)); // field 4 (Metrics)
    data.insert(data.end(), metricsTag.begin(), metricsTag.end());
    auto metricsLen = encodeVarint32(eventsListData.size());
    data.insert(data.end(), metricsLen.begin(), metricsLen.end());
    data.insert(data.end(), eventsListData.begin(), eventsListData.end());

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanEventsListReadTagFailure() {
    // Create truncated tag in SpanEvents list
    std::vector<uint8_t> eventsListData = {0x80, 0x80, 0x80}; // Incomplete varint tag
    std::vector<uint8_t> data;
    auto spansTag = encodeVarint32(encodeTag(5, kLengthDelimited)); // field 5 (Spans)
    data.insert(data.end(), spansTag.begin(), spansTag.end());
    auto spansLen = encodeVarint32(eventsListData.size());
    data.insert(data.end(), spansLen.begin(), spansLen.end());
    data.insert(data.end(), eventsListData.begin(), eventsListData.end());

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

// ============================================================================
// Read Failure Tests - Detailed Implementation
// ============================================================================

void ManualPBParserUnittest::TestLogEventReadLevelFailed() {
    // Create LogEvent with truncated level string length
    std::vector<uint8_t> logEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    logEventData.insert(logEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000000ULL);
    logEventData.insert(logEventData.end(), tsData.begin(), tsData.end());

    // field 5: level with incomplete length
    auto levelTag = encodeVarint32(encodeTag(5, kLengthDelimited));
    logEventData.insert(logEventData.end(), levelTag.begin(), levelTag.end());
    logEventData.push_back(0x80); // Incomplete varint length

    auto data = encodePipelineEventGroupWithLogs({logEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestLogEventReadTimestampFailed() {
    // Create LogEvent with truncated timestamp
    std::vector<uint8_t> logEventData;

    // field 1: timestamp with incomplete varint
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    logEventData.insert(logEventData.end(), tsTag.begin(), tsTag.end());
    logEventData.push_back(0x80); // Incomplete varint

    auto data = encodePipelineEventGroupWithLogs({logEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestMetricEventReadTimestampFailed() {
    // Create MetricEvent with truncated timestamp
    std::vector<uint8_t> metricEventData;

    // field 1: timestamp with incomplete varint
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    metricEventData.insert(metricEventData.end(), tsTag.begin(), tsTag.end());
    metricEventData.push_back(0x80); // Incomplete varint
    metricEventData.push_back(0x80); // Incomplete varint

    auto data = encodePipelineEventGroupWithMetrics({metricEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanEventReadTraceIdFailedPhase3() {
    // Create SpanEvent with incomplete traceId
    std::vector<uint8_t> spanEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    spanEventData.insert(spanEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    spanEventData.insert(spanEventData.end(), tsData.begin(), tsData.end());

    // field 2: traceId with incomplete length
    auto traceIdTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), traceIdTag.begin(), traceIdTag.end());
    spanEventData.push_back(0x80); // Incomplete varint length

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanEventReadSpanIdFailed() {
    // Create SpanEvent with incomplete spanId
    std::vector<uint8_t> spanEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    spanEventData.insert(spanEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    spanEventData.insert(spanEventData.end(), tsData.begin(), tsData.end());

    // field 2: traceId (valid)
    auto traceIdTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceId = encodeLengthDelimited("trace123");
    spanEventData.insert(spanEventData.end(), traceId.begin(), traceId.end());

    // field 3: spanId with incomplete length
    auto spanIdTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), spanIdTag.begin(), spanIdTag.end());
    spanEventData.push_back(0x80); // Incomplete varint length

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanEventReadNameFailed() {
    // Create SpanEvent with incomplete name
    std::vector<uint8_t> spanEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    spanEventData.insert(spanEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    spanEventData.insert(spanEventData.end(), tsData.begin(), tsData.end());

    // field 2: traceId
    auto traceIdTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceId = encodeLengthDelimited("trace123");
    spanEventData.insert(spanEventData.end(), traceId.begin(), traceId.end());

    // field 3: spanId
    auto spanIdTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), spanIdTag.begin(), spanIdTag.end());
    auto spanId = encodeLengthDelimited("span456");
    spanEventData.insert(spanEventData.end(), spanId.begin(), spanId.end());

    // field 5: name with incomplete length
    auto nameTag = encodeVarint32(encodeTag(5, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), nameTag.begin(), nameTag.end());
    spanEventData.push_back(0x80); // Incomplete varint length

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanEventReadStartTimeFailed() {
    // Create SpanEvent with incomplete startTime
    std::vector<uint8_t> spanEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    spanEventData.insert(spanEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    spanEventData.insert(spanEventData.end(), tsData.begin(), tsData.end());

    // field 2: traceId
    auto traceIdTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceId = encodeLengthDelimited("trace123");
    spanEventData.insert(spanEventData.end(), traceId.begin(), traceId.end());

    // field 3: spanId
    auto spanIdTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), spanIdTag.begin(), spanIdTag.end());
    auto spanId = encodeLengthDelimited("span456");
    spanEventData.insert(spanEventData.end(), spanId.begin(), spanId.end());

    // field 7: startTime with incomplete varint
    auto startTimeTag = encodeVarint32(encodeTag(7, kVarint));
    spanEventData.insert(spanEventData.end(), startTimeTag.begin(), startTimeTag.end());
    spanEventData.push_back(0x80); // Incomplete varint

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanEventReadEndTimeFailed() {
    // Create SpanEvent with incomplete endTime
    std::vector<uint8_t> spanEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    spanEventData.insert(spanEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    spanEventData.insert(spanEventData.end(), tsData.begin(), tsData.end());

    // field 2: traceId
    auto traceIdTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceId = encodeLengthDelimited("trace123");
    spanEventData.insert(spanEventData.end(), traceId.begin(), traceId.end());

    // field 3: spanId
    auto spanIdTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    spanEventData.insert(spanEventData.end(), spanIdTag.begin(), spanIdTag.end());
    auto spanId = encodeLengthDelimited("span456");
    spanEventData.insert(spanEventData.end(), spanId.begin(), spanId.end());

    // field 8: endTime with incomplete varint
    auto endTimeTag = encodeVarint32(encodeTag(8, kVarint));
    spanEventData.insert(spanEventData.end(), endTimeTag.begin(), endTimeTag.end());
    spanEventData.push_back(0x80); // Incomplete varint
    spanEventData.push_back(0x80); // Incomplete varint

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestSpanInnerEventReadNameFailed() {
    // Create SpanInnerEvent with incomplete name
    std::vector<uint8_t> innerEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    innerEventData.insert(innerEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(500ULL);
    innerEventData.insert(innerEventData.end(), tsData.begin(), tsData.end());

    // field 2: name with incomplete length
    auto nameTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    innerEventData.insert(innerEventData.end(), nameTag.begin(), nameTag.end());
    innerEventData.push_back(0x80); // Incomplete varint length

    // Wrap in SpanEvent
    auto spanEventData
        = encodeSpanEvent(1000ULL, "trace", "span", "test", 0, 100ULL, 200ULL, {}, {}, "", "", 0, {innerEventData});
    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestLogContentReadTagFailed() {
    // Create LogContent (map entry) with incomplete tag
    std::vector<uint8_t> logContentData;
    logContentData.push_back(0x80); // Incomplete varint tag

    // Wrap in LogEvent
    std::vector<uint8_t> logEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    logEventData.insert(logEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    logEventData.insert(logEventData.end(), tsData.begin(), tsData.end());

    // field 2: contents with malformed entry
    auto contentTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    logEventData.insert(logEventData.end(), contentTag.begin(), contentTag.end());
    auto contentLen = encodeVarint32(logContentData.size());
    logEventData.insert(logEventData.end(), contentLen.begin(), contentLen.end());
    logEventData.insert(logEventData.end(), logContentData.begin(), logContentData.end());

    auto data = encodePipelineEventGroupWithLogs({logEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestMapEntryReadKeyFailedPhase3() {
    // Create map entry with incomplete key
    std::vector<uint8_t> mapEntryData;

    // field 1: key with incomplete length
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), keyTag.begin(), keyTag.end());
    mapEntryData.push_back(0x80); // Incomplete varint length

    // Wrap in MetricEvent tags
    std::vector<uint8_t> metricEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    metricEventData.insert(metricEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    metricEventData.insert(metricEventData.end(), tsData.begin(), tsData.end());

    // field 2: name
    auto nameTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), nameTag.begin(), nameTag.end());
    auto name = encodeLengthDelimited("test");
    metricEventData.insert(metricEventData.end(), name.begin(), name.end());

    // field 3: tags with malformed entry
    auto tagsTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), tagsTag.begin(), tagsTag.end());
    auto tagsLen = encodeVarint32(mapEntryData.size());
    metricEventData.insert(metricEventData.end(), tagsLen.begin(), tagsLen.end());
    metricEventData.insert(metricEventData.end(), mapEntryData.begin(), mapEntryData.end());

    auto data = encodePipelineEventGroupWithMetrics({metricEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestMapEntryReadValueFailedPhase3() {
    // Create map entry with incomplete value
    std::vector<uint8_t> mapEntryData;

    // field 1: key (valid)
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), keyTag.begin(), keyTag.end());
    auto key = encodeLengthDelimited("testkey");
    mapEntryData.insert(mapEntryData.end(), key.begin(), key.end());

    // field 2: value with incomplete length
    auto valueTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), valueTag.begin(), valueTag.end());
    mapEntryData.push_back(0x80); // Incomplete varint length

    // Wrap in MetricEvent tags
    std::vector<uint8_t> metricEventData;

    // field 1: timestamp
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    metricEventData.insert(metricEventData.end(), tsTag.begin(), tsTag.end());
    auto tsData = encodeVarint64(1000ULL);
    metricEventData.insert(metricEventData.end(), tsData.begin(), tsData.end());

    // field 2: name
    auto nameTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), nameTag.begin(), nameTag.end());
    auto name = encodeLengthDelimited("test");
    metricEventData.insert(metricEventData.end(), name.begin(), name.end());

    // field 3: tags with malformed entry
    auto tagsTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), tagsTag.begin(), tagsTag.end());
    auto tagsLen = encodeVarint32(mapEntryData.size());
    metricEventData.insert(metricEventData.end(), tagsLen.begin(), tagsLen.end());
    metricEventData.insert(metricEventData.end(), mapEntryData.begin(), mapEntryData.end());

    auto data = encodePipelineEventGroupWithMetrics({metricEventData});

    ManualPBParser parser(data.data(), data.size(), false);
    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    string errMsg;

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

void ManualPBParserUnittest::TestVarint32MultiByteEncoding() {
    // Test various multi-byte varint32 encodings
    std::vector<uint32_t> testValues = {
        127, // 1-byte: 0x7F
        128, // 2-byte: 0x80 0x01
        16383, // 2-byte: 0xFF 0x7F
        16384, // 3-byte: 0x80 0x80 0x01
        2097151, // 3-byte: 0xFF 0xFF 0x7F
        2097152, // 4-byte: 0x80 0x80 0x80 0x01
        268435455, // 4-byte: 0xFF 0xFF 0xFF 0x7F
        268435456, // 5-byte: 0x80 0x80 0x80 0x80 0x01
        0xFFFFFFFF // 5-byte: max uint32
    };

    for (uint32_t value : testValues) {
        auto encoded = encodeVarint32(value);
        ManualPBParser parser(encoded.data(), encoded.size(), false);

        uint32_t decoded = 0;
        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadVarint32(parser, decoded),
                              "Should decode varint32 value: " + std::to_string(value));
        APSARA_TEST_EQUAL_DESC(value, decoded, "Decoded value should match: " + std::to_string(value));
    }
}

void ManualPBParserUnittest::TestVarint64MultiByteEncoding() {
    // Test various multi-byte varint64 encodings
    std::vector<uint64_t> testValues = {
        127ULL, // 1-byte
        128ULL, // 2-byte
        16383ULL, // 2-byte
        16384ULL, // 3-byte
        2097151ULL, // 3-byte
        2097152ULL, // 4-byte
        268435455ULL, // 4-byte
        268435456ULL, // 5-byte
        34359738367ULL, // 5-byte
        34359738368ULL, // 6-byte
        4398046511103ULL, // 6-byte
        4398046511104ULL, // 7-byte
        562949953421311ULL, // 7-byte
        562949953421312ULL, // 8-byte
        72057594037927935ULL, // 8-byte
        72057594037927936ULL, // 9-byte
        0xFFFFFFFFFFFFFFFFULL // 10-byte: max uint64
    };

    for (uint64_t value : testValues) {
        auto encoded = encodeVarint64(value);
        ManualPBParser parser(encoded.data(), encoded.size(), false);

        uint64_t decoded = 0;
        APSARA_TEST_TRUE_DESC(ManualPBParserTestHelper::TestReadVarint64(parser, decoded),
                              "Should decode varint64 value: " + std::to_string(value));
        APSARA_TEST_EQUAL_DESC(value, decoded, "Decoded value should match: " + std::to_string(value));
    }
}

// ============================================================================
// Phase 5-8: Coverage Improvement Tests
// ============================================================================

// Phase 5: Unknown field tests - test skipField in various parsing contexts

// Test unknown field in SpanEvents list
void ManualPBParserUnittest::TestSpanEventsListUnknownField() {
    // Create a span event followed by an unknown field (field number 999)
    auto spanEventData = encodeSpanEvent(123456789ULL, "trace123", "span456", "test_span", 0, 100ULL, 200ULL);

    std::vector<uint8_t> eventsListData;
    // Add the valid span event
    eventsListData.insert(eventsListData.end(), spanEventData.begin(), spanEventData.end());
    // Add unknown field: field 999 with varint wire type
    auto unknownTag = encodeVarint32(encodeTag(999, kVarint));
    eventsListData.insert(eventsListData.end(), unknownTag.begin(), unknownTag.end());
    auto unknownValue = encodeVarint32(12345);
    eventsListData.insert(eventsListData.end(), unknownValue.begin(), unknownValue.end());

    auto data = encodePipelineEventGroupWithSpans({eventsListData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test unknown field in LogEvents list
void ManualPBParserUnittest::TestLogEventsListUnknownField() {
    auto logEventData = encodeLogEvent(123456789ULL, {{"key", "value"}}, "", 0, 0);

    std::vector<uint8_t> eventsListData;
    eventsListData.insert(eventsListData.end(), logEventData.begin(), logEventData.end());
    // Add unknown field
    auto unknownTag = encodeVarint32(encodeTag(888, kLengthDelimited));
    eventsListData.insert(eventsListData.end(), unknownTag.begin(), unknownTag.end());
    std::string unknownData = "unknown data";
    auto unknownLen = encodeVarint32(unknownData.size());
    eventsListData.insert(eventsListData.end(), unknownLen.begin(), unknownLen.end());
    eventsListData.insert(eventsListData.end(), unknownData.begin(), unknownData.end());

    auto data = encodePipelineEventGroupWithLogs({eventsListData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test unknown field in MetricEvents list
void ManualPBParserUnittest::TestMetricEventsListUnknownField() {
    auto metricEventData = encodeMetricEvent(123456789ULL, "metric1", {}, 100.0);

    std::vector<uint8_t> eventsListData;
    eventsListData.insert(eventsListData.end(), metricEventData.begin(), metricEventData.end());
    // Add unknown field with Fixed64 wire type
    auto unknownTag = encodeVarint32(encodeTag(777, kFixed64));
    eventsListData.insert(eventsListData.end(), unknownTag.begin(), unknownTag.end());
    auto unknownValue = encodeFixed64(0x1234567890ABCDEFULL);
    eventsListData.insert(eventsListData.end(), unknownValue.begin(), unknownValue.end());

    auto data = encodePipelineEventGroupWithMetrics({eventsListData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Phase 6: Metric value parsing failure tests

// Test readLengthDelimited failure in MetricEvent value field
void ManualPBParserUnittest::TestMetricValueReadLengthDelimitedFailed() {
    std::vector<uint8_t> eventData;
    // timestamp
    auto timestamp = encodeVarint32(encodeTag(1, kVarint));
    eventData.insert(eventData.end(), timestamp.begin(), timestamp.end());
    auto tsVal = encodeVarint64(123456789ULL);
    eventData.insert(eventData.end(), tsVal.begin(), tsVal.end());
    // name
    auto nameTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    eventData.insert(eventData.end(), nameTag.begin(), nameTag.end());
    std::string name = "test_metric";
    auto nameLen = encodeVarint32(name.size());
    eventData.insert(eventData.end(), nameLen.begin(), nameLen.end());
    eventData.insert(eventData.end(), name.begin(), name.end());
    // value field with incorrect length
    auto valueTag = encodeVarint32(encodeTag(4, kLengthDelimited));
    eventData.insert(eventData.end(), valueTag.begin(), valueTag.end());
    auto valueLen = encodeVarint32(100); // Claim 100 bytes
    eventData.insert(eventData.end(), valueLen.begin(), valueLen.end());
    // Only provide 5 bytes
    eventData.push_back('s');
    eventData.push_back('h');
    eventData.push_back('o');
    eventData.push_back('r');
    eventData.push_back('t');

    auto data = encodePipelineEventGroupWithMetrics({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

// Test readVarint32 failure in MetricEvent value message
void ManualPBParserUnittest::TestMetricValueReadTagFailed() {
    std::vector<uint8_t> eventData;
    // timestamp
    auto timestamp = encodeVarint32(encodeTag(1, kVarint));
    eventData.insert(eventData.end(), timestamp.begin(), timestamp.end());
    auto tsVal = encodeVarint64(123456789ULL);
    eventData.insert(eventData.end(), tsVal.begin(), tsVal.end());
    // value field with incomplete tag inside
    auto valueTag = encodeVarint32(encodeTag(4, kLengthDelimited));
    eventData.insert(eventData.end(), valueTag.begin(), valueTag.end());
    std::vector<uint8_t> valueData;
    valueData.push_back(0x80); // Incomplete varint tag
    auto valueLen = encodeVarint32(valueData.size());
    eventData.insert(eventData.end(), valueLen.begin(), valueLen.end());
    eventData.insert(eventData.end(), valueData.begin(), valueData.end());

    auto data = encodePipelineEventGroupWithMetrics({eventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

// Phase 7: Nested message parsing failures

// Test unknown field in LogContent
void ManualPBParserUnittest::TestLogContentUnknownField() {
    // Create a LogContent message with key, value, and an unknown field
    std::vector<uint8_t> contentData;
    // key
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    contentData.insert(contentData.end(), keyTag.begin(), keyTag.end());
    std::string key = "testkey";
    auto keyLen = encodeVarint32(key.size());
    contentData.insert(contentData.end(), keyLen.begin(), keyLen.end());
    contentData.insert(contentData.end(), key.begin(), key.end());
    // value
    auto valTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    contentData.insert(contentData.end(), valTag.begin(), valTag.end());
    std::string val = "testvalue";
    auto valLen = encodeVarint32(val.size());
    contentData.insert(contentData.end(), valLen.begin(), valLen.end());
    contentData.insert(contentData.end(), val.begin(), val.end());
    // Unknown field 999
    auto unknownTag = encodeVarint32(encodeTag(999, kVarint));
    contentData.insert(contentData.end(), unknownTag.begin(), unknownTag.end());
    auto unknownVal = encodeVarint32(123);
    contentData.insert(contentData.end(), unknownVal.begin(), unknownVal.end());

    // Wrap in LogEvent
    std::vector<uint8_t> logEventData;
    auto contentsTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    logEventData.insert(logEventData.end(), contentsTag.begin(), contentsTag.end());
    auto contentsLen = encodeVarint32(contentData.size());
    logEventData.insert(logEventData.end(), contentsLen.begin(), contentsLen.end());
    logEventData.insert(logEventData.end(), contentData.begin(), contentData.end());

    auto data = encodePipelineEventGroupWithLogs({logEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test unknown field in MetricTags map entry
void ManualPBParserUnittest::TestMetricTagsUnknownField() {
    // Create MetricEvent with tags containing unknown field
    std::vector<uint8_t> mapEntryData;
    // key
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), keyTag.begin(), keyTag.end());
    std::string key = "tag1";
    auto keyLen = encodeVarint32(key.size());
    mapEntryData.insert(mapEntryData.end(), keyLen.begin(), keyLen.end());
    mapEntryData.insert(mapEntryData.end(), key.begin(), key.end());
    // value
    auto valTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), valTag.begin(), valTag.end());
    std::string val = "value1";
    auto valLen = encodeVarint32(val.size());
    mapEntryData.insert(mapEntryData.end(), valLen.begin(), valLen.end());
    mapEntryData.insert(mapEntryData.end(), val.begin(), val.end());
    // Unknown field
    auto unknownTag = encodeVarint32(encodeTag(999, kFixed32));
    mapEntryData.insert(mapEntryData.end(), unknownTag.begin(), unknownTag.end());
    auto unknownVal = encodeFixed32(123456);
    mapEntryData.insert(mapEntryData.end(), unknownVal.begin(), unknownVal.end());

    // Wrap in tags field
    std::vector<uint8_t> tagsData;
    auto entryTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    tagsData.insert(tagsData.end(), entryTag.begin(), entryTag.end());
    auto entryLen = encodeVarint32(mapEntryData.size());
    tagsData.insert(tagsData.end(), entryLen.begin(), entryLen.end());
    tagsData.insert(tagsData.end(), mapEntryData.begin(), mapEntryData.end());

    // Create MetricEvent
    auto metricData = encodeMetricEvent(123456789ULL, "test_metric", {}, 10.0, false);
    std::vector<uint8_t> metricEventData(metricData.begin(), metricData.end());
    // Add tags field
    auto metricTagsTag = encodeVarint32(encodeTag(3, kLengthDelimited));
    metricEventData.insert(metricEventData.end(), metricTagsTag.begin(), metricTagsTag.end());
    auto tagsLen = encodeVarint32(tagsData.size());
    metricEventData.insert(metricEventData.end(), tagsLen.begin(), tagsLen.end());
    metricEventData.insert(metricEventData.end(), tagsData.begin(), tagsData.end());

    auto data = encodePipelineEventGroupWithMetrics({metricEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test unknown field in SpanTags
void ManualPBParserUnittest::TestSpanTagsUnknownField() {
    // Create SpanEvent with tags containing unknown field
    std::vector<uint8_t> mapEntryData;
    // key
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), keyTag.begin(), keyTag.end());
    std::string key = "span_tag1";
    auto keyLen = encodeVarint32(key.size());
    mapEntryData.insert(mapEntryData.end(), keyLen.begin(), keyLen.end());
    mapEntryData.insert(mapEntryData.end(), key.begin(), key.end());
    // value
    auto valTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), valTag.begin(), valTag.end());
    std::string val = "span_value1";
    auto valLen = encodeVarint32(val.size());
    mapEntryData.insert(mapEntryData.end(), valLen.begin(), valLen.end());
    mapEntryData.insert(mapEntryData.end(), val.begin(), val.end());
    // Unknown field (should be skipped)
    auto unknownTag = encodeVarint32(encodeTag(888, kFixed32));
    mapEntryData.insert(mapEntryData.end(), unknownTag.begin(), unknownTag.end());
    auto unknownVal = encodeFixed32(123456);
    mapEntryData.insert(mapEntryData.end(), unknownVal.begin(), unknownVal.end());

    // Wrap in tags field
    std::vector<uint8_t> tagsData;
    auto entryTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    tagsData.insert(tagsData.end(), entryTag.begin(), entryTag.end());
    auto entryLen = encodeVarint32(mapEntryData.size());
    tagsData.insert(tagsData.end(), entryLen.begin(), entryLen.end());
    tagsData.insert(tagsData.end(), mapEntryData.begin(), mapEntryData.end());

    // Create SpanEvent without tags
    auto spanEventData
        = encodeSpanEvent(1000000ULL, "trace-123", "span-456", "test-span", 0, 100ULL, 200ULL, {}, {}, "", "", 0);
    std::vector<uint8_t> spanWithUnknown(spanEventData.begin(), spanEventData.end());

    // Add tags field with unknown field
    auto spanTagsTag = encodeVarint32(encodeTag(10, kLengthDelimited)); // field 10: tags
    spanWithUnknown.insert(spanWithUnknown.end(), spanTagsTag.begin(), spanTagsTag.end());
    auto tagsLen = encodeVarint32(tagsData.size());
    spanWithUnknown.insert(spanWithUnknown.end(), tagsLen.begin(), tagsLen.end());
    spanWithUnknown.insert(spanWithUnknown.end(), tagsData.begin(), tagsData.end());

    auto data = encodePipelineEventGroupWithSpans({spanWithUnknown});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Test unknown field in SpanScopeTags
void ManualPBParserUnittest::TestSpanScopeTagsUnknownField() {
    // Create SpanEvent with scope_tags containing unknown field
    std::vector<uint8_t> mapEntryData;
    // key
    auto keyTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), keyTag.begin(), keyTag.end());
    std::string key = "scope_tag1";
    auto keyLen = encodeVarint32(key.size());
    mapEntryData.insert(mapEntryData.end(), keyLen.begin(), keyLen.end());
    mapEntryData.insert(mapEntryData.end(), key.begin(), key.end());
    // value
    auto valTag = encodeVarint32(encodeTag(2, kLengthDelimited));
    mapEntryData.insert(mapEntryData.end(), valTag.begin(), valTag.end());
    std::string val = "scope_value1";
    auto valLen = encodeVarint32(val.size());
    mapEntryData.insert(mapEntryData.end(), valLen.begin(), valLen.end());
    mapEntryData.insert(mapEntryData.end(), val.begin(), val.end());
    // Unknown field (should be skipped)
    auto unknownTag = encodeVarint32(encodeTag(777, kFixed64));
    mapEntryData.insert(mapEntryData.end(), unknownTag.begin(), unknownTag.end());
    auto unknownVal = encodeFixed64(88888888ULL);
    mapEntryData.insert(mapEntryData.end(), unknownVal.begin(), unknownVal.end());

    // Create SpanEvent with scope tags
    std::vector<std::pair<std::string, std::string>> scopeTags = {{"scope_tag1", "scope_value1"}};
    auto spanEventData = encodeSpanEvent(
        2000000ULL, "trace-abc", "span-def", "test-scope-span", 0, 300ULL, 400ULL, {}, scopeTags, "", "", 0);

    // Manually insert the malformed scope tag entry with unknown field
    std::vector<uint8_t> spanWithUnknown;
    spanWithUnknown = spanEventData;
    // Add scope_tags field with unknown field entry
    auto scopeTagsFieldTag = encodeVarint32(encodeTag(10, kLengthDelimited)); // field 10: scope_tags
    spanWithUnknown.insert(spanWithUnknown.end(), scopeTagsFieldTag.begin(), scopeTagsFieldTag.end());
    auto entryLen = encodeVarint32(mapEntryData.size());
    spanWithUnknown.insert(spanWithUnknown.end(), entryLen.begin(), entryLen.end());
    spanWithUnknown.insert(spanWithUnknown.end(), mapEntryData.begin(), mapEntryData.end());

    auto data = encodePipelineEventGroupWithSpans({spanWithUnknown});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_TRUE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
    APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());
}

// Phase 8: Complex nested parsing failures

// Test readVarint64 failure in SpanInnerEvent timestamp
void ManualPBParserUnittest::TestSpanInnerEventReadTimestampFailed() {
    // Create a SpanInnerEvent with incomplete timestamp varint
    std::vector<uint8_t> innerEventData;
    auto tsTag = encodeVarint32(encodeTag(1, kVarint));
    innerEventData.insert(innerEventData.end(), tsTag.begin(), tsTag.end());
    innerEventData.push_back(0x80); // Incomplete varint

    // Wrap in SpanEvent inner_events field
    std::vector<uint8_t> spanEventData;
    auto innerEventsTag = encodeVarint32(encodeTag(13, kLengthDelimited)); // field 13 is inner_events
    spanEventData.insert(spanEventData.end(), innerEventsTag.begin(), innerEventsTag.end());
    auto innerEventsLen = encodeVarint32(innerEventData.size());
    spanEventData.insert(spanEventData.end(), innerEventsLen.begin(), innerEventsLen.end());
    spanEventData.insert(spanEventData.end(), innerEventData.begin(), innerEventData.end());

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

// Test readString failure in SpanLink trace_id
void ManualPBParserUnittest::TestSpanLinkReadTraceIdFailed() {
    // Create a SpanLink with truncated trace_id
    std::vector<uint8_t> linkData;
    auto traceIdTag = encodeVarint32(encodeTag(1, kLengthDelimited));
    linkData.insert(linkData.end(), traceIdTag.begin(), traceIdTag.end());
    auto traceIdLen = encodeVarint32(100); // Claim 100 bytes
    linkData.insert(linkData.end(), traceIdLen.begin(), traceIdLen.end());
    linkData.push_back('s'); // Only 5 bytes
    linkData.push_back('h');
    linkData.push_back('o');
    linkData.push_back('r');
    linkData.push_back('t');

    // Wrap in SpanEvent links field
    std::vector<uint8_t> spanEventData;
    auto linksTag = encodeVarint32(encodeTag(14, kLengthDelimited)); // field 14 is links
    spanEventData.insert(spanEventData.end(), linksTag.begin(), linksTag.end());
    auto linksLen = encodeVarint32(linkData.size());
    spanEventData.insert(spanEventData.end(), linksLen.begin(), linksLen.end());
    spanEventData.insert(spanEventData.end(), linkData.begin(), linkData.end());

    auto data = encodePipelineEventGroupWithSpans({spanEventData});

    auto sourceBuffer = make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string errMsg;
    ManualPBParser parser(reinterpret_cast<const uint8_t*>(data.data()), data.size());

    APSARA_TEST_FALSE(parser.ParsePipelineEventGroup(eventGroup, errMsg));
}

// Category 1 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint32Success)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint32MaxValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint32Invalid)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint64Success)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint64MaxValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadFixed32Success)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadFixed32InsufficientData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadFixed64Success)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadLengthDelimitedSuccess)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadLengthDelimitedInsufficientData)

// Category 2 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestParsePipelineEventGroupNullData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParsePipelineEventGroupEmptyData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParsePipelineEventGroupInvalidTag)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParsePipelineEventGroupInvalidWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetadataInvalidWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseTagsInvalidWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventsInvalidWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventsInvalidWireType)

// Category 3 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventComplete)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventMinimalFields)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithContents)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithLevel)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithFileOffset)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventWithRawSize)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventInvalidContentWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventMultipleEvents)

// Category 4 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventComplete)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventMinimalFields)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventWithTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventWithValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventDoubleValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventInvalidTagsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventInvalidValueWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventMultipleEvents)

// Category 5 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventComplete)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventMinimalFields)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithTraceInfo)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventAllKinds)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventAllStatus)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithScopeTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventWithInnerEvents)

// Category 6 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldEmpty)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldSingleEntry)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldMultipleEntries)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldKeyOnlyNoValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldValueOnlyNoKey)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldInvalidKeyWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldInvalidValueWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapFieldUnknownFieldNumber)

// Category 7 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldVarint)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldFixed32)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldFixed64)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldLengthDelimited)
UNIT_TEST_CASE(ManualPBParserUnittest, TestUnknownFieldInPipelineEventGroup)

// Category 8 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestMixedEventTypes)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMixedWithMetadataAndTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMixedEventsWithUnknownFields)
UNIT_TEST_CASE(ManualPBParserUnittest, TestComplexNestedStructure)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLargeScaleMixedData)

// Category 9 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestEmptyStrings)
UNIT_TEST_CASE(ManualPBParserUnittest, TestVeryLargeVarint)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMaximumNesting)
UNIT_TEST_CASE(ManualPBParserUnittest, TestZeroValues)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLargeNumberOfFields)

// Category 10 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkComplete)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkMinimalFields)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkWithTags)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkMultipleLinks)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkInvalidTraceIdWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkInvalidSpanIdWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkInvalidTagsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkTagsKeyOnlyNoValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkTagsInvalidKeyWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanLinkWithUnknownFields)

// Category 11 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint32Truncated)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadVarint64Truncated)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadFixed64InsufficientData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadStringLengthExceedsBounds)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseLogEventReadNameFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMetricEventReadNameFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseSpanEventReadTraceIdFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapEntryReadKeyFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestParseMapEntryReadValueFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestNestedMessageParseFailed)

// Category 12 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestVarint64MaxValueParsing)
UNIT_TEST_CASE(ManualPBParserUnittest, TestVarint3210ByteEncoding)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLengthDelimitedZeroLength)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLengthDelimitedVeryLarge)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventAllFieldsMaxValues)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventNegativeDoubleValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventTimestampZeroValue)
UNIT_TEST_CASE(ManualPBParserUnittest, TestDeepNestedSpanEventStructure)

// Category 13 test cases
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldInvalidWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldNestedLengthDelimited)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldMultipleConsecutive)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldAfterPartialParse)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSkipFieldLargeVarint)

// Category 14 test cases (Phase 1 - readBytes specialized)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadBytesSuccess)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadBytesZeroLength)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadBytesInsufficientData)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadBytesVeryLarge)
UNIT_TEST_CASE(ManualPBParserUnittest, TestReadBytesInReadVarintFailed)

// Category 15 test cases (Phase 2 - WireType validation errors)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventInvalidTimestampWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventInvalidContentsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventInvalidLevelWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventInvalidFileOffsetWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventInvalidRawSizeWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventInvalidTimestampWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventInvalidNameWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventInvalidTagsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventInvalidValueWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidTimestampWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidStartTimeWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidEndTimeWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidTraceIdWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidSpanIdWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidParentSpanIdWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidNameWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidKindWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidStatusWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidTagsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidScopeTagsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventInvalidTraceStateWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanInnerEventInvalidTimestampWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanInnerEventInvalidNameWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanInnerEventInvalidTagsWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanLinkInvalidTraceStateWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogContentInvalidKeyWireType)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogContentInvalidValueWireType)

// Category 16-17 test cases (Phase 3 & 4 - Read failures and enum tests)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventKindEnum)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventStatusEnum)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventFixed32Value)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventFixed64Value)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventsListReadTagFailure)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventsListReadTagFailure)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventsListReadTagFailure)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventReadLevelFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventReadTimestampFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventReadTimestampFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventReadTraceIdFailedPhase3)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventReadSpanIdFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventReadNameFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventReadStartTimeFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventReadEndTimeFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanInnerEventReadNameFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogContentReadTagFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMapEntryReadKeyFailedPhase3)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMapEntryReadValueFailedPhase3)
UNIT_TEST_CASE(ManualPBParserUnittest, TestVarint32MultiByteEncoding)
UNIT_TEST_CASE(ManualPBParserUnittest, TestVarint64MultiByteEncoding)

// Phase 5-8: Coverage improvement tests
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanEventsListUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogEventsListUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricEventsListUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricValueReadLengthDelimitedFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricValueReadTagFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestLogContentUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestMetricTagsUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanTagsUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanScopeTagsUnknownField)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanInnerEventReadTimestampFailed)
UNIT_TEST_CASE(ManualPBParserUnittest, TestSpanLinkReadTraceIdFailed)

} // namespace logtail

UNIT_TEST_MAIN
