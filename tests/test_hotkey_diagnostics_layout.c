/* Production responsive status geometry; not a Win32 rendering test. */
#include "../src/windows/ui_contract.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    const int widths[]={320,450,550,740,1100};
    for(size_t i=0;i<sizeof widths/sizeof *widths;++i) {
        UiLayout layout; assert(ui_layout_compute(widths[i],&layout));
        const UiRect *status=&layout.controls[UI_STATUS];
        assert(status->height==(widths[i]>=700?240:320));
        assert(status->x+status->width<=widths[i]);
        assert(status->y+status->height+10==layout.content_height);
        printf("Diagnostic status geometry: %d DIP width, %d DIP status, full scroll extent retained\n",widths[i],status->height);
    }
    return 0;
}
