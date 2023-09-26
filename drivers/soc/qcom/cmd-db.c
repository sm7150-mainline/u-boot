/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2018, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Linaro Ltd.
 */

#include <dm/device.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/byteorder/generic.h>

#include <soc/qcom/cmd-db.h>

#define NUM_PRIORITY		2
#define MAX_SLV_ID		8
#define SLAVE_ID_MASK		0x7
#define SLAVE_ID_SHIFT		16

/**
 * struct entry_header: header for each entry in cmddb
 *
 * @id: resource's identifier
 * @priority: unused
 * @addr: the address of the resource
 * @len: length of the data
 * @offset: offset from :@data_offset, start of the data
 */
struct entry_header {
	u8 id[8];
	__le32 priority[NUM_PRIORITY];
	__le32 addr;
	__le16 len;
	__le16 offset;
};

/**
 * struct rsc_hdr: resource header information
 *
 * @slv_id: id for the resource
 * @header_offset: entry's header at offset from the end of the cmd_db_header
 * @data_offset: entry's data at offset from the end of the cmd_db_header
 * @cnt: number of entries for HW type
 * @version: MSB is major, LSB is minor
 * @reserved: reserved for future use.
 */
struct rsc_hdr {
	__le16 slv_id;
	__le16 header_offset;
	__le16 data_offset;
	__le16 cnt;
	__le16 version;
	__le16 reserved[3];
};

/**
 * struct cmd_db_header: The DB header information
 *
 * @version: The cmd db version
 * @magic: constant expected in the database
 * @header: array of resources
 * @checksum: checksum for the header. Unused.
 * @reserved: reserved memory
 * @data: driver specific data
 */
struct cmd_db_header {
	__le32 version;
	u8 magic[4];
	struct rsc_hdr header[MAX_SLV_ID];
	__le32 checksum;
	__le32 reserved;
	u8 data[];
};

/**
 * DOC: Description of the Command DB database.
 *
 * At the start of the command DB memory is the cmd_db_header structure.
 * The cmd_db_header holds the version, checksum, magic key as well as an
 * array for header for each slave (depicted by the rsc_header). Each h/w
 * based accelerator is a 'slave' (shared resource) and has slave id indicating
 * the type of accelerator. The rsc_header is the header for such individual
 * slaves of a given type. The entries for each of these slaves begin at the
 * rsc_hdr.header_offset. In addition each slave could have auxiliary data
 * that may be needed by the driver. The data for the slave starts at the
 * entry_header.offset to the location pointed to by the rsc_hdr.data_offset.
 *
 * Drivers have a stringified key to a slave/resource. They can query the slave
 * information and get the slave id and the auxiliary data and the length of the
 * data. Using this information, they can format the request to be sent to the
 * h/w accelerator and request a resource state.
 */

static const u8 CMD_DB_MAGIC[] = { 0xdb, 0x30, 0x03, 0x0c };

static bool cmd_db_magic_matches(const struct cmd_db_header *header)
{
	const u8 *magic = header->magic;

	return memcmp(magic, CMD_DB_MAGIC, ARRAY_SIZE(CMD_DB_MAGIC)) == 0;
}

static struct cmd_db_header *cmd_db_header;

static inline const void *rsc_to_entry_header(const struct rsc_hdr *hdr)
{
	u16 offset = le16_to_cpu(hdr->header_offset);

	return cmd_db_header->data + offset;
}

static inline void *
rsc_offset(const struct rsc_hdr *hdr, const struct entry_header *ent)
{
	u16 offset = le16_to_cpu(hdr->data_offset);
	u16 loffset = le16_to_cpu(ent->offset);

	return cmd_db_header->data + offset + loffset;
}

static int cmd_db_get_header(const char *id, const struct entry_header **eh,
			     const struct rsc_hdr **rh)
{
	const struct rsc_hdr *rsc_hdr;
	const struct entry_header *ent;
	int i, j;
	u8 query[sizeof(ent->id)] __nonstring;

	/*
	 * Pad out query string to same length as in DB. NOTE: the output
	 * query string is not necessarily '\0' terminated if it bumps up
	 * against the max size. That's OK and expected.
	 */
	strncpy(query, id, sizeof(query));

	for (i = 0; i < MAX_SLV_ID; i++) {
		rsc_hdr = &cmd_db_header->header[i];
		if (!rsc_hdr->slv_id)
			break;

		ent = rsc_to_entry_header(rsc_hdr);
		for (j = 0; j < le16_to_cpu(rsc_hdr->cnt); j++, ent++) {
			if (memcmp(ent->id, query, sizeof(ent->id)) == 0) {
				if (eh)
					*eh = ent;
				if (rh)
					*rh = rsc_hdr;
				return 0;
			}
		}
	}

	return -ENODEV;
}

/**
 * cmd_db_read_addr() - Query command db for resource id address.
 *
 * @id: resource id to query for address
 *
 * Return: resource address on success, 0 on error
 *
 * This is used to retrieve resource address based on resource
 * id.
 */
u32 cmd_db_read_addr(const char *id)
{
	int ret;
	const struct entry_header *ent;

	ret = cmd_db_get_header(id, &ent, NULL);

	return ret < 0 ? 0 : le32_to_cpu(ent->addr);
}
EXPORT_SYMBOL(cmd_db_read_addr);

/**
 * cmd_db_read_aux_data() - Query command db for aux data.
 *
 *  @id: Resource to retrieve AUX Data on
 *  @len: size of data buffer returned
 *
 *  Return: pointer to data on success, error pointer otherwise
 */
const void *cmd_db_read_aux_data(const char *id, size_t *len)
{
	int ret;
	const struct entry_header *ent;
	const struct rsc_hdr *rsc_hdr;

	ret = cmd_db_get_header(id, &ent, &rsc_hdr);
	if (ret)
		return ERR_PTR(ret);

	if (len)
		*len = le16_to_cpu(ent->len);

	return rsc_offset(rsc_hdr, ent);
}
EXPORT_SYMBOL(cmd_db_read_aux_data);

/**
 * cmd_db_read_slave_id - Get the slave ID for a given resource address
 *
 * @id: Resource id to query the DB for version
 *
 * Return: cmd_db_hw_type enum on success, CMD_DB_HW_INVALID on error
 */
enum cmd_db_hw_type cmd_db_read_slave_id(const char *id)
{
	int ret;
	const struct entry_header *ent;
	u32 addr;

	ret = cmd_db_get_header(id, &ent, NULL);
	if (ret < 0)
		return CMD_DB_HW_INVALID;

	addr = le32_to_cpu(ent->addr);
	return (addr >> SLAVE_ID_SHIFT) & SLAVE_ID_MASK;
}
EXPORT_SYMBOL(cmd_db_read_slave_id);

int cmd_db_init(ofnode node)
{
	void __iomem *base;

	printf("%s(%s)\n", __func__, ofnode_get_name(node));

	base = (void __iomem *)ofnode_get_addr(node);
	if ((fdt_addr_t)base == FDT_ADDR_T_NONE) {
		printf("%s: Failed to read base address\n", __func__);
		return -ENOENT;
	}

	cmd_db_header = base;
	if (!cmd_db_magic_matches(cmd_db_header)) {
		printf("%s: Invalid Command DB Magic\n", __func__);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(cmd_db_init);
