package envconfig

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	openapi "github.com/alibabacloud-go/darabonba-openapi/v2/client"
	aliyunlog "github.com/alibabacloud-go/sls-20201230/v5/client"
	"github.com/alibabacloud-go/tea/tea"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type UpdateTokenFunc func() (accessKeyID, accessKeySecret, securityToken string, expireTime time.Time, err error)

var errSTSFetchHighFrequency = errors.New("sts token fetch frequency is too high")

type AliyunLogClient struct {
	client **aliyunlog.Client
}

func (c *AliyunLogClient) GetClient() *aliyunlog.Client {
	if c == nil || c.client == nil || *c.client == nil {
		return nil
	}
	return *c.client
}

type TokenAutoUpdateClient struct {
	*aliyunlog.Client
	shutdown               <-chan struct{} // 用于接收关闭信号的通道
	closeFlag              bool            // 关闭标志
	tokenUpdateFunc        UpdateTokenFunc // 更新Token的函数
	maxTryTimes            int             // 最大尝试次数 默认3
	waitIntervalMin        time.Duration   // 最小等待间隔 默认1s
	waitIntervalMax        time.Duration   // 最大等待间隔 默认60s
	updateTokenIntervalMin time.Duration   // 更新Token的最小间隔 默认1s
	nextExpire             time.Time       // 下次过期时间

	lock               sync.Mutex
	lastFetch          time.Time     // 最后一次获取Token的时间
	lastRetryFailCount int           // 最后一次重试失败的次数
	lastRetryInterval  time.Duration // 最后一次重试的间隔
	ctx                context.Context
}

// flushSTSToken 用于刷新STS Token
func (c *TokenAutoUpdateClient) flushSTSToken() {
	for {
		nowTime := time.Now()
		c.lock.Lock()
		// 计算下次过期时间与当前时间的差值，即需要睡眠的时间, 并根据不同的情况调整睡眠时间
		sleepTime := c.nextExpire.Sub(nowTime)
		switch {
		case sleepTime < time.Minute:
			sleepTime = time.Second * 30
		case sleepTime < time.Minute*10:
			sleepTime = sleepTime / 10 * 7
		case sleepTime < time.Hour:
			sleepTime = sleepTime / 10 * 6
		default:
			sleepTime = sleepTime / 10 * 5
		}
		c.lock.Unlock()
		logger.Debug(c.ctx, "next fetch sleep interval", sleepTime.String())
		trigger := time.After(sleepTime)
		select {
		case <-trigger:
			err := c.fetchSTSToken()
			if err != nil {
				logger.Error(c.ctx, "FetchSTSTokenError", "fetch sts token done, error", err)
			}
		case <-c.shutdown:
			logger.Info(c.ctx, "receive shutdown signal", "exit flushSTSToken")
			return
		}
		if c.closeFlag {
			logger.Info(c.ctx, "close flag is true", "exit flushSTSToken")
			return
		}
	}
}

// fetchSTSToken 用于获取STS Token
func (c *TokenAutoUpdateClient) fetchSTSToken() error {
	nowTime := time.Now()
	skip := false
	sleepTime := time.Duration(0)
	c.lock.Lock()
	// 如果当前时间与最后一次获取Token的时间差值小于更新Token的最小间隔，就跳过获取Token
	if nowTime.Sub(c.lastFetch) < c.updateTokenIntervalMin {
		skip = true
	} else {
		// 更新最后一次获取Token的时间
		c.lastFetch = nowTime
		// 如果最后一次重试失败的次数为0，就不需要睡眠
		if c.lastRetryFailCount == 0 {
			sleepTime = 0
		} else {
			// 否则，将最后一次重试的间隔翻倍
			c.lastRetryInterval *= 2
			// 如果最后一次重试的间隔小于最小等待间隔，就设置为最小等待间隔
			if c.lastRetryInterval < c.waitIntervalMin {
				c.lastRetryInterval = c.waitIntervalMin
			}
			// 如果最后一次重试的间隔大于等于最大等待间隔，就设置为最大等待间隔
			if c.lastRetryInterval >= c.waitIntervalMax {
				c.lastRetryInterval = c.waitIntervalMax
			}
			// 设置需要睡眠的时间为最后一次重试的间隔
			sleepTime = c.lastRetryInterval
		}
	}
	c.lock.Unlock()
	if skip {
		return errSTSFetchHighFrequency
	}
	if sleepTime > time.Duration(0) {
		time.Sleep(sleepTime)
	}

	// 调用更新Token的函数获取Token
	accessKeyID, accessKeySecret, securityToken, expireTime, err := c.tokenUpdateFunc()
	if err == nil { // 如果获取Token成功
		c.lock.Lock()
		// 将最后一次重试失败的次数设置为0, 将最后一次重试的间隔设置为0,更新下次过期时间
		c.lastRetryFailCount = 0
		c.lastRetryInterval = time.Duration(0)
		c.nextExpire = expireTime
		c.lock.Unlock()
		// 创建一个新的日志服务客户端
		var logClient **aliyunlog.Client
		logClient, err = CreateNormalInterface(*c.Client.Endpoint, accessKeyID, accessKeySecret, securityToken, *c.Client.UserAgent)
		if err != nil {
			return err
		}
		c.Client = *logClient
		logger.Info(c.ctx, "fetch sts token success id", accessKeyID)
	} else { // 如果获取Token失败
		c.lock.Lock()
		// 将最后一次重试失败的次数加1, 记录获取Token失败的错误信息
		c.lastRetryFailCount++
		c.lock.Unlock()
		err = fmt.Errorf("tokenUpdateFunc error:%v", err.Error())
	}
	return err
}

// CreateNormalInterface 用于创建一个普通的日志服务客户端
func CreateNormalInterface(endpoint, accessKeyID, accessKeySecret, securityToken string, userAgent string) (**aliyunlog.Client, error) {
	openapiConfig := &openapi.Config{
		AccessKeyId:     tea.String(accessKeyID),
		AccessKeySecret: tea.String(accessKeySecret),
		SecurityToken:   tea.String(securityToken),
		UserAgent:       tea.String(userAgent),
	}
	openapiConfig.Endpoint = tea.String(endpoint)
	var logClient *aliyunlog.Client
	logClient, err := aliyunlog.NewClient(openapiConfig)
	if err != nil {
		err = fmt.Errorf("aliyunlog NewClient error:%v", err.Error())
	}
	return &logClient, err
}

// CreateTokenAutoUpdateClient 用于创建一个自动更新Token的客户端
func CreateTokenAutoUpdateClient(endpoint string, tokenUpdateFunc UpdateTokenFunc, shutdown <-chan struct{}, userAgent string) (**aliyunlog.Client, error) {
	// 调用更新Token的函数获取Token
	accessKeyID, accessKeySecret, securityToken, expireTime, err := tokenUpdateFunc()
	if err != nil {
		err = fmt.Errorf("tokenUpdateFunc error:%v", err.Error())
		return nil, err
	}

	// 创建一个普通的日志服务客户端
	logClient, err := CreateNormalInterface(endpoint, accessKeyID, accessKeySecret, securityToken, userAgent)
	if err != nil {
		return nil, err
	}
	tauc := &TokenAutoUpdateClient{
		Client:                 *logClient,
		shutdown:               shutdown,
		closeFlag:              false,
		tokenUpdateFunc:        tokenUpdateFunc,
		maxTryTimes:            3,
		waitIntervalMin:        time.Second * 1,
		waitIntervalMax:        time.Second * 60,
		updateTokenIntervalMin: time.Second * 1,
		nextExpire:             expireTime,
		lock:                   sync.Mutex{},
		lastFetch:              time.Now(),
		lastRetryFailCount:     0,
		lastRetryInterval:      0,
		ctx:                    context.Background(),
	}
	go tauc.flushSTSToken()
	return &tauc.Client, nil
}

func CreateNormalAliyunLogClient(endpoint, accessKeyID, accessKeySecret, securityToken string, userAgent string) (*AliyunLogClient, error) {
	logClient, err := CreateNormalInterface(endpoint, accessKeyID, accessKeySecret, securityToken, userAgent)
	if err != nil {
		return nil, err
	}
	return &AliyunLogClient{
		client: logClient,
	}, nil
}

func CreateTokenAutoUpdateAliyunLogClient(endpoint string, tokenUpdateFunc UpdateTokenFunc, shutdown <-chan struct{}, userAgent string) (*AliyunLogClient, error) {
	logClient, err := CreateTokenAutoUpdateClient(endpoint, UpdateTokenFunction, shutdown, userAgent)
	if err != nil {
		return nil, err
	}
	return &AliyunLogClient{
		client: logClient,
	}, nil
}
