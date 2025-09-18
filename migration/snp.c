/*
 * QEMU SEV-SNP confidential machine migration
 *
 * Authors:
 *  Jakub Růžička <jakub.ruzicka@matfyz.cz>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "qemu-file.h"
#include "qemu/main-loop.h"
#include "io/channel-socket.h"
#include "snp.h"
#include "trace.h"
#include "qapi/error.h"

/* Helper function to manage reading from and writing to the migration page
 * registers.
 */
static void write_status_register(uint8_t value) {
    MigrationState *s = migrate_get_current();
    uint64_t pa = s->svsm_migration_page + STATUS_REGISTER_OFFSET;
    uint8_t buf[1] = {value};
    cpu_physical_memory_write(pa, buf, 1);
}

static uint8_t read_data_register(void) {
    MigrationState *s = migrate_get_current();
    uint64_t pa = s->svsm_migration_page + DATA_REGISTER_OFFSET;
    uint8_t buf[1] = {0};
    cpu_physical_memory_read(pa, buf, 1);
    return buf[0];
}

static void write_data_register(uint8_t value) {
    MigrationState *s = migrate_get_current();
    uint64_t pa = s->svsm_migration_page + DATA_REGISTER_OFFSET;
    uint8_t buf[1] = {value};
    cpu_physical_memory_write(pa, buf, 1);
}

static void write_address_register(ram_addr_t guest_physical_addr) {
    MigrationState *s = migrate_get_current();
    uint64_t pa = s->svsm_migration_page + ADDRESS_REGISTER_OFFSET;
    uint8_t buf[8];
    memcpy(buf, &guest_physical_addr, sizeof(buf));
    cpu_physical_memory_write(pa, buf, sizeof(buf));
}

// Transform the bytes to uint64_t big-endian
static inline uint64_t u64_from_be_bytes(const uint8_t b[8]) {
    return ((uint64_t)b[0] << 56) |
           ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) |
           ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) |
           ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] << 8)  |
           ((uint64_t)b[7]);
}

/* Start migration handler inside the SVSM */
void snp_start_migration_handler(void) {
    write_data_register(SNP_MIGRATION_DATA_READ);
    write_status_register(SNP_MIGRATION_STATUS_RUNNING);
}

void snp_read_validated_pages(void) {
    MigrationState *s = migrate_get_current();
    uint64_t pa = s->svsm_migration_page + DATA_BUFFER_OFFSET;

    //uint64_t pages[4096]; 
    uint64_t total = 0;
    while (true) {
        uint8_t status = read_data_register();
        if (status == SNP_MIGRATION_DATA_READY) {
            break;
        }
        else if (status == SNP_MIGRATION_DATA_VALIDATED) {
            // Process the pages
            uint8_t buf[4096];
            cpu_physical_memory_read(pa, buf, DATA_BUFFER_SIZE);
            uint64_t page_count = u64_from_be_bytes(buf);
            total += page_count;
            write_data_register(SNP_MIGRATION_DATA_READ);
        }
    }
    qemu_log("snp: validated pages %lu\n", total);
    write_data_register(SNP_MIGRATION_DATA_READ);
}

static uint64_t make_u64_be(const uint8_t *buf) {
    uint64_t v = 0;
    size_t i;
    for (i = 0; i < 8; ++i) {
        v = (v << 8) | (uint64_t)buf[i];
    }
    return v;
}

/* Send signal to migration handler that the migration is completed */
void snp_stop_migration_handler(void) {
    write_status_register(SNP_MIGRATION_STATUS_COMPLETED);
    while (read_data_register() != SNP_MIGRATION_DATA_READY) {}
    // Read 8 bytes and make number out of it
    uint8_t buf[8];
    MigrationState *s = migrate_get_current();
    cpu_physical_memory_read(s->svsm_migration_page + ADDRESS_REGISTER_OFFSET, buf, 8);
    uint64_t pages_read = make_u64_be(buf);
    qemu_log("snp: pages read: %lu\n", pages_read); 
}

uint64_t snp_package_page(ram_addr_t guest_physical_addr) {
    // Send address to the guest
    write_address_register(guest_physical_addr);
    write_data_register(SNP_MIGRATION_DATA_ADDRESS);

    // Wait for data to be ready
    while (read_data_register() != SNP_MIGRATION_DATA_READY) {}

    // Returning the pointer to the buffer holding the packaged page.
    MigrationState *s = migrate_get_current();
    return s->svsm_migration_page + DATA_BUFFER_OFFSET;
}
