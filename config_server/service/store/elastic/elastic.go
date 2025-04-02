package elastic

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"reflect"
	"strings"
	"time"

	"github.com/elastic/go-elasticsearch/v7"
	"github.com/elastic/go-elasticsearch/v7/esapi"

	"config-server/common"
	"config-server/model"
	"config-server/setting"
	database "config-server/store/interface_database"
)

const (
	MaxQueryBatchSize = 1000
	ScrollTime        = time.Second * 30

	ILMPolicyName = "ilogtail-heartbeat-policy"
	IndexTempName = "ilogtail-heartbeat-template"

	// DeletePhasesMinAge delete index after 1d
	DeletePhasesMinAge = "1d"

	// HeartbeatTimeRange if there is no heartbeat within five minutes, the service is considered offline
	HeartbeatTimeRange = "now-5m"
)

var NotFoundError = errors.New("404 not found")

type Store struct {
	es *elasticsearch.Client
}

func (s *Store) Connect() error {
	cfg := elasticsearch.Config{
		Addresses: setting.GetSetting().ESConfig.Addresses,
		Username:  setting.GetSetting().ESConfig.Username,
		Password:  setting.GetSetting().ESConfig.Password,
	}

	client, err := elasticsearch.NewClient(cfg)
	if err != nil {
		return err
	}
	s.es = client

	return s.Prepare()
}

func (s *Store) GetMode() string {
	return "elastic"
}

func (s *Store) Close() error {
	return nil
}

func (s *Store) Get(index string, entityKey string) (interface{}, error) {
	var doc map[string]interface{}
	modelPtr, ok := model.Models[index]
	if !ok {
		return nil, fmt.Errorf("unsupported index: %s", index)
	}

	ans := reflect.New(reflect.TypeOf(modelPtr).Elem()).Interface()

	if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Get(index, entityKey, s.es.Get.WithContext(ctx))
	}, &doc); err != nil {
		return nil, err
	}

	source, ok := doc["_source"].(map[string]interface{})
	if !ok {
		return nil, fmt.Errorf("_source field missing or not of expected type")
	}

	sourceBytes, err := json.Marshal(source)
	if err != nil {
		return nil, err
	}

	if err := json.Unmarshal(sourceBytes, ans); err != nil {
		return nil, err
	}

	return ans, nil
}

func (s *Store) Add(index string, entityKey string, entity interface{}) error {
	value, err := json.Marshal(entity)
	if err != nil {
		return fmt.Errorf("json.Marshal error: %s", err)
	}

	return s.performRequest(
		context.TODO(),
		index,
		func(ctx context.Context, index string) (*esapi.Response, error) {
			return s.es.Index(index, bytes.NewReader(value), s.es.Index.WithDocumentID(entityKey),
				s.es.Index.WithContext(ctx))
		},
		nil,
	)
}

func (s *Store) Update(index string, entityKey string, entity interface{}) error {
	return s.Add(index, entityKey, entity)
}

func (s *Store) Has(index string, entityKey string) (bool, error) {
	if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Exists(index, entityKey, s.es.Exists.WithContext(ctx))
	}, nil); err != nil {
		if errors.Is(err, NotFoundError) {
			return false, nil
		}
		return false, err
	}
	return true, nil
}

func (s *Store) Delete(index string, entityKey string) error {
	return s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Delete(index, entityKey, s.es.Delete.WithContext(ctx))
	}, nil)
}

func (s *Store) GetAll(index string) ([]interface{}, error) {
	var (
		data []interface{}
		err  error
	)
	switch {
	case index == common.TypeAgent:
		data, err = s.getAllHeartbeat(index, data)
	default:
		data, err = s.getAllOthers(index, data)
	}
	return data, err
}

func (s *Store) getAllHeartbeat(index string, data []interface{}) ([]interface{}, error) {
	query := HeartbeatQuery{
		Query: Query{
			BoolQuery: BoolQuery{
				Filter: []map[string]interface{}{
					{
						"range": map[string]interface{}{
							"Timestamp": map[string]interface{}{
								"gte": HeartbeatTimeRange,
							},
						},
					},
				},
			},
		},
		Aggs: Aggregations{
			Agents: Agents{Terms: Terms{
				Field: "AgentID.keyword",
				Size:  MaxQueryBatchSize,
			},
				Aggs: map[string]LatestHeartbeat{
					"latest_heartbeat": {
						TopHits: map[string]interface{}{
							"sort": []map[string]interface{}{
								{
									"Timestamp": map[string]interface{}{
										"order": "desc",
									},
								},
							},
							"size": 1,
						},
					},
				}},
		},
	}

	value, err := json.Marshal(query)
	if err != nil {
		return nil, fmt.Errorf("json.Marshal error: %s", err)
	}

	var hit struct {
		Aggregations struct {
			Agents struct {
				Buckets []struct {
					LatestHeartbeat struct {
						Hits struct {
							Hits []struct {
								Source model.Agent `json:"_source"`
							} `json:"hits"`
						} `json:"hits"`
					} `json:"latest_heartbeat"`
				} `json:"buckets"`
			} `json:"agents"`
		} `json:"aggregations"`
	}

	if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Search(s.es.Search.WithIndex(index),
			s.es.Search.WithSize(0),
			s.es.Search.WithBody(bytes.NewReader(value)))
	}, &hit); err != nil {
		return nil, err
	}

	for _, bucket := range hit.Aggregations.Agents.Buckets {
		data = append(data, &bucket.LatestHeartbeat.Hits.Hits[0].Source)
	}
	return data, nil
}

func (s *Store) getAllOthers(index string, data []interface{}) ([]interface{}, error) {
	var r map[string]interface{}

	modelType, ok := model.ModelTypes[index]
	if !ok {
		return nil, fmt.Errorf("unsupported index: %s", index)
	}

	results := reflect.MakeSlice(modelType, 0, 0)

	if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Search(s.es.Search.WithIndex(index),
			s.es.Search.WithSize(MaxQueryBatchSize),
			s.es.Search.WithScroll(ScrollTime),
			s.es.Search.WithContext(ctx))
	}, &r); err != nil {
		return nil, err
	}

	scrollID, ok := r["_scroll_id"].(string)
	if !ok {
		return nil, fmt.Errorf("missing or invalid _scroll_id")
	}

	hits, ok := r["hits"].(map[string]interface{})["hits"].([]interface{})
	if !ok {
		return nil, fmt.Errorf("missing or invalid hits")
	}

	elemType := modelType.Elem()

	for len(hits) > 0 {
		for _, hit := range hits {
			source, ok := hit.(map[string]interface{})["_source"].(map[string]interface{})
			if !ok {
				return nil, fmt.Errorf("invalid _source field in hit")
			}

			result := reflect.New(elemType).Interface()

			sourceBytes, err := json.Marshal(source)
			if err != nil {
				return nil, err
			}

			if err := json.Unmarshal(sourceBytes, result); err != nil {
				return nil, err
			}

			results = reflect.Append(results, reflect.ValueOf(result).Elem())
		}

		if len(hits) < MaxQueryBatchSize {
			break
		}

		if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
			return s.es.Scroll(s.es.Scroll.WithScrollID(scrollID),
				s.es.Scroll.WithScroll(ScrollTime),
				s.es.Scroll.WithContext(ctx))
		}, &r); err != nil {
			return nil, err
		}

		scrollID, ok = r["_scroll_id"].(string)
		if !ok {
			return nil, fmt.Errorf("missing or invalid _scroll_id")
		}

		hits, ok = r["hits"].(map[string]interface{})["hits"].([]interface{})
		if !ok {
			return nil, fmt.Errorf("missing or invalid hits")
		}
	}

	// Clear up scroll context
	if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.ClearScroll(s.es.ClearScroll.WithScrollID(scrollID))
	}, nil); err != nil {
		return nil, err
	}

	for i := 0; i < results.Len(); i++ {
		data = append(data, results.Index(i).Interface())
	}

	return data, nil
}

func (s *Store) Count(index string) (int, error) {
	var count map[string]interface{}
	if err := s.performRequest(context.TODO(), index, func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Count(s.es.Count.WithIndex(index),
			s.es.Count.WithContext(ctx))
	}, &count); err != nil {
		return 0, err
	}
	return int(count["count"].(float64)), nil
}

func (s *Store) WriteBatch(batch *database.Batch) error {
	for !batch.Empty() {
		data := batch.Pop()

		switch data.Opt {
		case database.OptDelete:
			if data.Table != common.TypeAgent {
				if err := s.Delete(data.Table, data.Key); err != nil {
					return err
				}
			}
		case database.OptAdd, database.OptUpdate:
			value, err := json.Marshal(data.Value)
			if err != nil {
				return fmt.Errorf("json.Marshal error: %s", err)
			}

			if data.Table == common.TypeAgent {
				err = s.performRequest(context.TODO(), data.Table, func(ctx context.Context, index string) (*esapi.Response, error) {
					return s.es.Index(index+"-"+time.Now().UTC().Format("2006-01-02-15-04"), bytes.NewReader(value))
				}, nil)
			} else {
				if data.Opt == database.OptAdd {
					err = s.Add(data.Table, data.Key, value)
				} else {
					err = s.Update(data.Table, data.Key, value)
				}
			}

			if err != nil {
				return err
			}
		}
	}
	return nil
}

func (s *Store) Prepare() error {
	existsPolicy := s.existsILMPolicy()
	if !existsPolicy {
		if err := s.createILMPolicy(); err != nil {
			return err
		}
	}

	existsIndexTemp := s.existsIndexTemp()
	if !existsIndexTemp {
		if err := s.createIndexTemp(); err != nil {
			return err
		}
	}
	return nil
}

func (s *Store) createILMPolicy() error {
	ilmPolicy := ILMPolicy{
		Policy: struct {
			Phases map[string]struct {
				MinAge  string `json:"min_age,omitempty"`
				Actions struct {
					Delete struct{} `json:"delete,omitempty"`
				} `json:"actions,omitempty"`
			} `json:"phases"`
		}{
			Phases: map[string]struct {
				MinAge  string `json:"min_age,omitempty"`
				Actions struct {
					Delete struct{} `json:"delete,omitempty"`
				} `json:"actions,omitempty"`
			}{
				"delete": {
					MinAge: DeletePhasesMinAge,
					Actions: struct {
						Delete struct{} `json:"delete,omitempty"`
					}{
						Delete: struct{}{},
					},
				},
			},
		},
	}

	value, err := json.Marshal(ilmPolicy)
	if err != nil {
		return fmt.Errorf("json.Marshal error: %s", err)
	}

	return s.performRequest(context.TODO(), "", func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.ILM.PutLifecycle(ILMPolicyName,
			s.es.ILM.PutLifecycle.WithBody(bytes.NewReader(value)),
			s.es.ILM.PutLifecycle.WithContext(ctx))
	}, nil)
}

func (s *Store) existsILMPolicy() bool {
	if err := s.performRequest(context.TODO(), "", func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.ILM.GetLifecycle(s.es.ILM.GetLifecycle.WithPolicy(ILMPolicyName),
			s.es.ILM.GetLifecycle.WithContext(ctx),
		)
	}, nil); err != nil {
		return false
	}
	return true
}

func (s *Store) createIndexTemp() error {
	temp := IndexTemp{
		IndexPatterns: []string{"agent-*"},
		Settings: map[string]interface{}{
			"index": map[string]interface{}{
				"lifecycle": map[string]string{
					"name": ILMPolicyName,
				},
			},
		},
		Aliases: map[string]interface{}{
			"agent": struct{}{},
		},
	}

	value, err := json.Marshal(temp)
	if err != nil {
		return fmt.Errorf("json.Marshal error: %s", err)
	}

	return s.performRequest(context.TODO(), "", func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Indices.PutTemplate(IndexTempName, bytes.NewReader(value),
			s.es.Indices.PutTemplate.WithContext(ctx))
	}, nil)
}

func (s *Store) existsIndexTemp() bool {
	if err := s.performRequest(context.TODO(), "", func(ctx context.Context, index string) (*esapi.Response, error) {
		return s.es.Indices.ExistsIndexTemplate(IndexTempName,
			s.es.Indices.ExistsIndexTemplate.WithContext(ctx))
	}, nil); err != nil {
		return false
	}
	return true
}

// performRequest abstracts the Elasticsearch request execution process.
func (s *Store) performRequest(ctx context.Context, index string, apiFunc func(ctx context.Context, index string) (*esapi.Response, error), ret interface{}) error {
	lower := changeToLower(index)

	resp, err := apiFunc(ctx, lower)
	if err != nil {
		return err
	}

	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)

	if !resp.IsError() {
		if ret == nil {
			// no body to read out
			return nil
		}
		if err := json.Unmarshal(body, ret); err != nil {
			// We received a success response from the ES API but the body was in an unexpected format.
			return fmt.Errorf("unexpected format from elasticsearch api: %s; %d", err, resp.StatusCode)
		}

		// body has been successfully read out
		return nil
	}

	if resp.StatusCode == http.StatusNotFound {
		return NotFoundError
	}

	// We received some sort of API error. Let's return it.
	return fmt.Errorf("error returned from elasticsearch api: %d", resp.StatusCode)
}

// Elasticsearch indexes don't support uppercase, so we'll need to change it.
func changeToLower(index string) string {
	lower := strings.ToLower(index)
	return lower
}
