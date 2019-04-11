/*-------------------------------------------------------------------------
 *
 * snapshot.h
 *	  POSTGRES snapshot definition
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/snapshot.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include "access/htup.h"
#include "access/xlogdefs.h"
#include "datatype/timestamp.h"
#include "access/zhtup.h"
#include "lib/pairingheap.h"
#include "storage/buf.h"


/*
 * The different snapshot types.  We use SnapshotData structures to represent
 * both "regular" (MVCC) snapshots and "special" snapshots that have non-MVCC
 * semantics.  The specific semantics of a snapshot are encoded by its type.
 *
 * The behaviour of each type of snapshot should be documented alongside its
 * enum value, best in terms that are not specific to an individual table AM.
 *
 * The reason the snapshot type rather than a callback as it used to be is
 * that that allows to use the same snapshot for different table AMs without
 * having one callback per AM.
 */
typedef enum SnapshotType
{
	/*-------------------------------------------------------------------------
	 * A tuple is visible iff the tuple is valid for the given MVCC snapshot.
	 *
	 * Here, we consider the effects of:
	 * - all transactions committed as of the time of the given snapshot
	 * - previous commands of this transaction
	 *
	 * Does _not_ include:
	 * - transactions shown as in-progress by the snapshot
	 * - transactions started after the snapshot was taken
	 * - changes made by the current command
	 * -------------------------------------------------------------------------
	 */
	SNAPSHOT_MVCC = 0,

	/*-------------------------------------------------------------------------
	 * A tuple is visible iff the tuple is valid including effects of open
	 * transactions.
	 *
	 * Here, we consider the effects of:
	 * - all committed and in-progress transactions (as of the current instant)
	 * - previous commands of this transaction
	 * - changes made by the current command
	 * -------------------------------------------------------------------------
	 */
	SNAPSHOT_SELF,

	/*
	 * Any tuple is visible.
	 */
	SNAPSHOT_ANY,

	/*
	 * A tuple is visible iff the tuple tuple is valid as a TOAST row.
	 */
	SNAPSHOT_TOAST,

	/*-------------------------------------------------------------------------
	 * A tuple is visible iff the tuple is valid including effects of open
	 * transactions.
	 *
	 * Here, we consider the effects of:
	 * - all committed and in-progress transactions (as of the current instant)
	 * - previous commands of this transaction
	 * - changes made by the current command
	 * -------------------------------------------------------------------------
	 */
	SNAPSHOT_DIRTY,

	/*
	 * A tuple is visible iff it follows the rules of SNAPSHOT_MVCC, but
	 * supports being called in timetravel context (for decoding catalog
	 * contents in the context of logical decoding).
	 */
	SNAPSHOT_HISTORIC_MVCC,

	/*
	 * A tuple is visible iff the tuple might be visible to some transaction;
	 * false if it's surely dead to everyone, ie, vacuumable.
	 *
	 * Snapshot.xmin must have been set up with the xmin horizon to use.
	 */
	SNAPSHOT_NON_VACUUMABLE
} SnapshotType;

typedef struct SnapshotData *Snapshot;

#define InvalidSnapshot		((Snapshot) NULL)

/*
 * Struct representing all kind of possible snapshots.
 *
 * There are several different kinds of snapshots:
 * * Normal MVCC snapshots
 * * MVCC snapshots taken during recovery (in Hot-Standby mode)
 * * Historic MVCC snapshots used during logical decoding
 * * snapshots passed to HeapTupleSatisfiesDirty()
 * * snapshots passed to HeapTupleSatisfiesNonVacuumable()
 * * snapshots used for SatisfiesAny, Toast, Self where no members are
 *	 accessed.
 *
 * TODO: It's probably a good idea to split this struct using a NodeTag
 * similar to how parser and executor nodes are handled, with one type for
 * each different kind of snapshot to avoid overloading the meaning of
 * individual fields.
 */
typedef struct SnapshotData
{
	SnapshotType snapshot_type; /* type of snapshot */

	/*
	 * The remaining fields are used only for MVCC snapshots, and are normally
	 * just zeroes in special snapshots.  (But xmin and xmax are used
	 * specially by HeapTupleSatisfiesDirty, and xmin is used specially by
	 * HeapTupleSatisfiesNonVacuumable.)
	 *
	 * An MVCC snapshot can never see the effects of XIDs >= xmax. It can see
	 * the effects of all older XIDs except those listed in the snapshot. xmin
	 * is stored as an optimization to avoid needing to search the XID arrays
	 * for most tuples.
	 */
	TransactionId xmin;			/* all XID < xmin are visible to me */
	TransactionId xmax;			/* all XID >= xmax are invisible to me */

	/*
	 * This if for the new type of locks for sub-transactions for zheap.
	 * This is filled in ZHeapTupleSatisfiesDirty, if the tuple is modified
	 * by a sub-transaction.  This allows us to wait on subtransactions.
	 */
	SubTransactionId subxid;

	/*
	 * For normal MVCC snapshot this contains the all xact IDs that are in
	 * progress, unless the snapshot was taken during recovery in which case
	 * it's empty. For historic MVCC snapshots, the meaning is inverted, i.e.
	 * it contains *committed* transactions between xmin and xmax.
	 *
	 * note: all ids in xip[] satisfy xmin <= xip[i] < xmax
	 */
	TransactionId *xip;
	uint32		xcnt;			/* # of xact ids in xip[] */

	/*
	 * For non-historic MVCC snapshots, this contains subxact IDs that are in
	 * progress (and other transactions that are in progress if taken during
	 * recovery). For historic snapshot it contains *all* xids assigned to the
	 * replayed transaction, including the toplevel xid.
	 *
	 * note: all ids in subxip[] are >= xmin, but we don't bother filtering
	 * out any that are >= xmax
	 */
	TransactionId *subxip;
	int32		subxcnt;		/* # of xact ids in subxip[] */
	bool		suboverflowed;	/* has the subxip array overflowed? */

	bool		takenDuringRecovery;	/* recovery-shaped snapshot? */
	bool		copied;			/* false if it's a static snapshot */

	CommandId	curcid;			/* in my xact, CID < curcid are visible */

	/*
	 * An extra return value for HeapTupleSatisfiesDirty, not used in MVCC
	 * snapshots.
	 */
	uint32		speculativeToken;

	/*
	 * Book-keeping information, used by the snapshot manager
	 */
	uint32		active_count;	/* refcount on ActiveSnapshot stack */
	uint32		regd_count;		/* refcount on RegisteredSnapshots */
	pairingheap_node ph_node;	/* link in the RegisteredSnapshots heap */

	TimestampTz whenTaken;		/* timestamp when snapshot was taken */
	XLogRecPtr	lsn;			/* position in the WAL stream when taken */
} SnapshotData;

#endif							/* SNAPSHOT_H */
