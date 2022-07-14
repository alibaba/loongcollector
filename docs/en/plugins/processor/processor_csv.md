# processor_csv
## Description
csv decoder for logtail
## Config
|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |
|SourceKey|string|The source key containing the CSV record|""|
|NoKeyError|bool|Optional. Whether to report error if no key in the log mathes the SourceKey, default to false|false|
|SplitKeys|[]string|The keys matching the decoded CSV fields|null|
|SplitSep|string|Optional. The Seperator, default to ,|","|
|TrimLeadingSpace|bool|Optional. Whether to ignore the leading space in each CSV field, default to false|false|
|PreserveOthers|bool|Optional. Whether to preserve the remaining record if #splitKeys < #CSV fields, default to false|false|
|ExpandOthers|bool|Optional. Whether to decode the remaining record if #splitKeys < #CSV fields, default to false|false|
|ExpandKeyPrefix|string|Required when ExpandOthers=true. The prefix of the keys for storing the remaing record fields|""|
|KeepSource|bool|Optional. Whether to keep the source log content given successful decoding, default to false|false|