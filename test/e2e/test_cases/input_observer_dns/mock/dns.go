// Copyright 2022 iLogtail Authors
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

package main

import (
	"errors"
	"fmt"
	"log"
	"net"
	"strconv"
	"sync"
	"time"

	"github.com/miekg/dns"
)

var domainsToAddresses map[string]string = map[string]string{
	"google.com.":       "1.2.3.4",
	"jameshfisher.com.": "104.198.14.52",
}

type handler struct{}

func (*handler) ServeDNS(w dns.ResponseWriter, r *dns.Msg) {
	msg := dns.Msg{}
	msg.SetReply(r)
	if *latency > 0 {
		time.Sleep(time.Millisecond * time.Duration(*latency))
	}
	switch r.Question[0].Qtype {
	case dns.TypeA:
		msg.Authoritative = true
		domain := msg.Question[0].Name
		msg.Answer = append(msg.Answer, &dns.A{
			Hdr: dns.RR_Header{Name: domain, Rrtype: dns.TypeA, Class: dns.ClassINET, Ttl: 60},
			A:   net.ParseIP("123.212.196.123"),
		})
	}
	w.WriteMsg(&msg)
}

// https://jameshfisher.com/2017/08/04/golang-dns-server/
func mocDnsServer() {
	srv := &dns.Server{Addr: *address + ":" + strconv.Itoa(*port), Net: "udp"}
	srv.Handler = &handler{}
	if err := srv.ListenAndServe(); err != nil {
		log.Fatalf("Failed to set udp listener %s\n", err.Error())
	}
}

func searchServerIP(domain string, version int, DNSserver string) (answer *dns.Msg, err error) {
	dnsRequest := new(dns.Msg)
	if dnsRequest == nil {
		return nil, errors.New("Can not new dnsRequest")
	}
	dnsClient := new(dns.Client)
	if dnsClient == nil {
		return nil, errors.New("Can not new dnsClient")
	}
	if version == 4 {
		dnsRequest.SetQuestion(domain+".", dns.TypeA)
	} else if version == 6 {
		dnsRequest.SetQuestion(domain+".", dns.TypeAAAA)
	} else {
		return nil, errors.New("wrong parameter in version")
	}
	dnsRequest.SetEdns0(4096, true)
	answer, _, err = dnsClient.Exchange(dnsRequest, DNSserver)
	if err != nil {
		return nil, err
	}
	return answer, nil
}

var (
	SuccessCount int64
	FailureCount int64
	TotalLatency int64
)

// https://golang.hotexamples.com/zh/examples/github.com.miekg.dns/Client/-/golang-client-class-examples.html
func mockDnsClient() {
	subQps := *qps / *concurrency
	subCount := *count / *concurrency / subQps
	var wg sync.WaitGroup
	for i := 0; i < *concurrency; i++ {
		wg.Add(1)
		go func(index int) {
			for j := 0; j < subCount; j++ {
				nowTime := time.Now()
				for k := 0; k < subQps; k++ {
					// request
					rst, err := searchServerIP("sls.test.ebpf", 4, *address+":"+strconv.Itoa(*port))
					if *print {
						if err == nil {
							fmt.Printf("request : sls.test.ebpf , response : %s \n", rst.String())
						} else {
							fmt.Printf("request : sls.test.ebpf , error : %s \n", err)
						}
					}
				}
				if duration := time.Since(nowTime); duration < time.Second {
					time.Sleep(time.Second - duration)
				}
			}
			wg.Done()
		}(i)

	}
	wg.Wait()
}
