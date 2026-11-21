# MU1005 R48 — Native SD Transaction Controller, cloud review ONLY

Recovered from the user's already-supplied `AU57x_Wireless_Consolidated.zip`,
`source/install_native/`. The original C implementation is retained here
as an auditable **review-only**, source-available research candidate.

Do not run on a real car. There is no verified MU1005 writable filesystem
mapping, boot-time maintenance service, original-process quiesce function,
real candidate health probe, recovery entry point, or power-loss/flash medium
qualification. Nothing here constitutes a ready-to-install firmware update.

`bash ci/host_sd_txn.sh` from the repo root executes the original
67 Linux host tests with throwaway temporary directories. The important
logic includes: exclusive maintenance lock, initial OEM SHA checks, atomic
backup and journal, quiesce before file replacement, health-based commit,
independent cold recovery, failed-stop blocking and return to original service.
Host filesystem behavior does NOT establish identical QNX-on-car persistence.

`tools/cloud_sd_transaction_sim.py` remains as a separate deliberately
low-privilege fake-file simulator. The native controller has no production
entrypoint, install manifest or OEM payload; **installable=false**.
