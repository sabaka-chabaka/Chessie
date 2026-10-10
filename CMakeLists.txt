cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(Chessie C ASM)

add_executable(Chessie.elf
        src/boot.S
        src/cpu/gdt_flush.S
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

add_custom_target(iso ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${ISO_DIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:Chessie.elf> ${ISO_DIR}/boot/Chessie.elf
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/grub.cfg ${ISO_DIR}/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o ${ISO_FILE} ${ISO_DIR}
        DEPENDS Chessie.elf ${CMAKE_SOURCE_DIR}/grub.cfg
        COMMENT "Building Chessie.iso"
)
add_dependencies(iso Chessie.elf)

add_custom_target(run
        COMMAND qemu-system-i386 -cdrom ${ISO_FILE} -serial stdio
        DEPENDS iso
)