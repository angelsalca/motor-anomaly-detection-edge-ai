# Next improvements

These are useful improvements if the project is revisited; they should be presented as **post-project work**, not retroactively claimed as part of the original ITS implementation.

1. Run the Raspberry reconstruction with a local Mosquitto instance and create a reproducible demo using simulated MQTT messages.
2. Add a small Python test suite for payload parsing and SQLite persistence.
3. Replace hard-coded deployment paths with command-line/config options.
4. Add a dashboard screenshot from a real local run.
5. If the physical bench becomes available again, validate the recovered LIS2DW12 + NanoEdge firmware and correct the FAULT flow without forcing similarity values.
6. Archive raw sensor datasets and record the NanoEdge model version/parameters.
