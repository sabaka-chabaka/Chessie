cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(Chessie C ASM)

add_executable(Chessie.elf
        src/boot.S
        src/cpu/gdt_flush.S
        src/cpu/isr.s
        src/cpu/irq.S
        src/cpu/switch.S
        src/cpu/syscall.S
        src/cpu/usermode.S
        src/kernel/usertest.S
        src/kernel/main.c
        src/drivers/vga.c
        src/include/string.h
        src/lib/string.c
        src/include/kprintf.h
        src/kernel/kprintf.c
        src/include/serial.h
        src/drivers/serial.c
        src/include/gdt.h
        src/cpu/gdt.c
        src/include/idt.h
        src/cpu/isr.c
        src/cpu/idt.c
        src/cpu/idt.c
        src/include/pic.h
        src/include/irq.h
        src/cpu/pic.c
        src/cpu/irq.c
        src/include/pit.h
        src/drivers/pit.c
        src/include/keyboard.h
        src/drivers/keyboard.c
        src/include/shell.h
        src/kernel/shell.c
        src/include/multiboot.h
        src/include/memmap.h
        src/kernel/memmap.c
        src/include/pmm.h
        src/mm/pmm.c
        src/include/vmm.h
        src/mm/vmm.c
        src/mm/vmm.c
        src/include/heap.h
        src/mm/heap.c
        src/include/thread.h
        src/include/sched.h
        src/include/sync.h
        src/kernel/thread.c
        src/kernel/sched.c
        src/kernel/sync.c
        src/include/tss.h
        src/cpu/tss.c
        src/include/syscall.h
        src/cpu/syscall.c
        src/include/aspace.h
        src/mm/aspace.c
        src/include/usertest.h
        src/kernel/usertest.c
        src/include/initrd.h
        src/kernel/initrd.c
        src/include/vfs.h
        src/fs/vfs.c
        src/include/tarfs.h
        src/fs/tarfs.c
        src/include/elf.h
        src/kernel/elf.c
        src/include/usermode.h
        src/include/process.h
        src/kernel/process.c
)

target_include_directories(Chessie.elf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)

target_compile_options(Chessie.elf PRIVATE
        -m32 -fno-pie -fno-stack-protector
        -fcf-protection=none -fno-asynchronous-unwind-tables
        -ffreestanding -O2 -Wall -Wextra
)

target_link_options(Chessie.elf PRIVATE
        -m32 -no-pie -nostdlib -ffreestanding
        -Wl,-z,noexecstack
        -Wl,--build-id=none
        -T ${CMAKE_SOURCE_DIR}/linker.ld
)

set_target_properties(Chessie.elf PROPERTIES LINK_DEPENDS ${CMAKE_SOURCE_DIR}/linker.ld)

set(ISO_DIR  ${CMAKE_BINARY_DIR}/isodir)
set(ISO_FILE ${CMAKE_BINARY_DIR}/Chessie.iso)

set(INITRD_DIR ${CMAKE_SOURCE_DIR}/initrd)
set(INITRD_STAGE ${CMAKE_BINARY_DIR}/initrd_stage)
set(INITRD_TAR ${CMAKE_BINARY_DIR}/initrd.tar)

set(USER_PROGS hello args exitcode crash layout argcheck spawner)
set(USER_TARGETS "")
set(INITRD_BIN_FILES "")

foreach(p IN LISTS USER_PROGS)
    add_executable(user_${p} user/${p}.c user/crt0.S)
    target_include_directories(user_${p} PRIVATE ${CMAKE_SOURCE_DIR}/user)
    target_compile_options(user_${p} PRIVATE
            -m32 -fno-pie -fno-stack-protector -fcf-protection=none
            -fno-asynchronous-unwind-tables -fno-tree-loop-distribute-patterns
            -ffreestanding -O2 -Wall -Wextra
    )
    target_link_options(user_${p} PRIVATE
            -m32 -static -no-pie -nostdlib -ffreestanding
            -Wl,--build-id=none -Wl,-z,noexecstack -Wl,-z,max-page-size=0x1000
            -T ${CMAKE_SOURCE_DIR}/user/user.ld
    )
    set_target_properties(user_${p} PROPERTIES
            OUTPUT_NAME ${p}
            RUNTIME_OUTPUT_DIRECTORY ${INITRD_STAGE}/bin
            LINK_DEPENDS ${CMAKE_SOURCE_DIR}/user/user.ld
    )
    list(APPEND USER_TARGETS user_${p})
    list(APPEND INITRD_BIN_FILES bin/${p})
endforeach()

file(GLOB_RECURSE INITRD_FILES CONFIGURE_DEPENDS RELATIVE ${INITRD_DIR} ${INITRD_DIR}/*)
if(NOT INITRD_FILES)
    message(FATAL_ERROR "initrd/ is empty")
endif()
list(TRANSFORM INITRD_FILES PREPEND ${INITRD_DIR}/ OUTPUT_VARIABLE INITRD_DEPS)

add_custom_command(
        OUTPUT ${INITRD_TAR}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${INITRD_DIR} ${INITRD_STAGE}
        COMMAND ${CMAKE_COMMAND} -E tar cf ${INITRD_TAR} --format=gnutar -- ${INITRD_FILES} ${INITRD_BIN_FILES}
        WORKING_DIRECTORY ${INITRD_STAGE}
        DEPENDS ${INITRD_DEPS} ${USER_TARGETS}
        COMMENT "Packing initrd.tar"
        VERBATIM
)

add_custom_target(iso ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Chessie.elf> ${ISO_DIR}/boot/Chessie.elf
        COMMAND ${CMAKE_COMMAND} -E copy ${INITRD_TAR} ${ISO_DIR}/boot/initrd.tar
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub.cfg ${ISO_DIR}/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o ${ISO_FILE} ${ISO_DIR}
        DEPENDS Chessie.elf ${INITRD_TAR} ${CMAKE_SOURCE_DIR}/grub.cfg
        COMMENT "Building Chessie.iso"
)
add_dependencies(iso Chessie.elf)

add_custom_target(run
        COMMAND qemu-system-i386 -cdrom ${ISO_FILE} -serial stdio -m 128M
        DEPENDS iso
)