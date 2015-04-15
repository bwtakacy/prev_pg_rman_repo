/*-------------------------------------------------------------------------
 *
 * controlfile.c: operations for control file
 *
 * Copyright (c) 2009-2015, NIPPON TELEGRAPH AND TELEPHONE CORPORATION
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include "catalog/pg_control.h"
#include "common/fe_memutils.h"

#include <unistd.h>

#include "pg_rman.h"

/*
 * Reads control file into a pg_malloc()'d buffer and returns a pointer to it.
 * To be used by more special-purpose routines such as get_current_timeline()
 * and get_data_checksum_version()
 *
 * NOTE: The special-purpose routines need to pg_free()/free() the block of
 * memory allocated here once they are done using the control file info
 *
 */
char *
read_control_file()
{
	char 		*buffer;
	int			fd;
	char		ControlFilePath[MAXPGPATH];
	pg_crc32c	crc;

	snprintf(ControlFilePath, MAXPGPATH, "%s/global/pg_control", pgdata);

	if ((fd = open(ControlFilePath, O_RDONLY | PG_BINARY, 0)) == -1)
	{
		elog(WARNING, _("can't open pg_controldata file \"%s\": %s"),
		ControlFilePath, strerror(errno));
		return NULL;
	}

	buffer = (char *) pg_malloc(PG_CONTROL_SIZE);

	if (read(fd, buffer, PG_CONTROL_SIZE) != PG_CONTROL_SIZE)
	{
		elog(WARNING, _("can't read pg_controldata file \"%s\": %s"),
		ControlFilePath, strerror(errno));
		return NULL;
	}

	close(fd);

	/* Check the CRC. */
	PGRMAN_INIT_CRC32(crc);
	PGRMAN_COMP_CRC32(crc,
			buffer,
			offsetof(ControlFileData, crc));
	PGRMAN_FIN_CRC32(crc);

	if (!PGRMAN_EQ_CRC32(crc, ((ControlFileData *) buffer)->crc))
	{
		elog(WARNING, _("Calculated CRC checksum does not match value stored in file.\n"
			"Either the file is corrupt, or it has a different layout than this program\n"
			"is expecting.  The results below are untrustworthy.\n"));
	}

	if (((ControlFileData *) buffer)->pg_control_version != PG_CONTROL_VERSION)
	{
		elog(WARNING, _("possible byte ordering mismatch\n"
			"The byte ordering used to store the pg_control file might not match the one\n"
			"used by this program.  In that case the results below would be incorrect, and\n"
			"the PostgreSQL installation would be incompatible with this data directory.\n"));
	}

	return buffer;
}
