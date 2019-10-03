/*-------------------------------------------------------------------------
 *
 * undodefs.h
 *
 * Basic definitions for PostgreSQL undo layer. These are separated into
 * their own header file to avoid including more things than necessary
 * into widely-used headers.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/access/undodefs.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef UNDODEFS_H
#define UNDODEFS_H

/* The type used to identify an undo log and position within it. */
typedef uint64 UndoRecPtr;

/* The type used for undo record lengths. */
typedef uint16 UndoRecordSize;

/* Type for offsets within undo logs */
typedef uint64 UndoLogOffset;

/* Type for numbering undo logs. */
typedef int UndoLogNumber;

struct UndoRecordSet;
typedef struct UndoRecordSet UndoRecordSet;

#endif
