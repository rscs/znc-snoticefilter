# Server Notice Filter for ZNC (snoticefilter)

Filter (block) certain server notices (mostly intended for IRC Operators) from being sent to identified clients based on regular expression patterns. A typical use case is to have a subset of server notices visible to a mobile client.

Filters are shared across all filter-enabled clients. Per-client filtering will be made available in a future release.

**Requires ZNC version 1.7.0 or later.**

## Commands

Add a client:

`/msg *snoticefilter AddClient <identifier>`

Delete a client:

`/msg *snoticefilter DelClient <identifier>`

List all known clients and show their filter status:

`/msg *snoticefilter ListClients`

Turn filtering on or off for the current client or the specified client:

`/msg *snoticefilter ToggleFilter [<identifier>]`

Add a filter:

`/msg *snoticefilter AddFilter <regex pattern>`

Delete a filter:

`/msg *snoticefilter DelFilter <number>`

List all filters:

`/msg *snoticefilter ListFilters`

