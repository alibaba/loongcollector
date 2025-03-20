package elastic

type ILMPolicy struct {
	Policy struct {
		Phases map[string]struct {
			MinAge  string `json:"min_age,omitempty"`
			Actions struct {
				Delete struct{} `json:"delete,omitempty"`
			} `json:"actions,omitempty"`
		} `json:"phases"`
	} `json:"policy"`
}

type IndexTemp struct {
	Order         int                    `json:"order"`
	IndexPatterns []string               `json:"index_patterns"`
	Settings      map[string]interface{} `json:"settings"`
	Aliases       map[string]interface{} `json:"aliases"`
}

type BoolQuery struct {
	Filter []map[string]interface{} `json:"filter"`
}

type Query struct {
	BoolQuery BoolQuery `json:"bool"`
}

type LatestHeartbeat struct {
	TopHits map[string]interface{} `json:"top_hits"`
}

type Terms struct {
	Field string `json:"field"`
	Size  int    `json:"size"`
}

type Agents struct {
	Terms Terms                      `json:"terms"`
	Aggs  map[string]LatestHeartbeat `json:"aggs"`
}

type Aggregations struct {
	Agents Agents `json:"agents"`
}

type HeartbeatQuery struct {
	Query Query        `json:"query"`
	Aggs  Aggregations `json:"aggs"`
}
