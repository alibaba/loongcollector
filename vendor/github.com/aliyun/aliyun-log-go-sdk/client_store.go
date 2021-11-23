package sls

import (
	base64E "encoding/base64"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/http/httputil"
	"net/url"
	"strconv"
	"time"

	"github.com/go-kit/kit/log/level"
)

func convertLogstore(c *Client, project, logstore string) *LogStore {
	c.accessKeyLock.RLock()
	proj := convert(c, project)
	c.accessKeyLock.RUnlock()
	return &LogStore{
		project: proj,
		Name:    logstore,
	}
}

// ListShards returns shard id list of this logstore.
func (c *Client) ListShards(project, logstore string) (shardIDs []*Shard, err error) {
	ls := convertLogstore(c, project, logstore)
	return ls.ListShards()
}

// SplitShard https://help.aliyun.com/document_detail/29021.html
func (c *Client) SplitShard(project, logstore string, shardID int, splitKey string) (shards []*Shard, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}

	urlVal := url.Values{}
	urlVal.Add("action", "split")
	urlVal.Add("key", splitKey)
	uri := fmt.Sprintf("/logstores/%v/shards/%v?%v", logstore, shardID, urlVal.Encode())
	r, err := c.request(project, "POST", uri, h, nil)
	if err != nil {
		return
	}
	defer r.Body.Close()
	buf, err := ioutil.ReadAll(r.Body)
	if err != nil {
		return nil, NewClientError(err)
	}
	err = json.Unmarshal(buf, &shards)
	return
}

// MergeShards https://help.aliyun.com/document_detail/29022.html
func (c *Client) MergeShards(project, logstore string, shardID int) (shards []*Shard, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}

	urlVal := url.Values{}
	urlVal.Add("action", "merge")
	uri := fmt.Sprintf("/logstores/%v/shards/%v?%v", logstore, shardID, urlVal.Encode())
	r, err := c.request(project, "POST", uri, h, nil)
	if err != nil {
		return
	}
	defer r.Body.Close()
	buf, err := ioutil.ReadAll(r.Body)
	if err != nil {
		return nil, NewClientError(err)
	}
	err = json.Unmarshal(buf, &shards)
	return
}

// PutLogs put logs into logstore.
// The callers should transform user logs into LogGroup.
func (c *Client) PutLogs(project, logstore string, lg *LogGroup) (err error) {
	ls := convertLogstore(c, project, logstore)
	return ls.PutLogs(lg)
}

// PostLogStoreLogs put logs into Shard logstore by hashKey.
// The callers should transform user logs into LogGroup.
func (c *Client) PostLogStoreLogs(project, logstore string, lg *LogGroup, hashKey *string) (err error) {
	ls := convertLogstore(c, project, logstore)
	return ls.PostLogStoreLogs(lg, hashKey)
}

// PutLogsWithCompressType put logs into logstore with specific compress type.
// The callers should transform user logs into LogGroup.
func (c *Client) PutLogsWithCompressType(project, logstore string, lg *LogGroup, compressType int) (err error) {
	ls := convertLogstore(c, project, logstore)
	if err := ls.SetPutLogCompressType(compressType); err != nil {
		return err
	}
	return ls.PutLogs(lg)
}

// PutRawLogWithCompressType put raw log data to log service, no marshal
func (c *Client) PutRawLogWithCompressType(project, logstore string, rawLogData []byte, compressType int) (err error) {
	ls := convertLogstore(c, project, logstore)
	if err := ls.SetPutLogCompressType(compressType); err != nil {
		return err
	}
	return ls.PutRawLog(rawLogData)
}

// GetCursor gets log cursor of one shard specified by shardId.
// The from can be in three form: a) unix timestamp in seccond, b) "begin", c) "end".
// For more detail please read: https://help.aliyun.com/document_detail/29024.html
func (c *Client) GetCursor(project, logstore string, shardID int, from string) (cursor string, err error) {
	ls := convertLogstore(c, project, logstore)
	return ls.GetCursor(shardID, from)
}

// GetCursorTime ...
func (c *Client) GetCursorTime(project, logstore string, shardID int, cursor string) (cursorTime time.Time, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}

	urlVal := url.Values{}
	urlVal.Add("cursor", cursor)
	urlVal.Add("type", "cursor_time")
	uri := fmt.Sprintf("/logstores/%v/shards/%v?%v", logstore, shardID, urlVal.Encode())
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return
	}
	defer r.Body.Close()
	buf, err := ioutil.ReadAll(r.Body)
	if err != nil {
		return cursorTime, NewClientError(err)
	}
	type getCursorResult struct {
		CursorTime int `json:"cursor_time"`
	}
	var rst getCursorResult
	err = json.Unmarshal(buf, &rst)
	return time.Unix(int64(rst.CursorTime), 0), err
}

// GetPrevCursorTime ...
func (c *Client) GetPrevCursorTime(project, logstore string, shardID int, cursor string) (cursorTime time.Time, err error) {
	realCursor, err := base64E.StdEncoding.DecodeString(cursor)
	if err != nil {
		return cursorTime, NewClientError(err)
	}
	cursorVal, err := strconv.Atoi(string(realCursor))
	if err != nil {
		return cursorTime, NewClientError(err)
	}
	cursorVal--
	nextCursor := base64E.StdEncoding.EncodeToString([]byte(strconv.Itoa(cursorVal)))
	return c.GetCursorTime(project, logstore, shardID, nextCursor)
}

// GetLogsBytes gets logs binary data from shard specified by shardId according cursor and endCursor.
// The logGroupMaxCount is the max number of logGroup could be returned.
// The nextCursor is the next curosr can be used to read logs at next time.
func (c *Client) GetLogsBytes(project, logstore string, shardID int, cursor, endCursor string,
	logGroupMaxCount int) (out []byte, nextCursor string, err error) {
	ls := convertLogstore(c, project, logstore)
	return ls.GetLogsBytes(shardID, cursor, endCursor, logGroupMaxCount)
}

// PullLogs gets logs from shard specified by shardId according cursor and endCursor.
// The logGroupMaxCount is the max number of logGroup could be returned.
// The nextCursor is the next cursor can be used to read logs at next time.
// @note if you want to pull logs continuous, set endCursor = ""
func (c *Client) PullLogs(project, logstore string, shardID int, cursor, endCursor string,
	logGroupMaxCount int) (gl *LogGroupList, nextCursor string, err error) {
	ls := convertLogstore(c, project, logstore)
	return ls.PullLogs(shardID, cursor, endCursor, logGroupMaxCount)
}

// GetHistograms query logs with [from, to) time range
func (c *Client) GetHistograms(project, logstore string, topic string, from int64, to int64, queryExp string) (*GetHistogramsResponse, error) {
	ls := convertLogstore(c, project, logstore)
	return ls.GetHistograms(topic, from, to, queryExp)
}

// GetLogs query logs with [from, to) time range
func (c *Client) GetLogs(project, logstore string, topic string, from int64, to int64, queryExp string,
	maxLineNum int64, offset int64, reverse bool) (*GetLogsResponse, error) {
	ls := convertLogstore(c, project, logstore)
	return ls.GetLogs(topic, from, to, queryExp, maxLineNum, offset, reverse)
}

// CreateIndex ...
func (c *Client) CreateIndex(project, logstore string, index Index) error {
	ls := convertLogstore(c, project, logstore)
	return ls.CreateIndex(index)
}

// UpdateIndex ...
func (c *Client) UpdateIndex(project, logstore string, index Index) error {
	ls := convertLogstore(c, project, logstore)
	return ls.UpdateIndex(index)
}

// GetIndex ...
func (c *Client) GetIndex(project, logstore string) (*Index, error) {
	ls := convertLogstore(c, project, logstore)
	return ls.GetIndex()
}

// CreateIndexString ...
func (c *Client) CreateIndexString(project, logstore string, index string) error {
	ls := convertLogstore(c, project, logstore)
	return ls.CreateIndexString(index)
}

// UpdateIndexString ...
func (c *Client) UpdateIndexString(project, logstore string, index string) error {
	ls := convertLogstore(c, project, logstore)
	return ls.UpdateIndexString(index)
}

// GetIndexString ...
func (c *Client) GetIndexString(project, logstore string) (string, error) {
	ls := convertLogstore(c, project, logstore)
	return ls.GetIndexString()
}

// DeleteIndex ...
func (c *Client) DeleteIndex(project, logstore string) error {
	ls := convertLogstore(c, project, logstore)
	return ls.DeleteIndex()
}

// ListSubStore ...
func (c *Client) ListSubStore(project, logstore string) (sortedSubStores []string, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}

	uri := fmt.Sprintf("/logstores/%v/substores", logstore)
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return
	}
	defer r.Body.Close()
	buf, err := ioutil.ReadAll(r.Body)
	if err != nil {
		return
	}

	if r.StatusCode != http.StatusOK {
		errMsg := &Error{}
		err = json.Unmarshal(buf, errMsg)
		if err != nil {
			err = fmt.Errorf("failed to remove config from machine group")
			if IsDebugLevelMatched(1) {
				dump, _ := httputil.DumpResponse(r, true)
				level.Error(Logger).Log("msg", string(dump))
			}
			return
		}
		err = fmt.Errorf("%v:%v", errMsg.Code, errMsg.Message)
		return
	}

	type sortedSubStoreList struct {
		SubStores []string `json:"substores"`
	}

	body := &sortedSubStoreList{}
	err = json.Unmarshal(buf, body)
	if err != nil {
		return
	}

	sortedSubStores = body.SubStores
	return
}

// GetSubStore ...
func (c *Client) GetSubStore(project, logstore, name string) (sortedSubStore *SubStore, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}

	uri := fmt.Sprintf("/logstores/%s/substores/%s", logstore, name)
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return
	}
	defer r.Body.Close()
	buf, err := ioutil.ReadAll(r.Body)
	if err != nil {
		return
	}

	if r.StatusCode != http.StatusOK {
		errMsg := &Error{}
		err = json.Unmarshal(buf, errMsg)
		if err != nil {
			err = fmt.Errorf("failed to remove config from machine group")
			if IsDebugLevelMatched(1) {
				dump, _ := httputil.DumpResponse(r, true)
				level.Error(Logger).Log("msg", string(dump))
			}
			return
		}
		err = fmt.Errorf("%v:%v", errMsg.Code, errMsg.Message)
		return
	}
	sortedSubStore = &SubStore{}
	err = json.Unmarshal(buf, sortedSubStore)
	if err != nil {
		sortedSubStore = nil
		return
	}
	return
}

// CreateSubStore ...
func (c *Client) CreateSubStore(project, logstore string, sss *SubStore) (err error) {
	body, err := json.Marshal(sss)
	if err != nil {
		return NewClientError(err)
	}

	h := map[string]string{
		"x-log-bodyrawsize": fmt.Sprintf("%v", len(body)),
		"Content-Type":      "application/json",
		"Accept-Encoding":   "deflate",
	}
	r, err := c.request(project, "POST", fmt.Sprintf("/logstores/%s/substores", logstore), h, body)
	if err != nil {
		return NewClientError(err)
	}
	defer r.Body.Close()
	body, err = ioutil.ReadAll(r.Body)
	if r.StatusCode != http.StatusOK {
		err := new(Error)
		json.Unmarshal(body, err)
		return err
	}
	return
}

// UpdateSubStore ...
func (c *Client) UpdateSubStore(project, logstore string, sss *SubStore) (err error) {
	body, err := json.Marshal(sss)
	if err != nil {
		return NewClientError(err)
	}

	h := map[string]string{
		"x-log-bodyrawsize": fmt.Sprintf("%v", len(body)),
		"Content-Type":      "application/json",
		"Accept-Encoding":   "deflate",
	}
	r, err := c.request(project, "PUT", fmt.Sprintf("/logstores/%s/substores/%s", logstore, sss.Name), h, body)
	if err != nil {
		return NewClientError(err)
	}
	defer r.Body.Close()
	body, err = ioutil.ReadAll(r.Body)
	if r.StatusCode != http.StatusOK {
		err := new(Error)
		json.Unmarshal(body, err)
		return err
	}
	return
}

// DeleteSubStore ...
func (c *Client) DeleteSubStore(project, logstore string, name string) (err error) {

	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}
	r, err := c.request(project, "DELETE", fmt.Sprintf("/logstores/%s/substores/%s", logstore, name), h, nil)
	if err != nil {
		return NewClientError(err)
	}
	defer r.Body.Close()
	body, err := ioutil.ReadAll(r.Body)
	if r.StatusCode != http.StatusOK {
		err := new(Error)
		json.Unmarshal(body, err)
		return err
	}
	return
}

// GetSubStoreTTL ...
func (c *Client) GetSubStoreTTL(project, logstore string) (ttl int, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}

	uri := fmt.Sprintf("/logstores/%s/substores/storage/ttl", logstore)
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return
	}
	defer r.Body.Close()
	buf, err := ioutil.ReadAll(r.Body)
	if err != nil {
		return
	}

	if r.StatusCode != http.StatusOK {
		errMsg := &Error{}
		err = json.Unmarshal(buf, errMsg)
		if err != nil {
			err = fmt.Errorf("failed to remove config from machine group")
			if IsDebugLevelMatched(1) {
				dump, _ := httputil.DumpResponse(r, true)
				level.Error(Logger).Log("msg", string(dump))
			}
			return
		}
		err = fmt.Errorf("%v:%v", errMsg.Code, errMsg.Message)
		return
	}

	type ttlDef struct {
		TTL int `json:"ttl"`
	}

	var ttlIns ttlDef
	err = json.Unmarshal(buf, &ttlIns)
	if err != nil {
		return
	}
	return ttlIns.TTL, err
}

// UpdateSubStoreTTL ...
func (c *Client) UpdateSubStoreTTL(project, logstore string, ttl int) (err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
	}
	r, err := c.request(project, "PUT", fmt.Sprintf("/logstores/%s/substores/storage/ttl?ttl=%d", logstore, ttl), h, nil)
	if err != nil {
		return NewClientError(err)
	}
	defer r.Body.Close()
	body, err := ioutil.ReadAll(r.Body)
	if r.StatusCode != http.StatusOK {
		err := new(Error)
		json.Unmarshal(body, err)
		return err
	}
	return
}
