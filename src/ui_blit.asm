.model small
.code

public ui_blit_row_

ui_blit_row_ proc near
    mov cx, 80
    rep movsw
    ret
ui_blit_row_ endp

end
