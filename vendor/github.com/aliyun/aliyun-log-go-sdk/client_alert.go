package sls

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"net/url"
	"strconv"
)

// SavedSearch ...
type SavedSearch struct {
	SavedSearchName string `json:"savedsearchName"`
	SearchQuery     string `json:"searchQuery"`
	Logstore        string `json:"logstore"`
	Topic           string `json:"topic"`
	DisplayName     string `json:"displayName"`
}

const (
	NotificationTypeSMS           = "SMS"
	NotificationTypeWebhook       = "Webhook"
	NotificationTypeDingTalk      = "DingTalk"
	NotificationTypeEmail         = "Email"
	NotificationTypeMessageCenter = "MessageCenter"
)

type Alert struct {
	Name             string              `json:"name"`
	DisplayName      string              `json:"displayName"`
	Description      string              `json:"description"`
	State            string              `json:"state"`
	Configuration    *AlertConfiguration `json:"configuration"`
	Schedule         *Schedule           `json:"schedule"`
	CreateTime       int64               `json:"createTime,omitempty"`
	LastModifiedTime int64               `json:"lastModifiedTime,omitempty"`
}

func (alert *Alert) MarshalJSON() ([]byte, error) {
	body := map[string]interface{}{
		"name":          alert.Name,
		"displayName":   alert.DisplayName,
		"description":   alert.Description,
		"state":         alert.State,
		"configuration": alert.Configuration,
		"schedule":      alert.Schedule,
		"type":          "Alert",
	}
	return json.Marshal(body)
}

type AlertConfiguration struct {
	Condition        string          `json:"condition"`
	Dashboard        string          `json:"dashboard"`
	QueryList        []*AlertQuery   `json:"queryList"`
	MuteUntil        int64           `json:"muteUntil"`
	NotificationList []*Notification `json:"notificationList"`
	NotifyThreshold  int32           `json:"notifyThreshold"`
	Throttling       string          `json:"throttling"`
}

type AlertQuery struct {
	ChartTitle   string `json:"chartTitle"`
	LogStore     string `json:"logStore"`
	Query        string `json:"query"`
	TimeSpanType string `json:"timeSpanType"`
	Start        string `json:"start"`
	End          string `json:"end"`
}

type Notification struct {
	Type       string   `json:"type"`
	Content    string   `json:"content"`
	EmailList  []string `json:"emailList,omitempty"`
	Method     string   `json:"method,omitempty"`
	MobileList []string `json:"mobileList,omitempty"`
	ServiceUri string   `json:"serviceUri,omitempty"`
}

type Schedule struct {
	Type     string `json:"type"`
	Interval string `json:"interval"`
}

func (c *Client) CreateSavedSearch(project string, savedSearch *SavedSearch) error {
	body, err := json.Marshal(savedSearch)
	if err != nil {
		return NewClientError(err)
	}

	h := map[string]string{
		"x-log-bodyrawsize": fmt.Sprintf("%v", len(body)),
		"Content-Type":      "application/json",
	}

	uri := "/savedsearches"
	r, err := c.request(project, "POST", uri, h, body)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) UpdateSavedSearch(project string, savedSearch *SavedSearch) error {
	body, err := json.Marshal(savedSearch)
	if err != nil {
		return NewClientError(err)
	}

	h := map[string]string{
		"x-log-bodyrawsize": fmt.Sprintf("%v", len(body)),
		"Content-Type":      "application/json",
	}

	uri := "/savedsearches/" + savedSearch.SavedSearchName
	r, err := c.request(project, "PUT", uri, h, body)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) DeleteSavedSearch(project string, savedSearchName string) error {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}

	uri := "/savedsearches/" + savedSearchName
	r, err := c.request(project, "DELETE", uri, h, nil)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) GetSavedSearch(project string, savedSearchName string) (*SavedSearch, error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}

	uri := "/savedsearches/" + savedSearchName
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return nil, err
	}
	defer r.Body.Close()
	buf, _ := ioutil.ReadAll(r.Body)
	savedSearch := &SavedSearch{}
	if err = json.Unmarshal(buf, savedSearch); err != nil {
		err = NewClientError(err)
	}
	return savedSearch, err
}

func (c *Client) ListSavedSearch(project string, savedSearchName string, offset, size int) (savedSearches []string, total int, count int, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
		"savedsearchName":   savedSearchName,
		"offset":            strconv.Itoa(offset),
		"size":              strconv.Itoa(size),
	}

	uri := "/savedsearches"
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return nil, 0, 0, err
	}
	defer r.Body.Close()

	type ListSavedSearch struct {
		Total         int      `json:"total"`
		Count         int      `json:"count"`
		Savedsearches []string `json:"savedsearches"`
	}

	buf, _ := ioutil.ReadAll(r.Body)
	listSavedSearch := &ListSavedSearch{}
	if err = json.Unmarshal(buf, listSavedSearch); err != nil {
		err = NewClientError(err)
	}
	return listSavedSearch.Savedsearches, listSavedSearch.Total, listSavedSearch.Count, err
}

func (c *Client) CreateAlert(project string, alert *Alert) error {
	body, err := json.Marshal(alert)
	if err != nil {
		return NewClientError(err)
	}
	h := map[string]string{
		"x-log-bodyrawsize": fmt.Sprintf("%v", len(body)),
		"Content-Type":      "application/json",
	}

	uri := "/jobs"
	r, err := c.request(project, "POST", uri, h, body)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) UpdateAlert(project string, alert *Alert) error {
	body, err := json.Marshal(alert)
	if err != nil {
		return NewClientError(err)
	}

	h := map[string]string{
		"x-log-bodyrawsize": fmt.Sprintf("%v", len(body)),
		"Content-Type":      "application/json",
	}

	uri := "/jobs/" + alert.Name
	r, err := c.request(project, "PUT", uri, h, body)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) DeleteAlert(project string, alertName string) error {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}

	uri := "/jobs/" + alertName
	r, err := c.request(project, "DELETE", uri, h, nil)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) DisableAlert(project string, alertName string) error {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}
	uri := fmt.Sprintf("/jobs/%s?action=disable", alertName)
	r, err := c.request(project, "PUT", uri, h, nil)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) EnableAlert(project string, alertName string) error {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}
	uri := fmt.Sprintf("/jobs/%s?action=enable", alertName)
	r, err := c.request(project, "PUT", uri, h, nil)
	if err != nil {
		return err
	}
	r.Body.Close()
	return nil
}

func (c *Client) GetAlert(project string, alertName string) (*Alert, error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}
	uri := "/jobs/" + alertName
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return nil, err
	}
	defer r.Body.Close()
	buf, _ := ioutil.ReadAll(r.Body)
	alert := &Alert{}
	if err = json.Unmarshal(buf, alert); err != nil {
		err = NewClientError(err)
	}
	return alert, err
}

func (c *Client) ListAlert(project, alertName, dashboard string, offset, size int) (alerts []*Alert, total int, count int, err error) {
	h := map[string]string{
		"x-log-bodyrawsize": "0",
		"Content-Type":      "application/json",
	}
	v := url.Values{}
	v.Add("jobName", alertName)
	v.Add("jobType", "Alert")
	v.Add("offset", fmt.Sprintf("%d", offset))
	v.Add("size", fmt.Sprintf("%d", size))
	if dashboard != "" {
		v.Add("resourceProvider", dashboard)
	}
	uri := "/jobs?" + v.Encode()
	r, err := c.request(project, "GET", uri, h, nil)
	if err != nil {
		return nil, 0, 0, err
	}
	defer r.Body.Close()

	type AlertList struct {
		Total   int      `json:"total"`
		Count   int      `json:"count"`
		Results []*Alert `json:"results"`
	}
	buf, _ := ioutil.ReadAll(r.Body)
	listAlert := &AlertList{}
	if err = json.Unmarshal(buf, listAlert); err != nil {
		err = NewClientError(err)
	}
	return listAlert.Results, listAlert.Total, listAlert.Count, err
}
