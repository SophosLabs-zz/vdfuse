/* Commandline tool for mounting VDI/VMDK/VHD files 					*
 *  																	*
 *  Copyright 2009-2011, 2013 by it's authors.  						*
 *  Some rights reserved. See COPYING, AUTHORS.							*
 *																		*
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 2 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

/* DESCRIPTION
 * This code is structured in the following sections:
 *  *  The main(argc, argv) routine including validation of arguments and call to fuse_main
 *  *  MBR and EBR parsing routines
 *  *  The Fuse callback routines for destroy ,flush ,getattr ,open, read, readdir, write
 *
 * For further details on how this all works see http://fuse.sourceforge.net/
 *
 * VirtualBox provided an API to enable you to manipulate VD image files programmatically.
 * This is documented in the embedded source comments.  See for further details
 *    http://www.virtualbox.org/svn/vbox/trunk/include/VBox/VBoxHDD-new.h
 *
 */
#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64
#include <limits.h>
#include <fuse.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "config.h"

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#define IN_RING3
#define BLOCKSIZE 512
#define UNALLOCATED -1
#define GETOPT_ARGS "rgvawt:s:f:dh?"
#define HOSTPARTITION_MAX 100
#define DIFFERENCING_MAX 100
#define PNAMESIZE 15
#define MBR_START 446
#define EBR_START 446
#define PARTTYPE_IS_EXTENDED(x) ((x) == 0x05 || (x) == 0x0f || (x) == 0x85)
#define ENTIRE_DISK_STR "EntireDisk"

void usageAndExit (char *optFormat, ...);
void vbprintf (const char *format, ...);
void vdErrorCallback (void *pvUser, int rc, const char *file, unsigned iLine,
											const char *function, const char *format, va_list va);
void initialisePartitionTable (void);
int findPartition (const char *filename);
int detectDiskType (char **disktype, char *filename);
static int VD_open (const char *c, struct fuse_file_info *i);
static int VD_release (const char *name, struct fuse_file_info *fi);
static int VD_read (const char *c, char *out, size_t len, off_t offset,
										struct fuse_file_info *i UNUSED);
static int VD_write (const char *c, const char *in, size_t len, off_t offset,
										 struct fuse_file_info *i UNUSED);
static int VD_flush (const char *p, struct fuse_file_info *i UNUSED);
static int VD_readdir (const char *p, void *buf, fuse_fill_dir_t filler,
											 off_t offset UNUSED, struct fuse_file_info *i UNUSED);
static int VD_getattr (const char *p, struct stat *stbuf);
void VD_destroy (void *u);

#include <VBox/vd.h>

#define DISKread(o,b,s) VDRead (hdDisk,o,b,s);
#define DISKwrite(o,b,s) VDWrite (hdDisk,o,b,s);
#define DISKclose VDCloseAll(hdDisk)
#define DISKsize VDGetSize(hdDisk, 0)
#define DISKflush VDFlush(hdDisk)
#define DISKopen(t,i) \
   if (RT_FAILURE(VDOpen(hdDisk,t , i, readonly ? VD_OPEN_FLAGS_READONLY : VD_OPEN_FLAGS_NORMAL, NULL))) \
      usageAndExit("opening vbox image failed");

PVBOXHDD hdDisk;
PVDINTERFACE pVDifs = NULL;
VDINTERFACE vdError;
VDINTERFACEERROR vdErrorCallbacks = {
	// .cbSize = sizeof (VDINTERFACEERROR),
	//.enmInterface = VDINTERFACETYPE_ERROR,
	.pfnError = vdErrorCallback
};

// Partition table information

typedef struct
{																// See http://en.wikipedia.org/wiki/Master_boot_record
	uint8_t status;								// status[7] (0x80 = bootable, 0x00 = non-bootable,other = invalid[8])
// ** CHS address of first block **
	uint8_t shead;								// first head
	uint8_t ssector;							// first sector is in bits 5-0; bits 9-8 of cylinder are in bits 7-6
	uint8_t sbits;								// first bits 7-0 of cylinder
	uint8_t type;									// partition type
// ** CHS address of last block **
	uint8_t ehead;								// last head
	uint8_t esector;							// last sector is in bits 5-0; bits 9-8 of cylinder are in bits 7-6
	uint8_t ebits;								// last bits 7-0 of cylinder
	uint32_t offset;							// LBA of first sector in the partition
	uint32_t size;								// number of blocks in partition, in little-endian format
} MBRentry;

typedef struct
{
	char name[PNAMESIZE + 1];			// name of partition
	off_t offset;									// offset into disk in bytes
	uint64_t size;								// size of partiton in bytes
	int no;												// partition number
	MBRentry descriptor;					// copy of MBR / EBR descriptor that defines the partion
} Partition;

#pragma pack( push )
#pragma pack( 1 )

typedef struct
{
	char fill[MBR_START];
	MBRentry descriptor[4];
	uint16_t signature;
} MBRblock;


typedef struct
{																// See http://en.wikipedia.org/wiki/Extended_boot_record for details
	char fill[EBR_START];
	MBRentry descriptor;
	MBRentry chain;
	MBRentry fill1, fill2;
	uint16_t signature;
} EBRentry;

#pragma pack( pop )

Partition partitionTable[HOSTPARTITION_MAX + 1];	// Note the partitionTable[0] is reserved for the EntireDisk descriptor
static int lastPartition = 0;

static struct fuse_operations fuseOperations = {
	.readdir = VD_readdir,
	.getattr = VD_getattr,
	.open = VD_open,
	.release = VD_release,
	.read = VD_read,
	.write = VD_write,
	.flush = VD_flush,
	.destroy = VD_destroy
};

static struct fuse_args fuseArgs = FUSE_ARGS_INIT (0, NULL);

static struct stat VDfile_stat;

static int verbose = 0;
static int readonly = 0;
static int allowall = 0;				// allow all users to read from disk
static int allowallw = 0;				// allow all users to write to disk
static uid_t myuid = 0;
static gid_t mygid = 0;
static char *processName;
static int entireDiskOpened = 0;
static int partitionOpened = 0;
static int opened = 0;					// how many opened instances are there

//
//====================================================================================================
//                                Main routine including validation
//====================================================================================================

int
main (int argc, char **argv)
{
	char *diskType = "auto";
	char *imagefilename = NULL;
	char *mountpoint = NULL;
	int debug = 0;
	int foreground = 0;
	char c;
	int i;
	char *differencing[DIFFERENCING_MAX];
	int differencingLen = 0;

	extern char *optarg;
	extern int optind, optopt;

//
// *** Parse the command line options ***
//
	processName = argv[0];

	while ((c = getopt (argc, argv, GETOPT_ARGS)) != -1)
	{
		switch (c)
		{
			case 'r':
				readonly = 1;
				break;
			case 'g':
				foreground = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'a':
				allowall = 1;
				break;
			case 'w':
				allowall = 1;
				allowallw = 1;
				break;
			case 't':
				diskType = (char *) optarg;
				break;									// ignored if OLDAPI
			case 's':
				if (differencingLen == DIFFERENCING_MAX)
					usageAndExit ("Too many differencing disks");
				differencing[differencingLen++] = (char *) optarg;
				break;
			case 'f':
				imagefilename = (char *) optarg;
				break;
			case 'd':
				foreground = 1;
				debug = 1;
				break;
			case 'h':
				usageAndExit (NULL);
			case '?':
				usageAndExit ("Unknown option");
		}
	}
//
// *** Validate the command line ***
//
	if (argc != optind + 1)
		usageAndExit ("a single mountpoint must be specified");
	mountpoint = argv[optind];
	if (!mountpoint)
		usageAndExit ("no mountpoint specified");
	if (!imagefilename)
		usageAndExit ("no image chosen");
	if (stat (imagefilename, &VDfile_stat) < 0)
		usageAndExit ("cannot access imagefile");
	if (access (imagefilename, F_OK | R_OK | ((!readonly) ? W_OK : 0)) < 0)
		usageAndExit ("cannot access imagefile");
	for (i = 0; i < differencingLen; i++)
		if (access (differencing[i], F_OK | R_OK | ((readonly) ? 0 : W_OK)) < 0)
			usageAndExit ("cannot access differencing imagefile %s",
										differencing[i]);

#define IS_TYPE(s) (strcmp (s, diskType) == 0)
	if (!
			(IS_TYPE ("auto") || IS_TYPE ("VDI") || IS_TYPE ("VMDK")
			 || IS_TYPE ("VHD") || IS_TYPE ("auto")))
		usageAndExit ("invalid disk type specified");
	if (strcmp ("auto", diskType) == 0
			&& detectDiskType (&diskType, imagefilename) < 0)
		return 1;

//
// *** Open the VDI, parse the MBR + EBRs and connect to the fuse service ***
//

	if (RT_FAILURE (VDInterfaceAdd (&vdError, "VD Error", VDINTERFACETYPE_ERROR, &vdErrorCallbacks, 0, &pVDifs)))
		usageAndExit ("invalid initialisation of VD interface");
	if (RT_FAILURE (VDCreate (&vdError, VDTYPE_HDD, &hdDisk)))
		usageAndExit ("invalid initialisation of VD interface");
	DISKopen (diskType, imagefilename);

	for (i = 0; i < differencingLen; i++)
	{
		char *diffType;
		char *diffFilename = differencing[i];
		detectDiskType (&diffType, diffFilename);
		DISKopen (diffType, diffFilename);
	}

	initialisePartitionTable ();

	myuid = geteuid ();
	mygid = getegid ();

	fuse_opt_add_arg (&fuseArgs, "vdfuse");

	{
		char fsname[strlen (imagefilename) + 12];
		strcpy (fsname, "-ofsname=\0");
		strcat (fsname, imagefilename);
		fuse_opt_add_arg (&fuseArgs, fsname);
	}

	fuse_opt_add_arg (&fuseArgs, "-osubtype=vdfuse");
	fuse_opt_add_arg (&fuseArgs, "-o");
	fuse_opt_add_arg (&fuseArgs, (allowall) ? "allow_other" : "allow_root");
	if (foreground)
		fuse_opt_add_arg (&fuseArgs, "-f");
	if (debug)
		fuse_opt_add_arg (&fuseArgs, "-d");
	fuse_opt_add_arg (&fuseArgs, mountpoint);

	return fuse_main (fuseArgs.argc, fuseArgs.argv, &fuseOperations
#if FUSE_USE_VERSION >= 26
										, NULL
#endif
		);
}

//====================================================================================================
//                                  Miscellaneous output utilities
//====================================================================================================

void
usageAndExit (char *optFormat, ...)
{
	va_list ap;
	if (optFormat != NULL)
	{
		fputs ("\nERROR: ", stderr);
		va_start (ap, optFormat);
		vfprintf (stderr, optFormat, ap);
		va_end (ap);
		fputs ("\n\n", stderr);
	}
//              ---------!---------!---------!---------!---------!---------!---------!---------!
	fprintf (stderr,
     "DESCRIPTION: This Fuse module uses the VirtualBox access library to open a \n"
     "VirtualBox supported VD image file and mount it as a Fuse file system.  The\n"
     "mount point contains a flat directory containing the files EntireDisk,\n"
     "Partition1 .. PartitionN.  These can then be loop mounted to access the\n"
     "underlying file systems\n\n"
     "USAGE: %s [options] -f image-file mountpoint\n"
     "\t-h\thelp\n" "\t-r\treadonly\n"
#ifndef OLDAPI
     "\t-t\tspecify type (VDI, VMDK, VHD, or raw; default: auto)\n"
#endif
     "\t-f\tVDimage file\n"
     "\t-s\tdifferencing disk files\n"    // prevent misuse
     "\t-a\tallow all users to read disk\n"
     "\t-w\tallow all users to read and write to disk\n"
     "\t-g\trun in foreground\n"
     "\t-v\tverbose\n"
     "\t-d\tdebug\n\n"
     "NOTE: \n"
     "Linux: you must add the line \"user_allow_other\" (without quotes) to /etc/fuse.confand set proper permissions on /etc/fuse.conf\n"
     "OSX: run with sudo for this to work.\n", processName);
    exit (1);
}

void
vbprintf (const char *format, ...)
{
	va_list ap;
	if (!verbose)
		return;
	va_start (ap, format);
	vprintf (format, ap);
	va_end (ap);
	fputs ("\n", stdout);
	fflush (stdout);
}

void
vdErrorCallback (void *pvUser UNUSED, int rc, const char *file,
								 unsigned iLine, const char *function, const char *format,
								 va_list va)
{
	fprintf (stderr, "\nVD CallbackError rc %d at %s:%u (%s): ", rc, file,
					 iLine, function);
	vfprintf (stderr, format, va);
	fputs ("\n", stderr);
}


//====================================================================================================
//                                        MBR + EBR parsing routine
//====================================================================================================
//
// This code is algorithmically based on partRead in VBoxInternalManage.cpp plus the Wikipedia articles
// on MBR and EBR. As in partRead, a statically allocated partition list is used same  to keep things
// simple (but up to a max 100 partitions :lol:).  Note than unlike partRead, this doesn't resort the
// partitions.
//
//int VDRead(PVBOXHDD pDisk, uint64_t uOffset, void *pvBuf, size_t cbRead, int ii );

void
initialisePartitionTable (void)
{
//uint16_t MBRsignature;
	int entendedFlag = UNALLOCATED;
	int i;
	MBRblock mbrb;

	memset (partitionTable, 0, sizeof (partitionTable));
	for (i = 0; i <= (signed) (sizeof (partitionTable) / sizeof (Partition)); i++) {
	    partitionTable[i].no = UNALLOCATED;
    }

	partitionTable[0].no = 0;
	partitionTable[0].offset = 0;
	partitionTable[0].size = DISKsize;
	strcpy (partitionTable[0].name, ENTIRE_DISK_STR);
//
// Check that this is unformated or a DOS partitioned disk.  Sorry but other formats not supported.
//
	DISKread (0, &mbrb, sizeof (mbrb));
	if (mbrb.signature == 0x0000)
		return;											// an unformated disk is allowed but only EntireDisk is defined
	if (mbrb.signature != 0xaa55)
		usageAndExit ("Invalid MBR found on image with signature 0x%04hX",
									mbrb.signature);

//
// Process the four physical partition entires in the MBR
//
	for (i = 1; i <= 4; i++)
	{
		Partition *p = partitionTable + i;
//    MBRentry  *m = &(p->descriptor);
	//DISKread (MBR_START + (i-1) * sizeof(MBRentry), &(p->descriptor), sizeof(MBRentry));
		memcpy (&(p->descriptor), &mbrb.descriptor[i - 1], sizeof (MBRentry));
		if ((p->descriptor).type == 0)
			continue;
		if (PARTTYPE_IS_EXTENDED ((p->descriptor).type))
		{
			if (entendedFlag != UNALLOCATED)
				usageAndExit ("More than one extended partition in MBR");
			entendedFlag = i;
		}
		else
		{
			lastPartition = i;
			p->no = i;
			p->offset = (off_t) ((p->descriptor).offset) * BLOCKSIZE;
			p->size = (off_t) ((p->descriptor).size) * BLOCKSIZE;
		}
	}
//
// Now chain down any EBRs to process the logical partition entries
//
	if (entendedFlag != UNALLOCATED)
	{
		EBRentry ebr;
		off_t uStart =
			(off_t) ((partitionTable[entendedFlag].descriptor).offset) * BLOCKSIZE;
		off_t uOffset = 0;

		if (!uStart)
			usageAndExit ("Inconsistency for logical partition start. Aborting\n");

		for (i = 5; i <= HOSTPARTITION_MAX; i++)
		{
			lastPartition++;
			Partition *p = partitionTable + i;

			DISKread (uStart + uOffset + EBR_START, &ebr, sizeof (ebr));

			if (ebr.signature != 0xaa55)
				usageAndExit ("Invalid EBR signature found on image");
			if ((ebr.descriptor).type == 0)
				usageAndExit ("Logical partition with type 0 encountered");
			if (!((ebr.descriptor).offset))
				usageAndExit
					("Logical partition invalid partition start offset encountered");

			p->descriptor = ebr.descriptor;
			p->no = i;
			lastPartition = i;
			p->offset =
				uStart + uOffset + (off_t) ((ebr.descriptor).offset) * BLOCKSIZE;
			p->size = (off_t) ((ebr.descriptor).size) * BLOCKSIZE;

			if (ebr.chain.type == 0)
				break;
			if (!PARTTYPE_IS_EXTENDED (ebr.chain.type))
				usageAndExit ("Logical partition chain broken");
			uOffset = (ebr.chain).offset;
		}
	}
//
// Now print out the partition table
//
	vbprintf ("Partition       Size           Offset\n"
						"=========       ====           ======\n");
	for (i = 1; i <= lastPartition; i++)
	{
		Partition *p = partitionTable + i;
		if (p->no != UNALLOCATED)
		{
			sprintf (p->name, "Partition%d", i);
			vbprintf ("%-14s  %-13lld  %-13lld", p->name, p->offset, p->size);
		}
	}
	vbprintf ("\n");
}

int
findPartition (const char *filename)
{
// Use a dumb serial search since there are typically less than 3 entries
	int i;
	register Partition *p = partitionTable;
	for (i = 0; i <= lastPartition; i++, p++)
	{
		if (p->no != UNALLOCATED && strcmp (filename + 1, p->name) == 0)
			return i;
	}
	return -1;
}

// detects type of virtual image
int
detectDiskType (char **disktype, char *filename)
{
	char buf[8];
	int fd = open (filename, O_RDONLY);
	read (fd, buf, sizeof (buf));

	if (strncmp (buf, "conectix", 8) == 0)
		*disktype = "VHD";
	else if (strncmp (buf, "VMDK", 4) == 0)
		*disktype = "VMDK";
	else if (strncmp (buf, "KDMV", 4) == 0)
		*disktype = "VMDK";
	else if (strncmp (buf, "<<<", 3) == 0)
		*disktype = "VDI";
	else
		usageAndExit ("cannot autodetect disk type of %s", filename);

	vbprintf ("disktype is %s", *disktype);
	close (fd);
	return 0;
}

//====================================================================================================
//                                         Fuse Callback Routines
//====================================================================================================
//
// in alphetic order to help find them: destroy ,flush ,getattr ,open, read, readdir, write

pthread_mutex_t disk_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t part_mutex = PTHREAD_MUTEX_INITIALIZER;

void
VD_destroy (void *u UNUSED)
{
// called when the fuse filesystem is umounted
	vbprintf ("destroy");
	DISKclose;
}

int
VD_flush (const char *p, struct fuse_file_info *i UNUSED)
{
	vbprintf ("flush: %s", p);
	DISKflush;
	return 0;
}

static int
VD_getattr (const char *p, struct stat *stbuf)
{
	vbprintf ("getattr: %s", p);
	int isFileRoot = (strcmp ("/", p) == 0);
	int n = findPartition (p);

	if (!isFileRoot && n == -1)
		return -ENOENT;

// Use the container file's stat return as the basis. However since partitions cannot
// be created by creating files, there is no write access to the directory.  I also
// treat group access the same as other.

	memcpy (stbuf, &VDfile_stat, sizeof (struct stat));

	if (isFileRoot)
	{
		stbuf->st_mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP;
		if (allowall)
			stbuf->st_mode |= S_IROTH;
		stbuf->st_size = 0;
		stbuf->st_blocks = 2;
	}
	else
	{
		stbuf->st_mode = S_IFREG | S_IRUSR | S_IWUSR;
		if (allowall)
			stbuf->st_mode |= S_IRGRP | S_IROTH;
		if (allowallw)
			stbuf->st_mode |= S_IWGRP | S_IWOTH;
		stbuf->st_size = partitionTable[n].size;
		stbuf->st_blocks = (stbuf->st_size + BLOCKSIZE - 1) / BLOCKSIZE;
	}
	if (readonly)
	{
		stbuf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	}

	stbuf->st_nlink = 1;

	return 0;
}

static int
VD_open (const char *cName, struct fuse_file_info *i)
{
	vbprintf ("open: %s, %lld, 0X%08lX ", cName, i->fh, i->flags);
	int n = findPartition (cName);
	if ((n == -1) || (entireDiskOpened && n > 0) || (partitionOpened && n == 0))
		return -ENOENT;
	if (readonly && ((i->flags & (O_WRONLY | O_RDWR)) != 0))
		return -EROFS;

	if (n == 0)
		entireDiskOpened = 1;
	else
		partitionOpened = 1;

	pthread_mutex_lock (&part_mutex);
	opened++;
	pthread_mutex_unlock (&part_mutex);

	return 0;
}

static int
VD_release (const char *name, struct fuse_file_info *fi)
{
	(void) fi;
	vbprintf ("release: %s", name);

	pthread_mutex_lock (&part_mutex);
	opened--;
	if (opened == 0)
	{
		initialisePartitionTable ();
		entireDiskOpened = 0;
		partitionOpened = 0;
	}
	pthread_mutex_unlock (&part_mutex);

	return 0;
}

static int
VD_read (const char *c, char *out, size_t len, off_t offset,
				 struct fuse_file_info *i UNUSED)
{
	vbprintf ("read: %s, offset=%lld, length=%d", c, offset, len);
	int n = findPartition (c);
	if (n < 0)
		return -ENOENT;
	if ((n == 0) ? partitionOpened : entireDiskOpened)
		return -EIO;

	Partition *p = &(partitionTable[n]);
//   if (offset >= p->size) return 0;
//   if (offset + len> p->size) len = p->size - offset;
	if ((uint64_t) offset >= p->size)
		return 0;
	if ((uint64_t) (offset + len) > p->size)
		len = p->size - offset;

	pthread_mutex_lock (&disk_mutex);
	int ret = DISKread (offset + p->offset, out, len);
	pthread_mutex_unlock (&disk_mutex);

	return RT_SUCCESS (ret) ? (signed) len : -EIO;
}

static int
VD_readdir (const char *p, void *buf, fuse_fill_dir_t filler,
						off_t offset UNUSED, struct fuse_file_info *i UNUSED)
{
	int n;
	vbprintf ("readdir");
	if (strcmp ("/", p) != 0)
		return -ENOENT;
	filler (buf, ".", NULL, 0);
	filler (buf, "..", NULL, 0);
	for (n = 0; n <= lastPartition; n++)
	{
		Partition *p = partitionTable + n;
		if (p->no != UNALLOCATED)
			filler (buf, p->name, NULL, 0);
	}
	return 0;
}

static int
VD_write (const char *c, const char *in, size_t len, off_t offset,
					struct fuse_file_info *i UNUSED)
{
	vbprintf ("write: %s, offset=%lld, length=%d", c, offset, len);
	int n = findPartition (c);
	if (n < 0)
		return -ENOENT;
	if ((n == 0) ? partitionOpened : entireDiskOpened)
		return -EIO;
	Partition *p = &(partitionTable[n]);
	if ((uint64_t) offset >= p->size)
		return 0;
	if ((uint64_t) (offset + len) > p->size)
		len = p->size - offset;

	pthread_mutex_lock (&disk_mutex);
	int ret = DISKwrite (offset + p->offset, in, len);
	pthread_mutex_unlock (&disk_mutex);

	return RT_SUCCESS (ret) ? (signed) len : -EIO;
}
