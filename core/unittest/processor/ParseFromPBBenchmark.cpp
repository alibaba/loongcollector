// Copyright 2023 iLogtail Authors
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

#include <cstdlib>
#include <cstring>

#include <iostream>
#include <sstream>

#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "config/CollectionConfig.h"
#include "models/LogEvent.h"
#include "plugin/processor/inner/ProcessorParseFromPBNative.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "protobuf/models/span_event.pb.h"
#include "unittest/Unittest.h"

using namespace logtail;

std::string formatSize(long long size) {
    static const char* units[] = {" B", "KB", "MB", "GB", "TB"};
    int index = 0;
    double doubleSize = static_cast<double>(size);
    while (doubleSize >= 1024.0 && index < 4) {
        doubleSize /= 1024.0;
        index++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << std::setw(6) << std::setfill(' ') << doubleSize << " " << units[index];
    return ss.str();
}

static void runBenchmark(int size, int batchSize, std::string serializedData) {
    logtail::Logger::Instance().InitGlobalLoggers();

    CollectionPipelineContext mContext;
    mContext.SetConfigName("project##config_0");

    Json::Value config;
    config["Protocol"] = "LoongSuite";
    ProcessorParseFromPBNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorParseFromPBNative::sName, "1");

    std::cout << "protobuf data size:\t" << formatSize(serializedData.size() * size) << std::endl;

    Json::Value root;
    Json::Value events;
    for (int i = 0; i < size; i++) {
        Json::Value event;
        event["type"] = 1;
        event["timestamp"] = 1234567890;
        event["timestampNanosecond"] = 0;
        {
            Json::Value contents;
            contents["content"] = serializedData;
            event["contents"] = std::move(contents);
        }
        events.append(event);
    }

    root["events"] = events;
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(root, &oss);
    std::string inJson = oss.str();

    bool init = processor.Init(config);
    processor.CommitMetricsRecordRef();
    if (init) {
        int count = 0;
        // Perform setup here
        uint64_t durationTime = 0;
        for (int i = 0; i < batchSize; i++) {
            count++;
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.FromJsonString(inJson);

            uint64_t startTime = GetCurrentTimeInMicroSeconds();
            processor.Process(eventGroup);
            durationTime += GetCurrentTimeInMicroSeconds() - startTime;

            // std::string outJson = eventGroup.ToJsonString();
            // std::cout << "outJson: " << outJson << std::endl;
        }
        std::cout << "durationTime: " << durationTime << std::endl;
        std::cout << "process: "
                  << formatSize(serializedData.size() * (uint64_t)count * 1000000 * (uint64_t)size / durationTime)
                  << std::endl;
    }
}

static void createHttpSpan(models::SpanEvent* span) {
    span->set_traceid("cba78930fe0c2626bc60696a3453cc40");
    span->set_spanid("4083239a6a2e704e");
    span->set_parentspanid("d42788c106b9c48e");
    span->set_name("/components/api/v1/http/success");
    span->set_kind(models::SpanEvent::SERVER);
    span->set_starttime(1748313835253000000ULL);
    span->set_endtime(1748313840262969241ULL);
    span->set_status(models::SpanEvent::Unset);

    std::map<std::string, std::string> httpTags = {{"http.path", "/components/api/v1/http/success"},
                                                   {"endpoint", "mall-user-service:9190"},
                                                   {"http.method", "POST"},
                                                   {"component.name", "http"},
                                                   {"http.status_code", "200"},
                                                   {"http.route", "/components/api/v1/http/success"}};

    for (const auto& tag : httpTags) {
        (*span->mutable_tags())[tag.first] = tag.second;
    }

    std::map<std::string, std::string> httpScopeTags = {{"otel.scope.version", "1.28.0-alpha"}, 
                                                        {"otel.scope.name", "io.opentelemetry.tomcat-8.0.15"}};

    for (const auto& tag : httpScopeTags) {
        (*span->mutable_scopetags())[tag.first] = tag.second;
    }
}

static void createNosqlSpan(models::SpanEvent* span) {
    span->set_traceid("cba78930fe0c2626bc60696a3453cc40");
    span->set_spanid("9a2c1a8a371d6798");
    span->set_parentspanid("4083239a6a2e704e");
    span->set_name("LLEN");
    span->set_kind(models::SpanEvent::CLIENT);
    span->set_starttime(1748313840259486017ULL);
    span->set_endtime(1748313840259765375ULL);
    span->set_status(models::SpanEvent::Unset);

    std::map<std::string, std::string> nosqlTags = {{"db.system", "redis"},
                                                    {"endpoint", "redis:6379"},
                                                    {"component.name", "redis"},
                                                    {"db.name", "redis:6379"},
                                                    {"net.peer.name", "redis:6379"},
                                                    {"redis.args", "key<big_key>"},
                                                    {"db.statement.id", "2191aada7df3c872"}};

    for (const auto& tag : nosqlTags) {
        (*span->mutable_tags())[tag.first] = tag.second;
    }

    std::map<std::string, std::string> nosqlScopeTags = {{"otel.scope.version", "1.28.0-alpha"}, 
                                                         {"otel.scope.name", "io.opentelemetry.lettuce-5.1"}};

    for (const auto& tag : nosqlScopeTags) {
        (*span->mutable_scopetags())[tag.first] = tag.second;
    }
}

static void createGenaiSpan(models::SpanEvent* span) {
    span->set_traceid("cba78930fe0c2626bc60696a3453cc40");
    span->set_spanid("4083239a6a2e704e");
    span->set_parentspanid("d42788c106b9c48e");
    span->set_name("Chat Qwen");
    span->set_kind(models::SpanEvent::CLIENT);
    span->set_starttime(1748313835253000000ULL);
    span->set_endtime(1748313840262969241ULL);
    span->set_status(models::SpanEvent::Unset);

    std::map<std::string, std::string> genaiTags =
        {{"gen_ai.span.kind", "LLM"},
        {"gen_ai.operation.name", "chat"},
        {"gen_ai.system", "unknown"},
        {"gen_ai.request.parameters", "{ temperature: 0.7 }"},
        {"gen_ai.model_name", "qwen3-235B-A22B"},
        {"gen_ai.conversation.id", "conv_5j66UpCpwteGg4YSxUnt7lPY"},
        {"gen_ai.output.type", "text"},
        {"gen_ai.request.choice.count", "3"},
        {"gen_ai.request.model", "qwen3-235B-A22B"},
        {"gen_ai.request.seed", "qwen3-235B-A22B"},
        {"gen_ai.request.frequency_penalty", "0.1"},
        {"gen_ai.request.max_tokens", "100"},
        {"gen_ai.request.presence_penalty", "0.1"},
        {"gen_ai.request.temperature", "0.1"},
        {"gen_ai.request.top_p", "1.0"},
        {"gen_ai.request.top_k", "1.0"},
        {"gen_ai.request.is_stream", "false"},
        {"gen_ai.request.stop_sequences", "[\"stop\"]"},
        {"gen_ai.request.tool_calls", "[\"get_current_weather\"]"},
        {"gen_ai.response.id", "qwen3-235B-A22B"},
        {"gen_ai.response.model", "qwen3-235B-A22B"},
        {"gen_ai.response.finish_reason", "stop"},
        {"gen_ai.response.time_to_first_token", "1000000"},
        {"gen_ai.response.reasoning_time", "1248"},
        {"gen_ai.usage.input_tokens", "100"},
        {"gen_ai.usage.output_tokens", "200"},
        {"gen_ai.usage.total_tokens", "300"},
        {"gen_ai.input.messages",
            "[{\"role\": \"user\", \"parts\": [{\"type\": \"text\", \"content\": \"Please tell me 50 jokes about OpenTelemetry.\"}]}]"},
        {"gen_ai.output.messages",
            "[{\"role\":\"assistant\",\"parts\":[{\"type\":\"text\",\"content\":"
            "\"Here are 50 intentionally terrible (but technically accurate) OpenTelemetry jokes — perfect for that observability-themed "
            "stand-up night in your monitoring dashboard. (Disclaimer: These are so niche, only SREs and observability engineers will laugh... "
            "or cry.)"
            "🔍 The Jokes (Guaranteed to cause mild confusion in your traces)"
            "Why did the OpenTelemetry span feel lonely?"
            "Because it had zero parents... just like my weekend plans."
            ""
            "Why do OpenTelemetry developers hate coffee?"
            "Because they’re already instrumented for caffeine."
            ""
            "What’s an OpenTelemetry engineer’s favorite pickup line?"
            "“Are you a distributed trace? Because I’d love to follow you all the way to production.”"
            ""
            "Why did the metric name get rejected?"
            "Too many http.server.duration — it was overly specific and nobody wanted to aggregate it."
            ""
            "What do you call an OpenTelemetry exporter with commitment issues?"
            "A Jaeger who keeps span-ning."
            ""
            "Why did the developer get fired from the OTel meetup?"
            "He kept dropping spans... and not the cool kind."
            ""
            "What’s OpenTelemetry’s least favorite game?"
            "Hide and Seek — because all its spans are always accounted for."
            ""
            "Why did the OTel Collector panic?"
            "It realized it was just a middleman... like my manager."
            ""
            "What do you call a failed OTel deployment?"
            "Tracer than fiction."
            ""
            "Why don’t OTel spans ever lie?"
            "They’re always recorded."
            ""
            "What’s an observability engineer’s bedtime story?"
            "“Once upon a time, the metrics pipeline didn’t drop data…” (Spoiler: It’s a horror story.)"
            ""
            "Why did the developer name his dog “otelcol”?"
            "Because it collects everything... especially the blame."
            ""
            "What do you call an OTel span during a holiday?"
            "Uninstrumented. (It’s on break.)"
            ""
            "Why was the OTel spec so humble?"
            "It knew it was just one part of the observability pillar."
            ""
            "What’s OpenTelemetry’s favorite dance move?"
            "The Span Shuffle."
            ""
            "Why did the trace ID get kicked out of the bar?"
            "It was too long and nobody could correlate it."
            ""
            "What do you call OTel when it’s tired?"
            "Lazy-loaded instrumentation."
            ""
            "Why did the developer refuse to use logs?"
            "“I prefer structured suffering.” (Thanks, OTel!)"
            ""
            "What’s an SRE’s least favorite OTel resource attribute?"
            "service.version=dev in production."
            ""
            "Why did the OTel SDK break up with the legacy tracer?"
            "It needed vendor-neutral space."
            ""
            "What do you call a misconfigured OTel exporter?"
            "Silent but deadly. (Like a dropped trace.)"
            ""
            "Why did the metric name cause a fight?"
            "Too many http.server.duration vs. http.server.request.duration — naming is hard."
            ""
            "What’s OpenTelemetry’s spirit animal?"
            "The observability octopus — it touches all your services."
            ""
            "Why don’t OTel spans go to parties?"
            "They hate unparented contexts."
            ""
            "What did the developer say when OTel broke in prod?"
            "“Well, at least the traces are consistent.” (They weren’t.)"
            ""
            "Why was the OTel Collector promoted?"
            "It handled the pressure (until the queue filled up)."
            ""
            "What’s an OTel engineer’s favorite fruit?"
            "Span-gold melon."
            ""
            "Why did the trace fail the interview?"
            "No end-to-end experience."
            ""
            "What do you call OTel in a serverless environment?"
            "Cold-started and confused."
            ""
            "Why did the developer distrust the OTel demo?"
            "It ran on localhost."
            ""
            "What’s OpenTelemetry’s least favorite word?"
            "“Works on my machine.”"
            ""
            "Why did the span duration go viral?"
            "It was 99.9% latency... for 10 minutes."
            ""
            "What do you call an OTel pipeline at 3 AM?"
            "Critical. (And full of otelcol errors.)"
            ""
            "Why did the metric get a tattoo?"
            "To show its unit (spoiler: it was s)."
            ""
            "What’s an observability engineer’s love language?"
            "“I’ll fix your cardinality issue.”"
            ""
            "Why was the OTel spec late to the meeting?"
            "Context propagation delays."
            ""
            "What do you call a successful OTel rollout?"
            "Myth. (Just kidding... mostly.)"
            ""
            "Why did the developer use otel.WithSpan?"
            "To wrap his existential dread."
            ""
            "What’s OpenTelemetry’s favorite movie?"
            "The Tracey Fragments."
            ""
            "Why don’t OTel spans trust the network?"
            "Too many unreliable hops."
            ""
            "What did the SRE say to the broken dashboard?"
            "“You’re not my trace.”"
            ""
            "Why was the OTel exporter feeling insecure?"
            "It kept getting rejected by the backend."
            ""
            "What’s an OTel engineer’s favorite exercise?"
            "Span-durance training."
            ""
            "Why did the trace get a Nobel Prize?"
            "It achieved perfect end-to-end visibility... in staging."
            ""
            "What do you call OTel during a outage?"
            "Silently judging you."
            ""
            "Why did the developer write custom metrics?"
            "Because http.server.duration wasn’t special enough."
            ""
            "What’s OpenTelemetry’s favorite band?"
            "The Span-ners."
            ""
            "Why did the OTel meetup end early?"
            "Everyone realized they still had uninstrumented services."
            ""
            "What do you call vendor lock-in with OpenTelemetry?"
            "Impossible. (Wait... is it?)"
            ""
            "Why did the observability engineer bring a ladder to work?"
            "To reach the top of the cardinality cliff."
            ""
            "💡 Pro Tip for Survival"
            "If your traces are clean but your code isn’t, you’re doing OpenTelemetry wrong."
            "— Ancient SRE Proverb (probably)"
            ""
            "Remember: In observability, the only thing more painful than no traces... is too many traces. 😂"
            "(Share these at your next incident review — they’ll either laugh or fire you. Win-win!)\"}],"
            "\"finish_reason\":\"stop\"}]"},
        {"gen_ai.system.instructions",
            "{\"role\": \"system\", \"message\": {\"type\": \"text\", \"content\": \"You are a helpful assistant\"}}"},
        {"gen_ai.response.reasoning_content",
            "{\"role\": \"assistant\", \"message\": {\"type\": \"text\", \"content\": "
            "\"Okay, the user asked for 50 jokes about OpenTelemetry. That's... a lot. And very specific. Hmm."
            "First, let's unpack why someone would ask this. Maybe they're a developer drowning in observability setup, "
            "trying to lighten the mood. Or a DevOps engineer who just spent 3 days debugging metric names. Or... "
            "could this be a trap? Like, someone testing if I'll generate nonsense? checks query again Nah, seems genuine. "
            "But 50 is excessive—no one actually needs 50 OTel jokes. They probably want some laughs but are being hyperbolic."
            "scratches head The real challenge: OTel is dry tech. How do you joke about distributed tracing? "
            "sigh Gotta lean into the pain points—boilerplate code, confusing exporters, \"spans\" that feel endless. "
            "Developers will get it. Wait—should I warn them these are bad jokes? Yeah, better set expectations. "
            "\"These are terrible\" disclaimer upfront. Also, must avoid actual misinformation. Can't joke about "
            "\"OTel deleting prod\" if it's not true. checks OTel docs mentally Nope, safe. starts drafting First joke: "
            "\"Why did the span feel lonely?\" Classic anthropomorphism. Second: \"OTel vs. coffee\" because every dev "
            "runs on caffeine. Third: \"metric name collisions\" — ugh, real pain. pauses Is \"otelcol\" too niche? "
            "Nah, if they know OTel, they've cursed that binary. counts halfway 25 jokes in... running out of steam. "
            "panics slightly Okay, recycle themes: more span puns, more \"why did the developer...\" formats. "
            "\"OTel in production\" jokes write themselves—everyone's been there. at 48 Two left. desperate ...Ah! "
            "\"OTel meetup\" joke for the conference crowd. And \"vendor lock-in\" for the cynical ones. Done. collapses "
            "Final check: All jokes technically accurate? Yep. No harmful stereotypes? Skipped the \"lazy developer\" trope. "
            "Added disclaimers? Triple-checked. User probably just wanted 5, but hey—they asked for 50. shrug Deliver "
            "what's promised, even if it's painful. Like debugging without logs.\"}}"},
    };

    for (const auto& tag : genaiTags) {
        (*span->mutable_tags())[tag.first] = tag.second;
    }

    std::map<std::string, std::string> genaiScopeTags = {{"otel.scope.version", "1.28.0-alpha"}, 
                                                        {"otel.scope.name", "io.opentelemetry.openai"}};

    for (const auto& tag : genaiScopeTags) {
        (*span->mutable_scopetags())[tag.first] = tag.second;
    }
}

static void BM_ParseFromPBBasicSpanData(int size, int batchSize) {
    logtail::models::PipelineEventGroup pbEventGroup;
    
    models::SpanEvent httpSpan;
    createHttpSpan(&httpSpan);

    models::SpanEvent nosqlSpan;
    createNosqlSpan(&nosqlSpan);

    auto* spanEvents = pbEventGroup.mutable_spans();
    // 16 spans for each event group - baseline
    for (int i = 0; i < 8; i++) {
        *spanEvents->add_events() = httpSpan;
        *spanEvents->add_events() = nosqlSpan;
    }

    runBenchmark(size, batchSize, pbEventGroup.SerializeAsString());
}

static void BM_ParseFromPBMaxBatchSizeSpanData(int size, int batchSize) {
    logtail::models::PipelineEventGroup pbEventGroup;
    
    models::SpanEvent httpSpan;
    createHttpSpan(&httpSpan);

    models::SpanEvent nosqlSpan;
    createNosqlSpan(&nosqlSpan);

    auto* spanEvents = pbEventGroup.mutable_spans();
    // 512 spans for max batch size of event group
    for (int i = 0; i < 256; i++) {
        *spanEvents->add_events() = httpSpan;
        *spanEvents->add_events() = nosqlSpan;
    }

    runBenchmark(size, batchSize, pbEventGroup.SerializeAsString());
}

static void BM_ParseFromPBLargeSingleSpanData(int size, int batchSize) {
    logtail::models::PipelineEventGroup pbEventGroup;
    
    models::SpanEvent genaiSpan;
    createGenaiSpan(&genaiSpan);

    auto* spanEvents = pbEventGroup.mutable_spans();
    *spanEvents->add_events() = genaiSpan;

    runBenchmark(size, batchSize, pbEventGroup.SerializeAsString());
}

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
#ifdef NDEBUG
    std::cout << "release" << std::endl;
#else
    std::cout << "debug" << std::endl;
#endif
    std::cout << "parse from protobuf basic span data (baseline)" << std::endl;
    BM_ParseFromPBBasicSpanData(512, 100);
    std::cout << "parse from protobuf max batch size span data" << std::endl;
    BM_ParseFromPBMaxBatchSizeSpanData(512, 100);
    std::cout << "parse from protobuf large single span data" << std::endl;
    BM_ParseFromPBLargeSingleSpanData(512, 100);
    return 0;
}
