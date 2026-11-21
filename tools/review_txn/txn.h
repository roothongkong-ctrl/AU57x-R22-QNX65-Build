#ifndef TXN_H
#define TXN_H
#include <sys/types.h>
#include <stddef.h>
/* Caller supplies immutable release manifest, exclusive maintenance, trusted dirs.
 * All names are single components. Backups and journal on durable media.
 * Directory fsync is mandatory; unsupported FS fails closed. No production caller. */
typedef struct { const char *name; const char *old_sha,*new_sha; mode_t old_mode,new_mode; uid_t uid; gid_t gid; } txn_row;
typedef struct { const char *target,*backup,*payload,*state; const txn_row *rows; size_t count; void (*checkpoint)(const char *,size_t); } txn_spec;
/* trial = prepare+apply under one lock; writes PENDING_HEALTH, not COMMITTED.
 * confirm: trusted maintenance controller only, after independent health checks.
 * recover: rolls back PENDING_HEALTH and all incomplete replacement phases.
 * status: outputs journal phase, not live service health. Legacy apply retained
 * for historical host tests only; not a deployment entry point. */
int txn_run(const txn_spec *s,const char *action);
/* All callbacks run under the transaction maintenance lock. They must be
 * bounded, independently verify their result, and must not recurse into txn.
 * quiesce must prove services stopped before touching installed files.
 * start_original/start_candidate are distinct lifecycle operations; verify
 * must validate actual candidate health, not simply process existence.
 * No production callbacks or QNX startup integration are provided. */
typedef struct {
 void *ctx;
 int (*quiesce)(void *);
 int (*start_candidate)(void *);
 int (*verify_candidate)(void *);
 int (*start_original)(void *);
} txn_lifecycle;
enum { TXN_LIFE_COMMITTED=0, TXN_LIFE_ROLLED_BACK=1, TXN_LIFE_BLOCKED=-1 };
int txn_supervised_trial(const txn_spec *s,const txn_lifecycle *life);
int txn_supervised_recover(const txn_spec *s,const txn_lifecycle *life);
/* Explicit restore after successful install; precheck backup before stopping.
 * Recovery never reads payload, but retains its original path in manifest ID. */
int txn_supervised_restore(const txn_spec *s,const txn_lifecycle *life);
/* Early boot only: caller must control OEM/candidate autostart, quiesce
 * proves exclusivity. COMMITTED is re-armed as PENDING_HEALTH before each
 * candidate start; reboot during health check selects OEM next boot.
 * ROLLED_BACK also means verified original startup when no trial exists.
 * No production boot hook or service callbacks are provided. */
int txn_supervised_boot(const txn_spec *s,const txn_lifecycle *life);
#endif
