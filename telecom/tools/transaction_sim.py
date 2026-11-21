#!/usr/bin/env python3
"""Dry-run-only SD transaction/rollback model.

This Python model runs on GitHub's Ubuntu runner in a temporary fixture.
It is NOT QNX car code and MUST NOT be copied onto a head unit as installer.
"""
import hashlib
import json
import os
from pathlib import Path


class TransactionError(RuntimeError):
    pass


def sha(data):
    return hashlib.sha256(data).hexdigest()


def _write_sync_exclusive(path, data):
    with path.open("xb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())


def simulate_update(target, candidate, backup, *,
                    expected_original_sha, expected_candidate_sha,
                    fault=None):
    """Never touch anything except the temporary fixture caller supplies.

    Tests simulate an OEM file transition and rollback on fault.
    Real QNX filesystem behavior, ownership, mount state, power-loss safety
    and actual repair semantics are outside the model.
    """
    target, backup, candidate = map(Path, (target, backup, candidate))
    stage = target.with_name(target.name + ".au57x.stage")
    pending = target.with_name(target.name + ".au57x.pending")
    for path in (target, candidate, backup, stage, pending):
        if path.is_symlink():
            raise TransactionError("REFUSE_SYMLINK")
    if pending.exists():
        raise TransactionError("PREVIOUS_TRANSACTION_MUST_BE_RECOVERED")
    if stage.exists():
        raise TransactionError("PREVIOUS_STAGE_MUST_BE_REVIEWED")
    old = target.read_bytes()
    new = candidate.read_bytes()
    if sha(old) != expected_original_sha:
        raise TransactionError("ORIGINAL_IDENTITY_MISMATCH")
    if sha(new) != expected_candidate_sha or old == new:
        raise TransactionError("CANDIDATE_INVALID")
    if backup.exists():
        if sha(backup.read_bytes()) != expected_original_sha:
            raise TransactionError("BACKUP_IDENTITY_MISMATCH")
    else:
        _write_sync_exclusive(backup, old)
    if sha(backup.read_bytes()) != expected_original_sha:
        raise TransactionError("BACKUP_VERIFICATION_FAILED")
    if fault == "after_backup":
        raise TransactionError("FAULT_AFTER_BACKUP_NO_ORIGINAL_WRITE")
    written = False
    try:
        _write_sync_exclusive(stage, new)
        if sha(stage.read_bytes()) != expected_candidate_sha:
            raise TransactionError("STAGING_HASH_MISMATCH")
        if fault == "after_stage":
            raise TransactionError("FAULT_AFTER_STAGE")
        _write_sync_exclusive(pending,
                json.dumps({"factory_sha256": expected_original_sha,
                            "candidate_sha256": expected_candidate_sha}).encode())
        if fault == "after_journal":
            raise TransactionError("FAULT_AFTER_JOURNAL")
        os.replace(stage, target)
        written = True
        if fault == "after_replace":
            raise TransactionError("FAULT_AFTER_REPLACE")
        if sha(target.read_bytes()) != expected_candidate_sha:
            raise TransactionError("FINAL_HASH_MISMATCH")
        if fault == "after_verify":
            raise TransactionError("FAULT_AFTER_VERIFY")
        pending.unlink()
        return {"result": "SIMULATED_APPLY_OK", "target_sha": expected_candidate_sha}
    except Exception:
        # A controlled exception can trigger rollback. Power loss requires
        # an independent recovery invocation; never claim auto-reboot recovery.
        if written:
            restore = target.with_name(target.name + ".au57x.rollback_stage")
            if restore.exists() or restore.is_symlink():
                raise TransactionError("UNSAFE_ROLLBACK_STAGE")
            _write_sync_exclusive(restore, backup.read_bytes())
            os.replace(restore, target)
            if sha(target.read_bytes()) != expected_original_sha:
                raise TransactionError("SIMULATED_ROLLBACK_FAILED")
        if stage.exists():
            stage.unlink()
        if pending.exists():
            pending.unlink()
        raise


def simulate_power_loss(target, candidate, backup, *, original_sha, candidate_sha):
    """Mock hard crash immediately after replacement, leaving journal.

    Deliberately separate from live car process behavior.
    """
    target, candidate, backup = map(Path, (target, candidate, backup))
    if any(p.is_symlink() for p in (target, candidate, backup)):
        raise TransactionError("REFUSE_SYMLINK")
    if sha(target.read_bytes()) != original_sha or sha(candidate.read_bytes()) != candidate_sha:
        raise TransactionError("IDENTITY_MISMATCH")
    if backup.exists() or target.with_name(target.name + ".au57x.pending").exists():
        raise TransactionError("UNSAFE_EXISTING_BACKUP_OR_JOURNAL")
    _write_sync_exclusive(backup, target.read_bytes())
    _write_sync_exclusive(target.with_name(target.name + ".au57x.pending"),
                          json.dumps({"factory_sha256": original_sha}).encode())
    os.replace(candidate, target)  # fixture file only
    return "SIMULATED_UNCLEAN_SHUTDOWN"


def recover_pending(target, backup, expected_original_sha):
    target, backup = Path(target), Path(backup)
    journal = target.with_name(target.name + ".au57x.pending")
    if any(p.is_symlink() for p in (target, backup, journal)):
        raise TransactionError("REFUSE_SYMLINK")
    if not journal.is_file():
        raise TransactionError("NO_PENDING_JOURNAL")
    j = json.loads(journal.read_text())
    if j.get("factory_sha256") != expected_original_sha:
        raise TransactionError("JOURNAL_ORIGINAL_ID_MISMATCH")
    if sha(backup.read_bytes()) != expected_original_sha:
        raise TransactionError("BACKUP_IDENTITY_MISMATCH")
    stage = target.with_name(target.name + ".au57x.recover")
    if stage.exists():
        raise TransactionError("RECOVERY_STAGE_EXISTS")
    _write_sync_exclusive(stage, backup.read_bytes())
    os.replace(stage, target)
    if sha(target.read_bytes()) != expected_original_sha:
        raise TransactionError("RECOVERY_FAILED")
    journal.unlink()
    return "SIMULATED_RECOVERY_OK"
