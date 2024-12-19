package main

import (
	"crypto/rand"
	"flag"
	"fmt"
	"math/big"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/cucumber/godog"

	"github.com/alibaba/ilogtail/test/engine"
)

func main() {
	name := flag.String("test_case", "soak", "test case type")
	flag.Parse()

	for {
		triggerOneRound(*name)
		time.Sleep(5 * time.Minute)
	}
}

func triggerOneRound(name string) {
	fmt.Println("=====================================")
	fmt.Printf("Trigger one round of %s\n", name)
	// walk all files name endwith .feature in name
	files := make([]string, 0)
	err := filepath.Walk(name, func(path string, info os.FileInfo, err error) error {
		if err != nil {
			return err
		}
		if strings.HasSuffix(path, ".feature") {
			files = append(files, path)
		}
		return nil
	})
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Printf("found %d files\n", len(files))
	randomIndex, err := rand.Int(rand.Reader, big.NewInt(int64(len(files))))
	if err != nil {
		fmt.Println(err)
		return
	}
	fmt.Printf("run %s\n", files[randomIndex.Int64()])
	fmt.Println("=====================================")

	// run godog
	suite := godog.TestSuite{
		Name:                "Soak",
		ScenarioInitializer: engine.ScenarioInitializer,
		Options: &godog.Options{
			Format: "pretty",
			Paths:  []string{files[randomIndex.Int64()]},
		},
	}
	if suite.Run() != 0 {
		fmt.Printf("run %s failed\n", files[randomIndex.Int64()])
		return
	}
}
