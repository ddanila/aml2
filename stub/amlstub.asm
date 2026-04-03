.model tiny
.286
.code
org 100h

start:
    mov ax, cs
    mov ds, ax
    mov es, ax
    cli
    mov ss, ax
    mov sp, offset stack_top
    sti

    mov bx, offset resident_end
    add bx, 15
    shr bx, 1
    shr bx, 1
    shr bx, 1
    shr bx, 1
    mov ah, 4Ah
    int 21h
    jc resize_fail

    mov ax, [2Ch]
    mov [env_seg], ax
    mov [cmd_tail_seg], cs
    mov [fcb1_seg], cs
    mov [fcb2_seg], cs

    call init_home

main_loop:
    call restore_home
    call delete_run_file

    mov word ptr [cmd_tail_off], offset empty_tail
    lea dx, launcher_name
    call exec_wait
    jc launcher_fail

    call read_run_request
    jc exit_ok

    call delete_run_file
    call launch_selected
    jc command_fail
    jmp main_loop

launcher_fail:
    lea dx, msg_launcher_fail
    call print_dollar
    jmp exit_err

command_fail:
    lea dx, msg_command_fail
    call print_dollar
    lea dx, command_buf
    call print_asciiz
    lea dx, crlf
    call print_dollar
    jmp exit_err

resize_fail:
    lea dx, msg_resize_fail
    call print_dollar
    jmp exit_err

exit_ok:
    mov ax, 4C00h
    int 21h

exit_err:
    mov ax, 4C01h
    int 21h

init_home proc near
    mov ah, 19h
    int 21h
    mov [home_drive], al

    mov byte ptr [home_path], '\'
    lea si, home_path + 1
    mov dl, 0
    mov ah, 47h
    int 21h
    jc init_home_done

    cmp byte ptr [home_path + 1], 0
    jne init_home_done
    mov byte ptr [home_path + 1], 0

init_home_done:
    ret
init_home endp

restore_home proc near
    mov dl, [home_drive]
    mov ah, 0Eh
    int 21h

    lea dx, home_path
    mov ah, 3Bh
    int 21h
    ret
restore_home endp

delete_run_file proc near
    lea dx, run_file_name
    mov ah, 41h
    int 21h
    ret
delete_run_file endp

read_run_request proc near
    mov ax, 3D00h
    lea dx, run_file_name
    int 21h
    jc read_fail

    mov bx, ax
    mov ah, 3Fh
    mov cx, RUN_BUF_SIZE - 1
    lea dx, run_buf
    int 21h
    jc read_close_fail

    mov si, ax
    mov byte ptr [run_buf + si], 0

    mov ah, 3Eh
    int 21h

    cmp si, 0
    je read_fail

    lea si, run_buf
    lea di, command_buf
    call parse_line
    cmp byte ptr [command_buf], 0
    je read_fail

    lea di, path_buf
    call parse_line
    clc
    ret

read_close_fail:
    pushf
    mov ah, 3Eh
    int 21h
    popf

read_fail:
    stc
    ret
read_run_request endp

parse_line proc near
parse_copy:
    lodsb
    cmp al, 0
    je parse_done
    cmp al, 0Dh
    je parse_skip
    cmp al, 0Ah
    je parse_skip
    stosb
    jmp parse_copy

parse_skip:
    cmp al, 0
    je parse_done
parse_skip_more:
    lodsb
    cmp al, 0Dh
    je parse_skip_more
    cmp al, 0Ah
    je parse_skip_more
    cmp al, 0
    je parse_done
    dec si

parse_done:
    mov al, 0
    stosb
    ret
parse_line endp

launch_selected proc near
    lea si, path_buf
    cmp byte ptr [si], 0
    je path_done

    cmp byte ptr [si + 1], ':'
    jne path_setdir

    mov dl, [si]
    and dl, 5Fh
    sub dl, 'A'
    mov ah, 0Eh
    int 21h
    add si, 2

path_setdir:
    cmp byte ptr [si], 0
    je path_done
    mov dx, si
    mov ah, 3Bh
    int 21h
    jc launch_fail_ret

path_done:
    call build_cmd_tail
    mov word ptr [cmd_tail_off], offset cmd_tail
    lea dx, command_shell
    call exec_wait
    ret

launch_fail_ret:
    stc
    ret
launch_selected endp

build_cmd_tail proc near
    lea di, cmd_tail + 1

    mov al, '/'
    stosb
    mov al, 'C'
    stosb
    mov al, ' '
    stosb

    lea si, command_buf
    mov cx, 123

build_loop:
    cmp cx, 0
    je build_done
    lodsb
    cmp al, 0
    je build_done
    stosb
    dec cx
    jmp build_loop

build_done:
    mov bx, di
    sub bx, offset cmd_tail + 1
    mov [cmd_tail], bl
    mov al, 0Dh
    stosb
    ret
build_cmd_tail endp

exec_wait proc near
    lea bx, exec_block
    mov ax, 4B00h
    int 21h
    ret
exec_wait endp

print_dollar proc near
    mov ah, 09h
    int 21h
    ret
print_dollar endp

print_asciiz proc near
    push ax
    push si
    push dx
    mov si, dx

print_loop:
    mov al, [si]
    cmp al, 0
    je print_done
    mov ah, 02h
    mov dl, al
    int 21h
    inc si
    jmp print_loop

print_done:
    pop dx
    pop si
    pop ax
    ret
print_asciiz endp

launcher_name    db 'AML2.EXE',0
command_shell    db 'COMMAND.COM',0
run_file_name    db 'AML2.RUN',0
msg_launcher_fail db 'AMLSTUB: failed to start AML2.EXE',13,10,'$'
msg_command_fail  db 'AMLSTUB: failed to launch selected command',13,10,'$'
msg_resize_fail   db 'AMLSTUB: failed to resize memory block',13,10,'$'
crlf             db 13,10,'$'

home_drive       db 0
home_path        db 66 dup (0)

command_buf      db 128 dup (0)
path_buf         db 64 dup (0)

RUN_BUF_SIZE     equ 255
run_buf          db RUN_BUF_SIZE dup (0)

empty_tail       db 0,0Dh
cmd_tail         db 127 dup (0)

exec_block label byte
env_seg          dw 0
cmd_tail_off     dw offset empty_tail
cmd_tail_seg     dw 0
fcb1_off         dw 5Ch
fcb1_seg         dw 0
fcb2_off         dw 6Ch
fcb2_seg         dw 0

stack_space      db 128 dup (0)
stack_top label byte
resident_end label byte

end start
