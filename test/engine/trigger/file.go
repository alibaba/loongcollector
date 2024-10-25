package trigger

import (
	"context"
	"fmt"
	"math/rand"
	"os"
	"path/filepath"
	"time"

	"golang.org/x/time/rate"

	"github.com/alibaba/ilogtail/test/config"
)

// JSON template
func GenerateRandomNginxLogToFile(ctx context.Context, speed, totalTime int, path string) (context.Context, error) {

	// clear file
	path = filepath.Clean(path)
	path = filepath.Join(config.CaseHome, path)
	fmt.Println(path)
	_ = os.WriteFile(path, []byte{}, 0600)
	file, _ := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0600) // #nosec G304

	rand.Seed(time.Now().UnixNano())
	maxLogLen := 1024
	nginxLog := genNginxLog()

	limiter := rate.NewLimiter(rate.Limit(speed*1024*1024), maxLogLen)

	timeout := time.After(time.Minute * time.Duration(totalTime))

	for {
		select {
		// context is done
		case <-ctx.Done():
			// clear file
			_ = file.Close()
			return ctx, nil
		// all time is done
		case <-timeout:
			// clear file
			_ = file.Close()
			return ctx, nil
		default:
			if limiter.AllowN(time.Now(), len(nginxLog)) {
				_, _ = file.WriteString(nginxLog + "\n") // #nosec G307
				nginxLog = genNginxLog()
			}
		}
	}
}

var ipAddresses = []string{
	"103.159.151.180",
	"12.55.18.241",
	"182.233.128.102",
	"221.85.57.231",
	"76.245.65.224",
	"86.250.231.93",
	"44.201.253.252",
	"218.7.2.219",
	"172.118.174.109",
	"208.16.46.154",
	"7.138.80.41",
	"214.73.25.80",
	"83.124.20.79",
	"80.226.48.153",
	"92.129.204.161",
	"212.103.145.159",
	"148.188.8.90",
	"148.212.244.121",
	"106.186.172.157",
	"30.127.196.158",
}

var userAgents = []string{
	"aliyun-sdk-java",
	"aliyun-sdk-golang",
	"aliyun-sdk-python",
}

var statusCodes = []string{
	"400",
	"401",
	"402",
	"403",
	"404",
	"200",
}

const bytesMean = 5500.0
const bytesStddev = 1500.0

func genNginxLog() string {
	nginxLogTemplate := `%s - - [%s] "GET http://www.districtdot-com.biz/syndicate HTTP/1.1" %s %d "http://www.chiefscalable.biz/webservices" "%s"`
	currentTime := time.Now().Format("02/Jan/2006:15:04:05 +0800")
	ipAddress := ipAddresses[rand.Intn(len(ipAddresses))] // #nosec G404
	statusIdx := rand.Intn(len(statusCodes) * 10)         // #nosec G404
	if statusIdx >= len(statusCodes) {
		statusIdx = len(statusCodes) - 1
	}
	bytesSize := int32(rand.NormFloat64()*bytesStddev + bytesMean)
	if bytesSize < 1000 {
		bytesSize = 0
	} else if bytesSize > 10000 {
		bytesSize = 10000
	}
	statusCode := statusCodes[statusIdx]
	userAgent := userAgents[rand.Intn(len(userAgents))] // #nosec G404

	return fmt.Sprintf(nginxLogTemplate, ipAddress, currentTime, statusCode, bytesSize, userAgent)
}
