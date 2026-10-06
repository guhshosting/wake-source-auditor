# JSON schema 1.0

> Release context: see [qualified platforms, runtime requirements and package scope](../RELEASE-QUALIFICATION.md).


One UTF-8 object, no BOM. `schema_version` is `1.0`; `application_version` is `1.0.0`. Consumers should tolerate additive fields. Unknown/unavailable booleans and values are `null`, not false or empty substitutes. Collected empty lists do not imply complete coverage; inspect acquisition status and limitations.

| Key | Meaning |
| --- | --- |
| audit_status | completed or required_source_failure |
| power_capabilities | acquisition, nullable S1/S2/S3/S4/hibernation-file/AoAc/wake-alarm booleans, category and explanation |
| last_wake | acquisition, lookback_days=30, max_events=32, nullable event |
| last_wake.event | reported_wake_event, source, provider, event_id and record_id as strings, timestamp_utc, available named fields, explanation |
| devices | acquisition and items; capability and enabled observations remain separate |
| devices.items[] | identity, identity_kind, name, class, evidence_category, nullable wake_capable/wake_enabled, optional numeric power_capability_flags, source |
| timers | unavailable status, unavailable_unknown category, null items, reason |
| wake_enabled_tasks | acquisition and items |
| wake_enabled_tasks.items[] | path, wake_to_run=true, enabled, nullable next_run_local, configured_wake_behavior category, source, proves_wake=false |
| warnings / limitations | arrays of strings |

Acquisition objects contain `status` and `source`. Status: `ok` (query completed within API coverage), `partial` (failure or budget exhaustion), `unavailable` (source could not initialize), `not_requested`. Power values are null unless acquisition succeeded. Null event means no usable event acquired; inspect acquisition to distinguish empty history from failure. UTC timestamps are preserved from Windows events. Task next_run_local is a local wall-clock ISO-like timestamp without UTC offset; null includes unscheduled tasks and failed next-run retrieval. It is not a timer expiration promise.

Evidence categories are `reported_wake_event`, `configured_capability`, `configured_wake_behavior`, and `unavailable_unknown`. This release makes no causal `possible_contributor` correlation and emits no active_timer records because it does not acquire active kernel timers. A configured task/device could be a contributor, but this tool does not establish that connection.
