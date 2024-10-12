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

package pluginmanager

import (
	"context"
	"testing"
	"time"
)

func Test_checkPointManager_SaveGetCheckpoint(t *testing.T) {
	CheckPointManager.Init()
	tests := []string{"xx", "xx", "213##13143", "~/.."}
	for _, tt := range tests {
		t.Run(tt, func(t *testing.T) {
			if err := CheckPointManager.SaveCheckpoint("1", tt, []byte(tt)); err != nil {
				t.Errorf("checkPointManager.SaveCheckpoint() error = %v", err)
			}
			if data, err := CheckPointManager.GetCheckpoint("1", tt); err != nil || string(data) != tt {
				t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
			}
			if err := CheckPointManager.DeleteCheckpoint("1", tt); err != nil {
				t.Errorf("checkPointManager.GetCheckpoint() error, %v ", err)
			}
		})
	}
}

func Test_checkPointManager_HoldOn(t *testing.T) {
	CheckPointManager.Init()
	t.Run("hold on resume", func(t *testing.T) {
		ctx, cancel := context.WithTimeout(context.Background(), time.Second*time.Duration(10))
		defer cancel()
		shutdown := make(chan struct{}, 1)
		go func() {
			for i := 0; i < 100; i++ {
				CheckPointManager.Start()
				CheckPointManager.Stop()
			}
			shutdown <- struct{}{}
		}()
		select {
		case <-ctx.Done():
			t.Errorf("checkPointManager timeout")
		case <-shutdown:
			break
		}
	})
}

func Test_checkPointManager_run(t *testing.T) {
	CheckPointManager.Init()
	t.Run("hold on resume", func(t *testing.T) {
		CheckPointManager.SaveCheckpoint("1", "xx", []byte("xxxxx"))
		CheckPointManager.SaveCheckpoint("2", "yy", []byte("yyyyyy"))
		*CheckPointCleanInterval = 1
		CheckPointManager.cleanThreshold = 3
		if data, err := CheckPointManager.GetCheckpoint("1", "xx"); err != nil || string(data) != "xxxxx" {
			t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
		}

		if data, err := CheckPointManager.GetCheckpoint("2", "yy"); err != nil || string(data) != "yyyyyy" {
			t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
		}
		CheckPointManager.Start()
		time.Sleep(time.Second * time.Duration(1))
		if data, err := CheckPointManager.GetCheckpoint("1", "xx"); err != nil || string(data) != "xxxxx" {
			t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
		}

		if data, err := CheckPointManager.GetCheckpoint("2", "yy"); err != nil || string(data) != "yyyyyy" {
			t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
		}
		time.Sleep(time.Second * time.Duration(5))
		if data, err := CheckPointManager.GetCheckpoint("1", "xx"); err == nil {
			t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
		}

		if data, err := CheckPointManager.GetCheckpoint("2", "yy"); err == nil {
			t.Errorf("checkPointManager.GetCheckpoint() error, %v %v", err, string(data))
		}
	})

	*CheckPointCleanInterval = 3600
	CheckPointManager.Stop()
}

func Test_checkPointManager_keyMatch(t *testing.T) {
	CheckPointManager.Init()
	t.Run("key match", func(t *testing.T) {
		LogtailConfigLock.Lock()
		LogtailConfig["test_1/1"] = nil
		LogtailConfig["test_2/1"] = nil
		LogtailConfigLock.Unlock()
		if got := CheckPointManager.keyMatch([]byte("test_1")); got {
			t.Errorf("checkPointManager.Test_checkPointManager_keyMatch()")
		}

		if got := CheckPointManager.keyMatch([]byte("test_1^")); !got {
			t.Errorf("checkPointManager.Test_checkPointManager_keyMatch()")
		}

		if got := CheckPointManager.keyMatch([]byte("test_2^xxx")); !got {
			t.Errorf("checkPointManager.Test_checkPointManager_keyMatch()")
		}

		if got := CheckPointManager.keyMatch([]byte("^test_1")); got {
			t.Errorf("checkPointManager.Test_checkPointManager_keyMatch()")
		}

		if got := CheckPointManager.keyMatch([]byte("texst_1^xxx")); got {
			t.Errorf("checkPointManager.Test_checkPointManager_keyMatch()")
		}
		LogtailConfigLock.Lock()
		delete(LogtailConfig, "test_1/1")
		delete(LogtailConfig, "test_2/1")
		LogtailConfigLock.Unlock()
	})
}
