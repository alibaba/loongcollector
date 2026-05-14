//go:build windows
// +build windows

package input_etw

import (
	"encoding/json"
	"testing"

	"golang.org/x/sys/windows"

	"github.com/miekg/dns"

	"github.com/alibaba/ilogtail/plugins/test/mock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestEtwInput_Init_WithProviderName(t *testing.T) {
	input := &EtwInput{
		ProviderName: "Microsoft-Windows-Kernel-Process",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.NotEqual(t, windows.GUID{}, input.parsedGUID)
}

func TestEtwInput_Init_WithProviderGUID(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.NotEqual(t, windows.GUID{}, input.parsedGUID)
}

func TestEtwInput_Init_ProviderNameTakesPrecedence(t *testing.T) {
	input := &EtwInput{
		ProviderName: "Microsoft-Windows-Kernel-Process",
		ProviderGUID: "not-a-valid-guid",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.NotEqual(t, windows.GUID{}, input.parsedGUID)
}

func TestEtwInput_Init_NoProvider(t *testing.T) {
	input := &EtwInput{}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "either ProviderName or ProviderGUID must be specified")
}

func TestEtwInput_Init_InvalidGUID(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "not-a-valid-guid",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "invalid GUID")
}

func TestEtwInput_Init_Defaults(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.Equal(t, 4, input.Level)
	assert.Equal(t, uint64(0), uint64(input.Keywords))
}

func TestEtwInput_Init_InvalidLevel(t *testing.T) {
	tests := []int{-1, 6}
	for _, level := range tests {
		input := &EtwInput{
			ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
			Level:        level,
		}
		ctx := mock.NewEmptyContext("test", "test", "test")
		_, err := input.Init(ctx)
		require.Error(t, err)
		assert.Contains(t, err.Error(), "invalid Level")
	}
}

func TestEtwInput_Description(t *testing.T) {
	input := &EtwInput{}
	assert.NotEmpty(t, input.Description())
}

func TestEtwInput_StopBeforeStart(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	err = input.Stop()
	assert.NoError(t, err)
}

func TestKeywordMask_UnmarshalJSON(t *testing.T) {
	tests := []struct {
		name string
		data string
		want uint64
	}{
		{name: "number", data: `{"Keywords":15}`, want: 15},
		{name: "decimal string", data: `{"Keywords":"15"}`, want: 15},
		{name: "hex string", data: `{"Keywords":"0x8000000000000037"}`, want: 0x8000000000000037},
		{name: "empty string", data: `{"Keywords":""}`, want: 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var input EtwInput
			require.NoError(t, json.Unmarshal([]byte(tt.data), &input))
			assert.Equal(t, tt.want, uint64(input.Keywords))
		})
	}
}

func TestKeywordMask_UnmarshalJSON_Invalid(t *testing.T) {
	var input EtwInput
	err := json.Unmarshal([]byte(`{"Keywords":"not-a-number"}`), &input)
	require.Error(t, err)
	assert.Contains(t, err.Error(), "invalid Keywords")
}

func TestNormalizeETWFieldName(t *testing.T) {
	tests := map[string]string{
		"QNAME":           "qname",
		"QTYPE":           "qtype",
		"TCP":             "tcp",
		"InterfaceIP":     "interface_ip",
		"BufferSize":      "buffer_size",
		"RecursionDepth":  "recursion_depth",
		"AdditionalInfo":  "additional_info",
		"QueriesAttached": "queries_attached",
		"PacketData":      "packet_data",
		"CacheScope":      "cache_scope",
	}
	for input, expected := range tests {
		assert.Equal(t, expected, normalizeETWFieldName(input))
	}
}

func TestResolveProviderName(t *testing.T) {
	guid, err := resolveProviderName("Microsoft-Windows-Kernel-Process")
	require.NoError(t, err)
	assert.NotEmpty(t, guid)
	assert.Contains(t, guid, "{")
	assert.Contains(t, guid, "}")
}

func TestResolveProviderName_NotFound(t *testing.T) {
	_, err := resolveProviderName("NonExistent-Provider-That-Does-Not-Exist-12345")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "not found")
}

func TestBuildWindowsOSVersion(t *testing.T) {
	assert.Equal(t, "10.0.20348.0", buildWindowsOSVersion(10, 0, true, true, "", "20348"))
	assert.Equal(t, "6.3.9600.0", buildWindowsOSVersion(0, 0, false, false, "6.3", "9600"))
	assert.Equal(t, "10.0.17763.0", buildWindowsOSVersion(0, 0, false, false, "", "17763"))
}

// --- DNS Enrichment Tests ---

func TestIsDNSProvider(t *testing.T) {
	tests := []struct {
		name     string
		input    EtwInput
		expected bool
	}{
		{
			name:     "by ProviderName",
			input:    EtwInput{ProviderName: "Microsoft-Windows-DNSServer"},
			expected: true,
		},
		{
			name:     "by ProviderName lowercase",
			input:    EtwInput{ProviderName: "microsoft-windows-dnsserver"},
			expected: true,
		},
		{
			name:     "by ProviderGUID uppercase",
			input:    EtwInput{ProviderGUID: "{EB79061A-A566-4698-9119-3ED2807060E7}"},
			expected: true,
		},
		{
			name:     "by ProviderGUID lowercase",
			input:    EtwInput{ProviderGUID: "{eb79061a-a566-4698-9119-3ed2807060e7}"},
			expected: true,
		},
		{
			name:     "non-DNS provider",
			input:    EtwInput{ProviderName: "Microsoft-Windows-Kernel-Process"},
			expected: false,
		},
		{
			name:     "empty",
			input:    EtwInput{},
			expected: false,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equal(t, tt.expected, tt.input.isDNSProvider())
		})
	}
}

func TestMapDNSQueryType(t *testing.T) {
	assert.Equal(t, "A", mapDNSQueryType("1"))
	assert.Equal(t, "AAAA", mapDNSQueryType("28"))
	assert.Equal(t, "CNAME", mapDNSQueryType("5"))
	assert.Equal(t, "MX", mapDNSQueryType("15"))
	assert.Equal(t, "TXT", mapDNSQueryType("16"))
	assert.Equal(t, "SRV", mapDNSQueryType("33"))
	assert.Equal(t, "NS", mapDNSQueryType("2"))
	assert.Equal(t, "SOA", mapDNSQueryType("6"))
	assert.Equal(t, "PTR", mapDNSQueryType("12"))
	assert.Equal(t, "ANY", mapDNSQueryType("255"))
	assert.Equal(t, "TYPE999", mapDNSQueryType("999"))
}

func TestMapDNSResponseCode(t *testing.T) {
	assert.Equal(t, "NOERROR", mapDNSResponseCode(0))
	assert.Equal(t, "FORMERR", mapDNSResponseCode(1))
	assert.Equal(t, "SERVFAIL", mapDNSResponseCode(2))
	assert.Equal(t, "NXDOMAIN", mapDNSResponseCode(3))
	assert.Equal(t, "REFUSED", mapDNSResponseCode(5))
	assert.Equal(t, "RCODE99", mapDNSResponseCode(99))
}

func TestEnrichDNSFields_Event256(t *testing.T) {
	d := &EtwInput{
		ProviderName: "Microsoft-Windows-DNSServer",
		hostname:     "dns-server-01",
		domain:       "example.local",
		domainType:   "FQDN",
		osName:       "Windows",
		osVersion:    "10.0.17763.0",
	}
	fields := map[string]string{
		"source":       "192.168.1.100",
		"interface_ip": "10.0.0.1",
		"port":         "53214",
		"qname":        "example.com.",
		"qtype":        "1",
		"tcp":          "0",
	}
	d.enrichDNSFields(fields, 256)

	assert.Equal(t, "192.168.1.100", fields["src_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dst_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dvc_ip_addr"])
	assert.Equal(t, "53214", fields["src_port_number"])
	assert.Equal(t, "request", fields["event_sub_type"])
	assert.Equal(t, "example.com", fields["dns_query"])
	assert.Equal(t, "A", fields["dns_query_type_name"])
	assert.Equal(t, "dns-server-01", fields["dvc_hostname"])
	assert.Equal(t, "example.local", fields["dvc_domain"])
	assert.Equal(t, "FQDN", fields["dvc_domain_type"])
	assert.Equal(t, "Windows", fields["dvc_os"])
	assert.Equal(t, "10.0.17763.0", fields["dvc_os_version"])
	assert.Equal(t, "udp", fields["network_protocol"])
	assert.Equal(t, "NA", fields["event_result_details"])
	assert.Equal(t, "10.0.0.1", d.serverIP, "should cache serverIP from Event 256")
}

func TestEnrichDNSFields_Event256_Loopback(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{
		"source":       "127.0.0.1",
		"interface_ip": "127.0.0.1",
	}
	d.enrichDNSFields(fields, 256)

	assert.Equal(t, "127.0.0.1", fields["dst_ip_addr"])
	assert.Empty(t, d.serverIP, "should NOT cache 127.0.0.1 as serverIP")
}

func TestEnrichDNSFields_Event260(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{
		"interface_ip": "10.0.0.1",
		"destination":  "8.8.8.8",
	}
	d.enrichDNSFields(fields, 260)

	assert.Equal(t, "10.0.0.1", fields["src_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dvc_ip_addr"])
	assert.Equal(t, "8.8.8.8", fields["dst_ip_addr"])
	assert.Equal(t, "request", fields["event_sub_type"])
}

func TestEnrichDNSFields_Event260_ServerIPFallback(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer", serverIP: "10.0.0.1"}
	fields := map[string]string{
		"interface_ip": "0.0.0.0",
		"destination":  "8.8.8.8",
	}
	d.enrichDNSFields(fields, 260)

	assert.Equal(t, "10.0.0.1", fields["src_ip_addr"], "should fallback to cached serverIP")
	assert.Equal(t, "8.8.8.8", fields["dst_ip_addr"])
}

func TestEnrichDNSFields_Event261(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{
		"source":       "8.8.8.8",
		"interface_ip": "10.0.0.1",
	}
	d.enrichDNSFields(fields, 261)

	assert.Equal(t, "8.8.8.8", fields["src_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dst_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dvc_ip_addr"])
	assert.Equal(t, "response", fields["event_sub_type"])
}

func TestEnrichDNSFields_Event261_ServerIPFallback(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer", serverIP: "10.0.0.1"}
	fields := map[string]string{
		"source":       "8.8.8.8",
		"interface_ip": "0.0.0.0",
	}
	d.enrichDNSFields(fields, 261)

	assert.Equal(t, "8.8.8.8", fields["src_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dst_ip_addr"], "should fallback to cached serverIP")
}

func TestEnrichDNSFields_Event279(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{
		"interface_ip": "10.0.0.1",
		"source":       "192.168.1.100",
		"port":         "53214",
	}
	d.enrichDNSFields(fields, 279)

	assert.Equal(t, "10.0.0.1", fields["src_ip_addr"])
	assert.Equal(t, "10.0.0.1", fields["dvc_ip_addr"])
	assert.Equal(t, "192.168.1.100", fields["dst_ip_addr"])
	assert.Equal(t, "53214", fields["dst_port_number"])
	assert.Equal(t, "response", fields["event_sub_type"])
}

func TestEnrichDNSFields_Event279_ServerIPFallback(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer", serverIP: "10.0.0.1"}
	fields := map[string]string{
		"interface_ip": "0.0.0.0",
		"source":       "192.168.1.100",
		"port":         "53214",
	}
	d.enrichDNSFields(fields, 279)

	assert.Equal(t, "10.0.0.1", fields["src_ip_addr"], "should fallback to cached serverIP")
}

func TestEnrichDNSFields_QNAME_Trimming(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{"qname": "www.example.com."}
	d.enrichDNSFields(fields, 256)
	assert.Equal(t, "www.example.com", fields["dns_query"])
}

func TestEnrichDNSFields_QTYPE_Mapping(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	tests := []struct {
		qtype    string
		expected string
	}{
		{"1", "A"},
		{"28", "AAAA"},
		{"5", "CNAME"},
		{"15", "MX"},
		{"999", "TYPE999"},
	}
	for _, tt := range tests {
		fields := map[string]string{"qtype": tt.qtype}
		d.enrichDNSFields(fields, 256)
		assert.Equal(t, tt.expected, fields["dns_query_type_name"])
	}
}

func TestEnrichDNSFields_Hostname(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer", hostname: "my-dns-server"}
	fields := map[string]string{}
	d.enrichDNSFields(fields, 256)
	assert.Equal(t, "my-dns-server", fields["dvc_hostname"])
}

func TestEnrichDNSFields_NoHostname(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.enrichDNSFields(fields, 256)
	_, exists := fields["dvc_hostname"]
	assert.False(t, exists, "dvc_hostname should not be set when hostname is empty")
}

func TestNormalizeDNSNetworkProtocol(t *testing.T) {
	assert.Equal(t, "udp", normalizeDNSNetworkProtocol("0"))
	assert.Equal(t, "tcp", normalizeDNSNetworkProtocol("1"))
	assert.Equal(t, "udp", normalizeDNSNetworkProtocol("UDP"))
	assert.Equal(t, "tcp", normalizeDNSNetworkProtocol("true"))
	assert.Empty(t, normalizeDNSNetworkProtocol(""))
	assert.Empty(t, normalizeDNSNetworkProtocol("unknown"))
}

func TestEnrichDNSFields_ServerIPCacheFlow(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}

	// Event 256 caches serverIP
	fields256 := map[string]string{
		"source":       "192.168.1.100",
		"interface_ip": "10.0.0.1",
	}
	d.enrichDNSFields(fields256, 256)
	assert.Equal(t, "10.0.0.1", d.serverIP)

	// Event 260 with 0.0.0.0 falls back to cached serverIP
	fields260 := map[string]string{
		"interface_ip": "0.0.0.0",
		"destination":  "8.8.8.8",
	}
	d.enrichDNSFields(fields260, 260)
	assert.Equal(t, "10.0.0.1", fields260["src_ip_addr"])

	// Event 261 with 0.0.0.0 falls back to cached serverIP
	fields261 := map[string]string{
		"source":       "8.8.4.4",
		"interface_ip": "0.0.0.0",
	}
	d.enrichDNSFields(fields261, 261)
	assert.Equal(t, "10.0.0.1", fields261["dst_ip_addr"])

	// Event 279 with 0.0.0.0 falls back to cached serverIP
	fields279 := map[string]string{
		"interface_ip": "0.0.0.0",
		"source":       "192.168.1.100",
		"port":         "12345",
	}
	d.enrichDNSFields(fields279, 279)
	assert.Equal(t, "10.0.0.1", fields279["src_ip_addr"])
}

func TestParseDNSPacketData_Response(t *testing.T) {
	// A minimal DNS response: query for example.com, answer 93.184.216.34
	// Header: ID=0x1234, Flags=0x8180 (response, no error), QDCOUNT=1, ANCOUNT=1
	// Question: example.com A IN
	// Answer: example.com A 93.184.216.34
	packetHex := "1234818000010001000000000765" +
		"78616d706c6503636f6d0000010001" +
		"c00c000100010000003c00045db8d822"

	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.parseDNSPacketData(fields, packetHex, 279)

	assert.Equal(t, "0", fields["dns_response_code"])
	assert.Equal(t, "NOERROR", fields["dns_response_code_name"])
	assert.Equal(t, "Success", fields["event_result"])
	assert.Equal(t, "NOERROR", fields["event_result_details"])
	assert.Equal(t, "0x8180", fields["dns_flags"])
	assert.Contains(t, fields["dns_response_name"], "93.184.216.34")
}

func TestParseDNSPacketData_NXDomain(t *testing.T) {
	// DNS response with NXDOMAIN (rcode=3)
	// Header: ID=0xABCD, Flags=0x8183 (response, NXDOMAIN), QDCOUNT=1, ANCOUNT=0
	packetHex := "abcd81830001000000000000" +
		"096e6f6e657869737473" +
		"076578616d706c6503636f6d0000010001"

	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.parseDNSPacketData(fields, packetHex, 279)

	assert.Equal(t, "3", fields["dns_response_code"])
	assert.Equal(t, "NXDOMAIN", fields["dns_response_code_name"])
	assert.Equal(t, "Failure", fields["event_result"])
	assert.Equal(t, "NXDOMAIN", fields["event_result_details"])
}

func TestParseDNSPacketData_HexPrefix(t *testing.T) {
	// Same packet with 0x prefix and spaces — should still parse
	packetHex := "0x 1234 8180 0001 0001 0000 0000 0765" +
		"78616d706c6503636f6d0000010001" +
		"c00c000100010000003c00045db8d822"

	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.parseDNSPacketData(fields, packetHex, 279)

	assert.Equal(t, "NOERROR", fields["dns_response_code_name"])
}

func TestParseDNSPacketData_InvalidHex(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.parseDNSPacketData(fields, "not-valid-hex", 279)
	_, exists := fields["dns_response_code"]
	assert.False(t, exists, "should not set fields on invalid hex")
}

func TestParseDNSPacketData_TooShort(t *testing.T) {
	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.parseDNSPacketData(fields, "1234", 279)
	_, exists := fields["dns_response_code"]
	assert.False(t, exists, "should not set fields when packet too short")
}

func TestParseDNSPacketData_RequestNoFlags(t *testing.T) {
	// A DNS query (not response): Flags=0x0100 (recursion desired, QR=0)
	packetHex := "1234010000010000000000000765" +
		"78616d706c6503636f6d0000010001"

	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{}
	d.parseDNSPacketData(fields, packetHex, 256)

	_, hasResponseCode := fields["dns_response_code"]
	assert.False(t, hasResponseCode, "dns_response_code should not be set for query packets")
	_, hasdns_flags := fields["dns_flags"]
	assert.False(t, hasdns_flags, "dns_flags should not be set for query packets")
}

func TestEnrichDNSFields_RequestPacketKeepsNA(t *testing.T) {
	packetHex := "1234010000010000000000000765" +
		"78616d706c6503636f6d0000010001"

	d := &EtwInput{ProviderName: "Microsoft-Windows-DNSServer"}
	fields := map[string]string{"packet_data": packetHex}
	d.enrichDNSFields(fields, 256)

	assert.Equal(t, "NA", fields["event_result_details"])
	_, hasResponseCode := fields["dns_response_code"]
	assert.False(t, hasResponseCode, "request packets should not set response code")
}

func TestFormatRR_A(t *testing.T) {
	rr, _ := dns.NewRR("example.com. 60 IN A 1.2.3.4")
	assert.Equal(t, "1.2.3.4", formatRR(rr))
}

func TestFormatRR_AAAA(t *testing.T) {
	rr, _ := dns.NewRR("example.com. 60 IN AAAA ::1")
	assert.Equal(t, "::1", formatRR(rr))
}

func TestFormatRR_CNAME(t *testing.T) {
	rr, _ := dns.NewRR("www.example.com. 60 IN CNAME example.com.")
	assert.Equal(t, "example.com", formatRR(rr))
}

func TestFormatRR_MX(t *testing.T) {
	rr, _ := dns.NewRR("example.com. 60 IN MX 10 mail.example.com.")
	assert.Equal(t, "10 mail.example.com", formatRR(rr))
}

func TestFormatRR_NS(t *testing.T) {
	rr, _ := dns.NewRR("example.com. 60 IN NS ns1.example.com.")
	assert.Equal(t, "ns1.example.com", formatRR(rr))
}

func TestFormatRR_PTR(t *testing.T) {
	rr, _ := dns.NewRR("4.3.2.1.in-addr.arpa. 60 IN PTR host.example.com.")
	assert.Equal(t, "host.example.com", formatRR(rr))
}

func TestFormatRR_TXT(t *testing.T) {
	rr, _ := dns.NewRR(`example.com. 60 IN TXT "v=spf1 include:example.com"`)
	assert.Equal(t, "v=spf1 include:example.com", formatRR(rr))
}

func TestFormatRR_SOA(t *testing.T) {
	rr, _ := dns.NewRR("example.com. 60 IN SOA ns1.example.com. admin.example.com. 2021010100 3600 900 604800 86400")
	assert.Equal(t, "ns1.example.com admin.example.com", formatRR(rr))
}

func TestFormatRR_SRV(t *testing.T) {
	rr, _ := dns.NewRR("_sip._tcp.example.com. 60 IN SRV 10 60 5060 sip.example.com.")
	assert.Equal(t, "10 60 5060 sip.example.com", formatRR(rr))
}
