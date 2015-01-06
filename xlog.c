/*-------------------------------------------------------------------------
 *
 * xlog.c: Parse WAL files.
 *
 * Copyright (c) 2009-2015, NIPPON TELEGRAPH AND TELEPHONE CORPORATION
 *
 *-------------------------------------------------------------------------
 */

#include "pg_rman.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#if PG_VERSION_NUM >= 80400
typedef unsigned long Datum;
#if PG_VERSION_NUM < 90300
typedef struct MemoryContextData *MemoryContext;
#endif
#endif

/*
 * These unfortunate undefs are here to prevent annoying compiler warnings
 * when compiling on pre-9.3 servers
 */
#if PG_VERSION_NUM < 90300
#undef XLogFileName
#undef NextLogSeg
#endif
#include "access/xlog_internal.h"

/*
 * XLogLongPageHeaderData is modified in 8.3, but the layout is compatible
 * except xlp_xlog_blcksz.
 */
typedef union XLogPage
{
	XLogPageHeaderData		header;
	XLogLongPageHeaderData	lheader;
	char					data[XLOG_BLCKSZ];
} XLogPage;

/*
 * Return whether the file is a WAL segment or not.
 * based on ValidXLOGHeader() in src/backend/access/transam/xlog.c.
 */
bool
xlog_is_complete_wal(const pgFile *file, int server_version)
{
	FILE		   *fp;
	XLogPage		page;

	fp = fopen(file->path, "r");
	if (!fp)
		return false;
	if (fread(&page, 1, sizeof(page), fp) != XLOG_BLCKSZ)
	{
		fclose(fp);
		return false;
	}
	fclose(fp);

	/* check header */
	if (page.header.xlp_magic != XLOG_PAGE_MAGIC)
		return false;
	if ((page.header.xlp_info & ~XLP_ALL_FLAGS) != 0)
		return false;
	if ((page.header.xlp_info & XLP_LONG_HEADER) == 0)
		return false;
	if (page.lheader.xlp_seg_size != XLogSegSize)
		return false;
	if (server_version >= 80300 && page.lheader.xlp_xlog_blcksz != XLOG_BLCKSZ)
		return false;

	/*
	 * check size (actual file size, not backup file size)
	 * TODO: Support pre-compressed xlog. They might have different file sizes.
	 */
	if (file->size != XLogSegSize)
		return false;

	return true;
}

/* Being used only with servers of version < 9.3 */
#if PG_VERSION_NUM < 90300
bool
xlog_logfname2lsn(const char *logfname, XLogRecPtr *lsn)
{
	uint32 tli;
	uint32 xlogid, xrecoff;

	if (sscanf(logfname, "%08X%08X%08X",
			&tli, &xlogid, &xrecoff) != 3)
		return false;

	xrecoff *= XLogSegSize;

	lsn->xlogid = xlogid;
	lsn->xrecoff = xrecoff;
	return true;
}
#endif

/*
 * based on XLogFileName() in xlog_internal.h
 */
void
xlog_fname(char *fname, size_t len, TimeLineID tli, XLogRecPtr *lsn)
{
	uint32 xlogid, xrecoff;

#if PG_VERSION_NUM >= 90300
	xlogid = (uint32) (*lsn >> 32);
	xrecoff = (uint32) *lsn;
#else
	xlogid = lsn->xlogid;
	xrecoff = lsn->xrecoff;
#endif
	snprintf(fname, len, "%08X%08X%08X", tli,
		xlogid, xrecoff / XLogSegSize);
}
