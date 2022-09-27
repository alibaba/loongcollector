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
	"time"

	"github.com/pingcap/check"

	"github.com/alibaba/ilogtail/pkg/flags"
)

// go test -check.f "logConfigManagerTestSuite.*" -args -debug=true -console=true
var _ = check.Suite(&logConfigManagerTestSuite{})

type logConfigManagerTestSuite struct {
}

func (s *logConfigManagerTestSuite) SetUpSuite(c *check.C) {
	*flags.DockerEnvUpdateInterval = 3
}

func (s *logConfigManagerTestSuite) TearDownSuite(c *check.C) {
}

func (s *logConfigManagerTestSuite) SetUpTest(c *check.C) {

}

func (s *logConfigManagerTestSuite) TearDownTest(c *check.C) {

}

func (s *logConfigManagerTestSuite) TestE2ECheck(c *check.C) {
	time.Sleep(time.Second * time.Duration(40))
	client := dockerEnvConfigManager.operationWrapper.logClient
	ok, err := client.CheckProjectExist(*flags.DefaultLogProject)
	c.Assert(ok, check.IsTrue)
	c.Assert(err, check.IsNil)
	ok, err = client.CheckLogstoreExist(*flags.DefaultLogProject, "stdout-logtail")
	c.Assert(ok, check.IsTrue)
	c.Assert(err, check.IsNil)
	ok, err = client.CheckLogstoreExist(*flags.DefaultLogProject, "stdout")
	c.Assert(ok, check.IsTrue)
	c.Assert(err, check.IsNil)
	ok, err = client.CheckLogstoreExist(*flags.DefaultLogProject, "catalina")
	c.Assert(ok, check.IsTrue)
	c.Assert(err, check.IsNil)
	ok, err = client.CheckLogstoreExist(*flags.DefaultLogProject, "catalina2")
	c.Assert(ok, check.IsTrue)
	c.Assert(err, check.IsNil)
	ok, err = client.CheckMachineGroupExist(*flags.DefaultLogProject, *flags.DefaultLogMachineGroup)
	c.Assert(ok, check.IsTrue)
	c.Assert(err, check.IsNil)
	configNames, err := client.GetAppliedConfigs(*flags.DefaultLogProject, *flags.DefaultLogMachineGroup)
	c.Assert(len(configNames), check.Equals, 4)
	c.Assert(err, check.IsNil)
}
