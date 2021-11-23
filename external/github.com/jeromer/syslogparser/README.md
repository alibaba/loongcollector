Syslogparser
============

This is a syslog parser for the Go programming language.

Installing
----------

go get github.com/jeromer/syslogparser

Supported RFCs
--------------

- [RFC 3164][RFC 3164]
- [RFC 5424][RFC 5424]

Not all features described in RFCs above are supported but only the most
part of it. For exaple `SDID`s are not supported in [RFC 5424][RFC 5424] and
`STRUCTURED-DATA` are parsed as a whole string.

This parser should solve 80% of use cases. If your use cases are in the
20% remaining ones I would recommend you to fully test what you want to
achieve and provide a patch if you want.

Parsing an RFC 3164 syslog message
----------------------------------

	b := "<34>Oct 11 22:14:15 mymachine su: 'su root' failed for lonvick on /dev/pts/8"
	buff := []byte(b)

	p := rfc3164.NewParser(buff)
	err := p.Parse()
	if err != nil {
		panic(err)
	}

	for k, v := range p.Dump() {
		fmt.Println(k, ":", v)
	}

You should see

    timestamp : 2013-10-11 22:14:15 +0000 UTC
    hostname  : mymachine
    tag       : su
    content   : 'su root' failed for lonvick on /dev/pts/8
    priority  : 34
    facility  : 4
    severity  : 2

Parsing an RFC 5424 syslog message
----------------------------------

	b := `<165>1 2003-10-11T22:14:15.003Z mymachine.example.com evntslog - ID47 [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"] An application event log entry...`
	buff := []byte(b)

	p := rfc5424.NewParser(buff)
	err := p.Parse()
	if err != nil {
		panic(err)
	}

	for k, v := range p.Dump() {
		fmt.Println(k, ":", v)
	}

You should see

    version : 1
    timestamp : 2003-10-11 22:14:15.003 +0000 UTC
    app_name : evntslog
    msg_id : ID47
    message : An application event log entry...
    priority : 165
    facility : 20
    severity : 5
    hostname : mymachine.example.com
    proc_id : -
    structured_data : [exampleSDID@32473 iut="3" eventSource="Application" eventID="1011"]

Running tests
-------------

Run `make tests`

Running benchmarks
------------------

Run `make benchmarks`

    CommonTestSuite.BenchmarkParsePriority   20000000  85.9 ns/op
    CommonTestSuite.BenchmarkParseVersion    500000000 4.59 ns/op
    Rfc3164TestSuite.BenchmarkParseFull      10000000  187 ns/op
    Rfc3164TestSuite.BenchmarkParseHeader    5000000   686 ns/op
    Rfc3164TestSuite.BenchmarkParseHostname  50000000  43.4 ns/op
    Rfc3164TestSuite.BenchmarkParseTag       50000000  63.5 ns/op
    Rfc3164TestSuite.BenchmarkParseTimestamp 5000000   616 ns/op
    Rfc3164TestSuite.BenchmarkParsemessage   10000000  187 ns/op
    Rfc5424TestSuite.BenchmarkParseFull      1000000   1345 ns/op
    Rfc5424TestSuite.BenchmarkParseHeader    1000000   1353 ns/op
    Rfc5424TestSuite.BenchmarkParseTimestamp 1000000   2045 ns/op

[RFC 5424]: https://tools.ietf.org/html/rfc5424
[RFC 3164]: https://tools.ietf.org/html/rfc3164
