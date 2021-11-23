# Apache SkyWalking data collect protocol
Apache SkyWalking typically collect data from 
1. Traces
2. Metrics(Meter system)
3. Logs
4. Command data. Push the commands to the agents from Server.
5. Event. 

This repo hosts the protocol of SkyWalking native report protocol, defined in gRPC. Read [Protocol DOC](https://github.com/apache/skywalking/blob/master/docs/en/protocols/README.md#probe-protocols) for more details

## Release
This repo wouldn't release separately. All source codes have been included in the main repo release. The tags match the [main repo](https://github.com/apache/skywalking) tags.

## License
Apache 2.0
