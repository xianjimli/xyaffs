/**
 * Copyright(C) 2010 Li XianJing <xianjimli@gmail.com>
 */

#include <utime.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>

#define YAFFS_MAX_NAME_LENGTH		255
#define YAFFS_MAX_ALIAS_LENGTH		159
#define YAFFS_CHUNCK_DATA_SIZE      2048
#define YAFFS_CHUNCK_TAG_SIZE       64

typedef struct {
	unsigned char colParity;
	unsigned lineParity;
	unsigned lineParityPrime;
} yaffs_ECCOther;

typedef struct {
#ifdef WITH_BRN_BBF
	unsigned short brn_bad_blk_flags;
#endif/*WITH_BRN_BBF*/

	unsigned sequenceNumber;
	unsigned objectId;
	unsigned chunkId;
	unsigned byteCount;
} __attribute__ ((packed)) yaffs_PackedTags2TagsPart;

typedef struct {
	yaffs_PackedTags2TagsPart t;
	yaffs_ECCOther ecc;
} yaffs_PackedTags2;

typedef struct _ChunkWithTag
{
	char data[YAFFS_CHUNCK_DATA_SIZE];
	yaffs_PackedTags2 pt;
	char unused[YAFFS_CHUNCK_TAG_SIZE-sizeof(yaffs_PackedTags2)];
}ChunkWithTag;

typedef enum {
	YAFFS_OBJECT_TYPE_UNKNOWN,
	YAFFS_OBJECT_TYPE_FILE,
	YAFFS_OBJECT_TYPE_SYMLINK,
	YAFFS_OBJECT_TYPE_DIRECTORY,
	YAFFS_OBJECT_TYPE_HARDLINK,
	YAFFS_OBJECT_TYPE_SPECIAL
} yaffs_ObjectType;

typedef unsigned short __u16;
typedef unsigned int   __u32;
typedef char YCHAR;

typedef struct {
	yaffs_ObjectType type;
	int parentObjectId;
    __u16 sum__NoLongerUsed;
    YCHAR name[YAFFS_MAX_NAME_LENGTH + 1];
    __u32 yst_mode; 
	__u32 yst_uid;
	__u32 yst_gid;
	__u32 yst_atime;
	__u32 yst_mtime;
	__u32 yst_ctime;

	int fileSize;

	int equivalentObjectId;

	YCHAR alias[YAFFS_MAX_ALIAS_LENGTH + 1];

	__u32 yst_rdev;
	__u32 roomToGrow[6];
	__u32 inbandShadowsObject;
	__u32 inbandIsShrink;
	__u32 reservedSpace[2];
	int shadowsObject;	
	__u32 isShrink;

} yaffs_ObjectHeader;

static void* extract(void* addr, void* end)
{
	FILE* fp          = NULL;
	char cwd[260]     = {0};
	char* file_name   = NULL;
	struct utimbuf times = {0};
	ChunkWithTag* cwt = addr;
	yaffs_ObjectHeader* obj_hdr = (yaffs_ObjectHeader*)cwt->data;
	int parent_id = cwt->pt.t.objectId;

	cwt++;
	getcwd(cwd, sizeof(cwd));

	//printf("%s oid=%d\n", cwd, parent_id);

	while(((size_t)cwt < (size_t)end))
	{
		if(cwt->pt.t.chunkId == 0)
		{
			if(fp != NULL)
			{
				utime(file_name, &times);
				fclose(fp);
				fp = NULL;
			}

			obj_hdr = (yaffs_ObjectHeader*)cwt->data;
			
			//printf("%s %s poid=%d(%d) oid=%d\n", cwd, obj_hdr->name,
			//		obj_hdr->parentObjectId, parent_id, cwt->pt.t.objectId);

			if(obj_hdr->parentObjectId != parent_id)
			{
				char* p = strrchr(cwd, '/');
				if(p != NULL)
				{
					*p = '\0';
				}
				chdir(cwd);

				return cwt;
			}

			switch(obj_hdr->type)
			{
				case YAFFS_OBJECT_TYPE_DIRECTORY:
				{
					mkdir(obj_hdr->name, 0777);
					chdir(obj_hdr->name);
					cwt = extract(cwt, end);
					continue;
				}
				case YAFFS_OBJECT_TYPE_FILE:
				{
					fp = fopen(obj_hdr->name, "wb+");
					file_name = obj_hdr->name;
					times.actime = obj_hdr->yst_atime;
					times.modtime = obj_hdr->yst_mtime;
					break;
				}
				case YAFFS_OBJECT_TYPE_SYMLINK:
				{
					if(symlink(obj_hdr->alias, obj_hdr->name) != 0)
					{
						printf("SYMLINK: %s %s %s fail.\n", 
							cwd, obj_hdr->alias, obj_hdr->name);
						perror("symlink");
					}
					break;
				}
				case YAFFS_OBJECT_TYPE_HARDLINK:
				{
					if(link(obj_hdr->alias, obj_hdr->name) != 0)
					{
						printf("LINK: %s %s %s fail.\n", 
							cwd, obj_hdr->alias, obj_hdr->name);
						perror("link");
					}
					break;
				}
				case YAFFS_OBJECT_TYPE_SPECIAL:
				{
					int ret = 0;
					if(S_ISBLK(obj_hdr->yst_mode))
					{
						ret = mknod(obj_hdr->name, obj_hdr->yst_mode, obj_hdr->yst_rdev);
					}
					else if(S_ISCHR(obj_hdr->yst_mode))
					{
						ret = mknod(obj_hdr->name, obj_hdr->yst_mode, obj_hdr->yst_rdev);
					}
					else if(S_ISFIFO(obj_hdr->yst_mode))
					{
						ret = mknod(obj_hdr->name, obj_hdr->yst_mode, obj_hdr->yst_rdev);
					}
					else if(S_ISSOCK(obj_hdr->yst_mode))
					{
						ret = mknod(obj_hdr->name, obj_hdr->yst_mode, obj_hdr->yst_rdev);
					}
					else
					{
						assert(!"impossible!");
					}
					if(ret != 0)
					{
						perror("mknod");
					}
					break;
				}
				default:break;
			}
		}
		else
		{
			if(fp != NULL)
			{
				fwrite(cwt->data, cwt->pt.t.byteCount, 1, fp);
				fflush(fp);
			}
		}

		cwt++;
	}

	return cwt;
}

int main(int argc, char* argv[])
{
	void*  addr     = 0;
	FILE*  fp       = NULL;
	struct stat st = {0};
	const char* file_name = argv[1];
	const char* path = argc == 3 ? argv[2] :"./tmp/";

	if(file_name == NULL)
	{
		printf("\n==================www.broncho.cn=======================\n");
		printf("#\n");
		printf("# Usage: %s image path\n", argv[0]);
		printf("#\n");
		printf("========================================================\n");

		return 0;
	}

	if((fp = fopen(file_name, "rb")) == NULL)
	{
		perror("fopen");
		return 0;
	}
	
	stat(file_name, &st);
	if((addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fileno(fp), 0)) != NULL)
	{
		mkdir(path, 0777);
		chdir(path);

		extract(addr, (char*)addr + st.st_size);
		munmap(addr, st.st_size);
	}
	else
	{
		perror("mmap");
	}
	
	fclose(fp);

	return 0;
}

