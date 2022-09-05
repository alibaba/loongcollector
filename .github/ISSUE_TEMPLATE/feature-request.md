---
name: Feature request
about: New feature request
title: "[FEATURE]:"
labels: 'feature request'
assignees: ''

---

**Concisely describe the proposed feature**
A clear and concise description of what you want. For example,
> I would like to add an input plugin to collect logs by [SNMP protocol](https://en.wikipedia.org/wiki/Simple_Network_Management_Protocol) so that my network devices' logs will be collected.

**Describe the solution you'd like (if any)**
A clear and concise description of what you want to achieve and implement. For example,
> We can use the package `github.com/gosnmp/gosnmp` to act as an snmp server in a gesture to collect client logs.

**Additional comments**
Add any other context or screenshots about the feature request here. 
For example, the ideal input and output logs.
> The logs are supposed to have the following contents, e.g.,
>
> ```json
> {"_target_":"127.0.0.1","_field_":"DISMAN-EXPRESSION-MIB::sysUpTimeInstance","_oid_":".1.3.6.1.2.1.1.3.0","_type_":"TimeTicks","_content_":"10423593"}
> ```
