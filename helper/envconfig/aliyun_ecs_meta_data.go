// Copyright 2021 iLogtail Authors
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

package envconfig

import (
	"context"
	"crypto/aes"
	"crypto/cipher"
	"encoding/base64"
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

const (
	aliyunECSRamURL      = "http://100.100.100.200/latest/meta-data/ram/security-credentials/"
	expirationTimeFormat = "2006-01-02T15:04:05Z"
)

var errNoFile = errors.New("no secret file")

const (
	AddOnTokenConfigPathEnv     = "ALIYUN_LOG_ADDON_TOKEN_PATH" //nolint:gosec
	DefaultAddOnTokenConfigPath = "/var/addon/token-config"     //nolint:gosec
)

// AKInfo ...
type AKInfo struct {
	AccessKeyID     string `json:"access.key.id"`
	AccessKeySecret string `json:"access.key.secret"`
	SecurityToken   string `json:"security.token"`
	Expiration      string `json:"expiration"`
	Keyring         string `json:"keyring"`
}

type SecurityTokenResult struct {
	AccessKeyID     string
	AccessKeySecret string
	Expiration      string
	SecurityToken   string
	Code            string
	LastUpdated     string
}

func getToken() (result []byte, err error) {
	client := http.Client{
		Timeout: time.Second * 3,
	}
	var respList *http.Response
	logger.Debug(context.Background(), "get role list request", aliyunECSRamURL)
	respList, err = client.Get(aliyunECSRamURL)
	if err != nil {
		logger.Warning(context.Background(), "UPDATE_STS_ALARM", "get role list error", err)
		return nil, err
	}
	defer respList.Body.Close()
	var body []byte
	body, err = io.ReadAll(respList.Body)
	if err != nil {
		logger.Warning(context.Background(), "UPDATE_STS_ALARM", "parse role list error", err)
		return nil, err
	}
	logger.Debug(context.Background(), "get role list response", string(body))

	bodyStr := string(body)
	bodyStr = strings.TrimSpace(bodyStr)
	roles := strings.Split(bodyStr, "\n")
	role := roles[0]

	var respGet *http.Response
	logger.Debug(context.Background(), "get token request", aliyunECSRamURL+role)
	respGet, err = client.Get(aliyunECSRamURL + role)
	if err != nil {
		logger.Warning(context.Background(), "UPDATE_STS_ALARM", "get token error", err, "role", role)
		return nil, err
	}
	defer respGet.Body.Close()
	body, err = io.ReadAll(respGet.Body)
	if err != nil {
		logger.Warning(context.Background(), "UPDATE_STS_ALARM", "parse token error", err, "role", role)
		return nil, err
	}
	return body, nil
}
func PKCS5UnPadding(origData []byte) []byte {
	length := len(origData)
	unpadding := int(origData[length-1])
	return origData[:(length - unpadding)]
}

func decrypt(s string, keyring []byte) ([]byte, error) {
	cdata, err := base64.StdEncoding.DecodeString(s)
	if err != nil {
		return nil, err
	}
	block, err := aes.NewCipher(keyring)
	if err != nil {
		return nil, err
	}
	blockSize := block.BlockSize()

	iv := cdata[:blockSize]
	blockMode := cipher.NewCBCDecrypter(block, iv)
	origData := make([]byte, len(cdata)-blockSize)

	blockMode.CryptBlocks(origData, cdata[blockSize:])

	origData = PKCS5UnPadding(origData)
	return origData, nil
}

func getAddOnTokenConfigPath() string {
	tokenPath := os.Getenv(AddOnTokenConfigPathEnv)
	if len(tokenPath) == 0 {
		return DefaultAddOnTokenConfigPath
	}
	return tokenPath
}

func getAKFromLocalFile() (accessKeyID, accessKeySecret, securityToken string, expireTime time.Time, err error) {
	addonTokenConfigPath := getAddOnTokenConfigPath()
	if runtime.GOOS == "windows" {
		addonTokenConfigPath = helper.NormalizeWindowsPath(addonTokenConfigPath)

		// NB(thxCode): since os.Stat has not worked as expected,
		// we use os.Lstat instead of os.Stat here,
		// ref to https://github.com/microsoft/Windows-Containers/issues/97#issuecomment-887713195.
		_, err = os.Lstat(addonTokenConfigPath)
	} else {
		_, err = os.Stat(addonTokenConfigPath)
	}
	logger.Info(context.Background(), "get security token addon tokenconfig. path", addonTokenConfigPath,
		"ret_code", err)

	if err == nil {
		var akInfo AKInfo
		// 获取token config json
		encodeTokenCfg, err := os.ReadFile(filepath.Clean(addonTokenConfigPath))
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		err = json.Unmarshal(encodeTokenCfg, &akInfo)
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		keyring := akInfo.Keyring
		ak, err := decrypt(akInfo.AccessKeyID, []byte(keyring))
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}

		sk, err := decrypt(akInfo.AccessKeySecret, []byte(keyring))
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}

		token, err := decrypt(akInfo.SecurityToken, []byte(keyring))
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		layout := "2006-01-02T15:04:05Z"
		t, err := time.Parse(layout, akInfo.Expiration)
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		if t.Before(time.Now()) {
			err = errors.New("invalid token which is expired")
		}
		akInfo.AccessKeyID = string(ak)
		akInfo.AccessKeySecret = string(sk)
		akInfo.SecurityToken = string(token)

		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		return akInfo.AccessKeyID, akInfo.AccessKeySecret, akInfo.SecurityToken, t, nil
	}
	return accessKeyID, accessKeySecret, securityToken, expireTime, errNoFile
}

func UpdateTokenFunction() (accessKeyID, accessKeySecret, securityToken string, expireTime time.Time, err error) {
	{
		accessKeyID, accessKeySecret, securityToken, expireTime, err = getAKFromLocalFile()
		if err != errNoFile {
			logger.Info(context.Background(), "get security token, id", accessKeyID, "expire", expireTime, "error", err)
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
	}
	{
		var tokenResultBuffer []byte
		tokenResultBuffer, err = getToken()
		if err != nil {
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		var tokenResult SecurityTokenResult
		err = json.Unmarshal(tokenResultBuffer, &tokenResult)
		if err != nil {
			logger.Warning(context.Background(), "UPDATE_STS_ALARM", "unmarshal token error", err, "token", string(tokenResultBuffer))
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		if strings.ToLower(tokenResult.Code) != "success" {
			tokenResult.AccessKeySecret = "xxxxx"
			tokenResult.SecurityToken = "xxxxx"
			logger.Warning(context.Background(), "UPDATE_STS_ALARM", "token code not success", err, "result", tokenResult)
			return accessKeyID, accessKeySecret, securityToken, expireTime, errors.New("token code not success")
		}
		expireTime, err = time.Parse(expirationTimeFormat, tokenResult.Expiration)
		if err != nil {
			tokenResult.AccessKeySecret = "xxxxx"
			tokenResult.SecurityToken = "xxxxx"
			logger.Warning(context.Background(), "UPDATE_STS_ALARM", "parse time error", err, "result", tokenResult)
			return accessKeyID, accessKeySecret, securityToken, expireTime, err
		}
		logger.Info(context.Background(), "get security token success, id", tokenResult.AccessKeyID, "expire", tokenResult.Expiration, "last update", tokenResult.LastUpdated)
		return tokenResult.AccessKeyID, tokenResult.AccessKeySecret, tokenResult.SecurityToken, expireTime, nil
	}
}
