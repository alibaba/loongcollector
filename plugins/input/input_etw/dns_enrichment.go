//go:build windows
// +build windows

package input_etw

import (
	"encoding/hex"
	"fmt"
	"strings"

	"github.com/miekg/dns"
)

var dnsQueryTypeMap = map[string]string{
	"1": "A", "2": "NS", "5": "CNAME", "6": "SOA",
	"12": "PTR", "15": "MX", "16": "TXT", "28": "AAAA",
	"33": "SRV", "35": "NAPTR", "43": "DS", "46": "RRSIG",
	"47": "NSEC", "48": "DNSKEY", "50": "NSEC3", "52": "TLSA",
	"65": "HTTPS", "99": "SPF", "252": "AXFR", "255": "ANY",
	"257": "CAA",
}

var dnsResponseCodeMap = map[string]string{
	"0": "NOERROR", "1": "FORMERR", "2": "SERVFAIL",
	"3": "NXDOMAIN", "4": "NOTIMP", "5": "REFUSED",
	"6": "YXDOMAIN", "7": "YXRRSET", "8": "NXRRSET",
	"9": "NOTAUTH",
}

func mapDNSQueryType(qtype string) string {
	if name, ok := dnsQueryTypeMap[qtype]; ok {
		return name
	}
	return "TYPE" + qtype
}

func mapDNSResponseCode(rcode int) string {
	key := fmt.Sprintf("%d", rcode)
	if name, ok := dnsResponseCodeMap[key]; ok {
		return name
	}
	return fmt.Sprintf("RCODE%d", rcode)
}

func (d *EtwInput) isDNSProvider() bool {
	return d.ProviderName == "Microsoft-Windows-DNSServer" ||
		strings.EqualFold(d.ProviderGUID, "{EB79061A-A566-4698-9119-3ED2807060E7}")
}

func (d *EtwInput) enrichDNSFields(fields map[string]string, eventID uint16) {
	switch eventID {
	case 256: // QUERY_RECEIVED
		if src, ok := fields["source"]; ok && src != "" && src != "0.0.0.0" {
			fields["src_ip_addr"] = src
		}
		if dst, ok := fields["interface_ip"]; ok && dst != "" && dst != "0.0.0.0" {
			fields["dst_ip_addr"] = dst
			fields["dvc_ip_addr"] = dst
			if dst != "127.0.0.1" {
				d.serverIP = dst
			}
		}
		if port, ok := fields["port"]; ok && port != "" {
			fields["src_port_number"] = port
		}
		fields["event_sub_type"] = "request"

	case 260: // RECURSE_QUERY_OUT
		if dst, ok := fields["destination"]; ok && dst != "" && dst != "0.0.0.0" {
			fields["dst_ip_addr"] = dst
		}
		if src, ok := fields["interface_ip"]; ok && src != "" && src != "0.0.0.0" {
			fields["src_ip_addr"] = src
			fields["dvc_ip_addr"] = src
		} else if d.serverIP != "" {
			fields["src_ip_addr"] = d.serverIP
			fields["dvc_ip_addr"] = d.serverIP
		}
		fields["event_sub_type"] = "request"

	case 261: // RECURSE_RESPONSE_IN
		if src, ok := fields["source"]; ok && src != "" && src != "0.0.0.0" {
			fields["src_ip_addr"] = src
		}
		if dst, ok := fields["interface_ip"]; ok && dst != "" && dst != "0.0.0.0" {
			fields["dst_ip_addr"] = dst
			fields["dvc_ip_addr"] = dst
		} else if d.serverIP != "" {
			fields["dst_ip_addr"] = d.serverIP
			fields["dvc_ip_addr"] = d.serverIP
		}
		fields["event_sub_type"] = "response"

	case 279: // RESPONSE_TO_CLIENT
		if src, ok := fields["interface_ip"]; ok && src != "" && src != "0.0.0.0" {
			fields["src_ip_addr"] = src
			fields["dvc_ip_addr"] = src
		} else if d.serverIP != "" {
			fields["src_ip_addr"] = d.serverIP
			fields["dvc_ip_addr"] = d.serverIP
		}
		if dst, ok := fields["source"]; ok && dst != "" && dst != "0.0.0.0" {
			fields["dst_ip_addr"] = dst
		}
		if port, ok := fields["port"]; ok && port != "" {
			fields["dst_port_number"] = port
		}
		fields["event_sub_type"] = "response"
	}

	if qname, ok := fields["qname"]; ok && qname != "" {
		fields["dns_query"] = strings.TrimSuffix(qname, ".")
	}

	if qtype, ok := fields["qtype"]; ok && qtype != "" {
		fields["dns_query_type_name"] = mapDNSQueryType(qtype)
	}

	if d.hostname != "" {
		fields["dvc_hostname"] = d.hostname
	}
	if d.domain != "" {
		fields["dvc_domain"] = d.domain
	}
	if d.domainType != "" {
		fields["dvc_domain_type"] = d.domainType
	}
	if d.osName != "" {
		fields["dvc_os"] = d.osName
	}
	if d.osVersion != "" {
		fields["dvc_os_version"] = d.osVersion
	}

	if protocol := normalizeDNSNetworkProtocol(fields["tcp"]); protocol != "" {
		fields["network_protocol"] = protocol
	}
	if _, ok := fields["event_result_details"]; !ok {
		fields["event_result_details"] = "NA"
	}

	if packetHex, ok := fields["packet_data"]; ok && packetHex != "" {
		d.parseDNSPacketData(fields, packetHex, eventID)
	}
}

func normalizeDNSNetworkProtocol(tcp string) string {
	switch strings.ToLower(strings.TrimSpace(tcp)) {
	case "0", "false", "udp":
		return "udp"
	case "1", "true", "tcp":
		return "tcp"
	default:
		return ""
	}
}

func (d *EtwInput) parseDNSPacketData(fields map[string]string, packetHex string, eventID uint16) {
	cleaned := strings.ReplaceAll(packetHex, " ", "")
	cleaned = strings.ReplaceAll(cleaned, "-", "")
	cleaned = strings.TrimPrefix(cleaned, "0x")
	cleaned = strings.TrimPrefix(cleaned, "0X")

	raw, err := hex.DecodeString(cleaned)
	if err != nil || len(raw) < 12 {
		return
	}

	msg := new(dns.Msg)
	if err := msg.Unpack(raw); err != nil {
		return
	}

	rcode := msg.Rcode
	fields["dns_response_code"] = fmt.Sprintf("%d", rcode)
	rcodeName := mapDNSResponseCode(rcode)
	fields["dns_response_code_name"] = rcodeName
	fields["event_result_details"] = rcodeName

	if rcode == dns.RcodeSuccess {
		fields["event_result"] = "Success"
	} else {
		fields["event_result"] = "Failure"
	}

	if msg.Response {
		fields["dns_flags"] = fmt.Sprintf("0x%04X", uint16(raw[2])<<8|uint16(raw[3]))
	}

	if eventID == 261 || eventID == 279 || msg.Response {
		var answers []string
		for _, rr := range msg.Answer {
			answers = append(answers, formatRR(rr))
		}
		if len(answers) > 0 {
			fields["dns_response_name"] = strings.Join(answers, "; ")
		}
	}
}

func formatRR(rr dns.RR) string {
	switch v := rr.(type) {
	case *dns.A:
		return v.A.String()
	case *dns.AAAA:
		return v.AAAA.String()
	case *dns.CNAME:
		return strings.TrimSuffix(v.Target, ".")
	case *dns.MX:
		return fmt.Sprintf("%d %s", v.Preference, strings.TrimSuffix(v.Mx, "."))
	case *dns.NS:
		return strings.TrimSuffix(v.Ns, ".")
	case *dns.PTR:
		return strings.TrimSuffix(v.Ptr, ".")
	case *dns.TXT:
		return strings.Join(v.Txt, " ")
	case *dns.SOA:
		return fmt.Sprintf("%s %s", strings.TrimSuffix(v.Ns, "."), strings.TrimSuffix(v.Mbox, "."))
	case *dns.SRV:
		return fmt.Sprintf("%d %d %d %s", v.Priority, v.Weight, v.Port, strings.TrimSuffix(v.Target, "."))
	default:
		hdr := rr.Header()
		return fmt.Sprintf("%s %s", dns.TypeToString[hdr.Rrtype], rr.String())
	}
}
