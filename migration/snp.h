/*
 * QEMU SEV-SNP confidential machine migration
 *
 * Authors:
 *  Jakub Růžička <jakub.ruzicka@matfyz.cz>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#ifndef MIGRATION_SNP_H
#define MIGRATION_SNP_H

#include "migration/migration.h"

// Used in communication with SVSM
#define SNP_MIGRATION_STATUS_NOT_STARTED 0x0
#define SNP_MIGRATION_STATUS_INCOMING 0x1
#define SNP_MIGRATION_STATUS_RUNNING 0x2
#define SNP_MIGRATION_STATUS_COMPLETED 0x3

#define SNP_MIGRATION_DATA_READY 0x4
#define SNP_MIGRATION_DATA_READ 0x5
#define SNP_MIGRATION_DATA_ADDRESS 0x6
#define SNP_MIGRATION_DATA_VALIDATED 0x7

// SVSM migration page layout
#define STATUS_REGISTER_OFFSET 0x0
#define DATA_REGISTER_OFFSET 0x1
#define ADDRESS_REGISTER_OFFSET 0x2
#define DATA_BUFFER_OFFSET 0xA
#define DATA_BUFFER_SIZE 0x1000

void snp_start_migration_handler(void);
void snp_stop_migration_handler(void);
uint64_t snp_read_validated_pages(void);
uint64_t snp_package_page(ram_addr_t guest_physical_addr);

#endif
