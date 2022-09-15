# go-dogstatsd-parser
[![Build Status](https://travis-ci.org/narqo/go-dogstatsd-parser.svg?branch=master)](https://travis-ci.org/narqo/go-dogstatsd-parser)
[![GoDoc](https://godoc.org/github.com/narqo/go-dogstatsd-parser?status.svg)](https://godoc.org/github.com/narqo/go-dogstatsd-parser)

A standalone parser for [DogStatsD] metrics protocol.

## Examples

Parsing simple StatsD metric:

~~~go
m, err := dogstatsd.Parse("page.views:1|c")
if err != nil {
    log.Fatal(err)
}
fmt.Println(m.Name)
fmt.Println(m.Value)
~~~

Parsing extended StatsD (DogStatsD, as described by Datadog Datagram Format) metric:

~~~go
m, err := dogstatsd.Parse("users.online:1|c|#country:china,city:beijing")
if err != nil {
    log.Fatal(err)
}
fmt.Println(m.Name)
fmt.Println(m.Value)
for k, v := range m.Tags {
    fmt.Println(k + " - " + v)
}
~~~

## Installation

~~~
go get github.com/narqo/go-dogstatsd-parser
~~~

## License

MIT

[DogStatsD]: http://docs.datadoghq.com/guides/dogstatsd/
