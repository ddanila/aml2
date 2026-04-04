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
    call init_mode

main_loop:
    call restore_home
    call delete_run_file

    mov ax, [launcher_tail_ptr]
    mov word ptr [cmd_tail_off], ax
    mov byte ptr [debug_pause], 0
    lea dx, launcher_name
    call exec_wait
    jc launcher_fail
    mov ah, 4Dh
    int 21h
    cmp al, 2
    je launcher_requested_run
    cmp al, 3
    jne launcher_check_exit
    mov byte ptr [debug_pause], 1
    jmp launcher_requested_run

launcher_check_exit:
    or al, al
    jne launcher_fail
    jmp exit_ok

launcher_requested_run:
    call read_run_request
    jc exit_ok

    call delete_run_file
    call launch_selected
    jc command_fail
    call pause_on_child_error
    jmp main_loop

launcher_fail:
    lea dx, msg_launcher_fail
    call print_dollar
    jmp exit_err

command_fail:
    lea dx, msg_command_fail
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
    add al, 'A'
    mov [command_shell], al

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

init_mode proc near
    mov word ptr [launcher_tail_ptr], offset view_tail

    mov cl, [80h]
    jcxz init_mode_done
    mov si, 81h

init_mode_skip_spaces:
    cmp cl, 0
    je init_mode_done
    lodsb
    dec cl
    cmp al, ' '
    je init_mode_skip_spaces
    cmp al, 9
    je init_mode_skip_spaces
    cmp al, '/'
    je init_mode_switch
    cmp al, '-'
    jne init_mode_done

init_mode_switch:
    cmp cl, 0
    je init_mode_done
    lodsb
    and al, 5Fh
    cmp al, '?'
    je init_mode_help
    cmp al, 'E'
    jne init_mode_done
    mov word ptr [launcher_tail_ptr], offset edit_tail
    jmp init_mode_done

init_mode_help:
    lea dx, msg_usage
    call print_dollar
    jmp exit_ok

init_mode_done:
    ret
init_mode endp

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
    lea di, cmd_buf
    mov cx, 123
    call read_line
    jc read_close_fail
    or al, al
    jz read_close_fail

    lea di, path_buf
    mov cx, 63
    call read_line
    jc read_close_fail

    mov ah, 3Eh
    int 21h
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

read_line proc near
    push bx
    xor si, si

read_line_next:
    mov ah, 3Fh
    mov cx, 1
    lea dx, read_char
    int 21h
    jc read_line_fail
    or ax, ax
    jz read_line_done

    mov al, [read_char]
    cmp al, 0Dh
    je read_line_skip
    cmp al, 0Ah
    je read_line_skip
    or cx, cx
    jz read_line_next
    stosb
    inc si
    dec cx
    jmp read_line_next

read_line_skip:
    mov ah, 3Fh
    mov cx, 1
    lea dx, read_char
    int 21h
    jmp read_line_done

read_line_fail:
    pop bx
    stc
    ret

read_line_done:
    xor al, al
    stosb
    mov ax, si
    pop bx
    clc
    ret
read_line endp

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
    call is_direct_command
    or al, al
    jz launch_via_shell

    mov word ptr [cmd_tail_off], offset empty_tail
    lea dx, cmd_buf
    call exec_wait
    ret

launch_via_shell:
    call build_shell_tail
    mov word ptr [cmd_tail_off], offset cmd_tail
    lea dx, command_shell
    call exec_wait
    ret

launch_fail_ret:
    stc
    ret
launch_selected endp

exec_wait proc near
    lea bx, exec_block
    mov ax, 4B00h
    int 21h
    ret
exec_wait endp

pause_on_child_error proc near
    cmp byte ptr [debug_pause], 0
    je child_error_done
    mov ah, 4Dh
    int 21h
    or ah, ah
    jne child_error_pause
    or al, al
    jne child_error_pause
child_error_done:
    ret

child_error_pause:
    lea dx, msg_child_error
    call print_dollar
    mov ah, 08h
    int 21h
    ret
pause_on_child_error endp

is_direct_command proc near
    lea si, cmd_buf
    xor bx, bx

is_direct_scan:
    mov al, [si]
    or al, al
    jz is_direct_check_ext
    cmp al, ' '
    je is_direct_no
    cmp al, 9
    je is_direct_no
    cmp al, '"'
    je is_direct_no
    cmp al, '%'
    je is_direct_no
    cmp al, '&'
    je is_direct_no
    cmp al, '|'
    je is_direct_no
    cmp al, '<'
    je is_direct_no
    cmp al, '>'
    je is_direct_no
    cmp al, '.'
    jne is_direct_next
    mov bx, si

is_direct_next:
    inc si
    jmp is_direct_scan

is_direct_check_ext:
    or bx, bx
    jz is_direct_no
    mov si, bx
    inc si

    mov al, [si]
    and al, 5Fh
    cmp al, 'E'
    jne is_direct_check_com
    mov al, [si + 1]
    and al, 5Fh
    cmp al, 'X'
    jne is_direct_no
    mov al, [si + 2]
    and al, 5Fh
    cmp al, 'E'
    jne is_direct_no
    cmp byte ptr [si + 3], 0
    jne is_direct_no
    mov al, 1
    ret

is_direct_check_com:
    mov al, [si]
    and al, 5Fh
    cmp al, 'C'
    jne is_direct_no
    mov al, [si + 1]
    and al, 5Fh
    cmp al, 'O'
    jne is_direct_no
    mov al, [si + 2]
    and al, 5Fh
    cmp al, 'M'
    jne is_direct_no
    cmp byte ptr [si + 3], 0
    jne is_direct_no
    mov al, 1
    ret

is_direct_no:
    xor al, al
    ret
is_direct_command endp

build_shell_tail proc near
    lea si, cmd_buf
    lea di, cmd_tail + 1
    mov ax, 'C/'
    stosw
    mov al, ' '
    stosb
    mov bl, 3
    mov cx, 123

build_shell_tail_copy:
    mov al, [si]
    or al, al
    jz build_shell_tail_done
    or cx, cx
    jz build_shell_tail_done
    stosb
    inc si
    inc bl
    dec cx
    jmp build_shell_tail_copy

build_shell_tail_done:
    mov [cmd_tail], bl
    mov byte ptr [di], 0Dh
    ret
build_shell_tail endp

print_dollar proc near
    mov ah, 09h
    int 21h
    ret
print_dollar endp

launcher_name     db 'AMLUI.EXE',0
command_shell     db 'A:\COMMAND.COM',0
run_file_name     db 'AML2.RUN',0
msg_launcher_fail db 'NO AMLUI.EXE',13,10,'$'
msg_command_fail  db 'RUN FAILED',13,10,'$'
msg_resize_fail   db 'NO MEMORY',13,10,'$'
msg_child_error   db 13,10,'PROGRAM ERROR - PRESS A KEY',13,10,'$'
msg_usage         db 'AML usage: AML [/E]',13,10,'$'

home_drive       db 0
debug_pause      db 0
home_path        db 64 dup (0)
launcher_tail_ptr dw offset view_tail

path_buf         db 64 dup (0)
read_char        db 0

view_tail        db 6,' /V /S',0Dh
edit_tail        db 6,' /E /S',0Dh
empty_tail       db 0
               db 0Dh
cmd_buf          db 124 dup (0)
cmd_tail         db 127 dup (0)

exec_block label byte
env_seg          dw 0
cmd_tail_off     dw offset empty_tail
cmd_tail_seg     dw 0
fcb1_off         dw 5Ch
fcb1_seg         dw 0
fcb2_off         dw 6Ch
fcb2_seg         dw 0

stack_space      db 64 dup (0)
stack_top label byte
resident_end label byte

end start
