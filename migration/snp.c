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

/* Start migration handler inside the SVSM */
void snp_start_migration_handler(void) {
    write_data_register(SNP_MIGRATION_DATA_READ);
    write_status_register(SNP_MIGRATION_STATUS_RUNNING);
}

/* Send signal to migration handler that the migration is completed */
void snp_stop_migration_handler(void) {
     write_status_register(SNP_MIGRATION_STATUS_COMPLETED);
}

/* Helper function */
// static bool all_vcpus_running(void)
// {
//     CPUState *cpu;
//     CPU_FOREACH(cpu) {
//         if (cpu->cpu_index == 3) {
//             qemu_log("CPU kick\n");
//             qemu_cpu_kick(cpu);
//         }
//         /*
//         if (cpu->stopped) {
//             qemu_log("KUBA: CPU stopped %d\n", cpu->cpu_index);
//             //return false;
//         }
//         if (cpu->stopped && cpu->cpu_index == 3) {
//             qemu_log("Before resume: Stopped: %b\n", cpu->stopped);
//             qemu_log("CPU stopped %d\n", cpu->cpu_index);
//             //cpu_resume(cpu);
//             qemu_log("Running %b\n", cpu->running);
//             qemu_log("Stopped %b\n", cpu->stopped);
//             //return true;
//             //return false;
//         }
//         */
//     }
//     return true;
// }

uint64_t snp_package_page(ram_addr_t guest_physical_addr) {
    // Send address to the guest
    write_address_register(guest_physical_addr);
    write_data_register(SNP_MIGRATION_DATA_ADDRESS);

    MigrationState *s = migrate_get_current();
    // Wait for data to be ready
    uint64_t value = 0x0; // Value different from SNP_MIGRATION_DATA_READY

    uint64_t iterations = 0;
    do {
        value = read_data_register();
        iterations += 1;
        //qemu_log("Data register: %lx\n", value);
        if (iterations % 10000000 == 0) {
            CPUState *cpu;
            CPU_FOREACH(cpu) {
                if (cpu->cpu_index == 3) {
                    qemu_log("CPU kick\n");
                    qemu_cpu_kick(cpu);
                }
            }
            iterations = 0;
        }
        /*
        if (!all_vcpus_running()) {
            return s->svsm_migration_page + DATA_BUFFER_OFFSET;
        }
        */
    } while (value != SNP_MIGRATION_DATA_READY);
    qemu_log("guest_physical_addr: %lx\n", guest_physical_addr);
    // Returning the pointer to the buffer holding the packaged page.
    return s->svsm_migration_page + DATA_BUFFER_OFFSET;
}
