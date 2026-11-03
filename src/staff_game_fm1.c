/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "staff_game_fm1.h"
#include "staff_game.h"

#define BG 0x0000u
#define CYAN 0x07FFu
#define GOLD 0xFEA0u
#define RED 0xF800u
#define GREEN 0x07E0u

static StaffGame game;
static StaffGame draw_snapshot;
static uint8_t initialized, note_active;
static uint8_t draw_band;
static unsigned draw_top;
static uint32_t previous_keys, previous_buttons, note_off_at;
static uint8_t sounding_midi;
static uint16_t band_pixels[FM1_SCREEN_DMA_PIXELS];
static const uint16_t frequencies[27] = {
    175,185,196,208,220,233,247,262,277,294,311,330,349,370,
    392,415,440,466,494,523,554,587,622,659,698,740,784
};
static __attribute__((minsize)) void stop_note(void);
/* Compact 5x7 alphabet and digits adapted from FM1 Open app/screen.c. */
static const uint8_t glyphs[36][5] = {
    {0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},
    {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},
    {0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},
    {0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},
    {0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},
    {0,0x41,0x7f,0x41,0},{0x20,0x40,0x41,0x3f,1},
    {0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},
    {0x7f,2,0x0c,2,0x7f},{0x7f,4,8,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},
    {0x3e,0x41,0x51,0x21,0x5e},{0x7f,9,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},
    {0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},
    {7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43}
};

static __attribute__((minsize)) void pixel(int x, int y, uint16_t color)
{
    if (x >= 0 && x < 240 && y >= (int)draw_top && y < (int)draw_top+16)
        band_pixels[(y-(int)draw_top)*240+x]=color;
}
static __attribute__((minsize)) void draw_line(int x1,int y1,int x2,int y2,uint16_t color)
{
    if (y1 == y2) {
        int left=x1<x2?x1:x2, right=x1>x2?x1:x2;
        if (left<0) left=0; if (right>239) right=239;
        if (y1>=(int)draw_top&&y1<(int)draw_top+16&&right>=left)
            for(int x=left;x<=right;x++) pixel(x,y1,color);
        return;
    }
    if (x1 == x2) {
        int top=y1<y2?y1:y2, bottom=y1>y2?y1:y2;
        if (top<0) top=0; if (bottom>239) bottom=239;
        if (x1>=0&&x1<240&&bottom>=top) for(int y=top;y<=bottom;y++) pixel(x1,y,color);
        return;
    }
    int dx=x2>x1?x2-x1:x1-x2, sx=x1<x2?1:-1;
    int dy=-(y2>y1?y2-y1:y1-y2), sy=y1<y2?1:-1, err=dx+dy;
    for (;;) { pixel(x1,y1,color); if(x1==x2&&y1==y2) break;
        int twice=2*err; if(twice>=dy){err+=dy;x1+=sx;} if(twice<=dx){err+=dx;y1+=sy;} }
}
static __attribute__((minsize)) void draw_circle(int x,int y,int r,uint16_t color,int filled)
{
    for(int dy=-r;dy<=r;dy++) {
        int half=0; while((half+1)*(half+1)+dy*dy<=r*r) ++half;
        if(filled) draw_line(x-half,y+dy,x+half,y+dy,color);
        else { pixel(x-half,y+dy,color); pixel(x+half,y+dy,color); }
    }
}
static __attribute__((minsize)) void draw_text(int x,int y,const char *s,uint16_t color)
{
    for(;*s;s++,x+=6) {
        char c=*s; if(c>='a'&&c<='z') c=(char)(c-'a'+'A');
        const uint8_t *g=0; uint8_t dash[5]={0,0,8,0,0};
        if(c>='0'&&c<='9') g=glyphs[c-'0'];
        else if(c>='A'&&c<='Z') g=glyphs[10+c-'A'];
        else if(c=='-') g=dash;
        if(!g) continue;
        for(int col=0;col<5;col++) {
            int row=0;
            while(row<7) {
                while(row<7&&!(g[col]&(1u<<row))) row++;
                int first=row;
                while(row<7&&(g[col]&(1u<<row))) row++;
                if(row>first) for(int py=first;py<row;py++) pixel(x+col,y+py,color);
            }
        }
    }
}
static __attribute__((minsize)) void tone(uint16_t hz,uint16_t ms)
{
    stop_note();
    if(note_active) return;
    unsigned midi=45;
    if(hz>=160) {
        unsigned best=0,delta=0xffff;
        for(unsigned i=0;i<27;i++) { unsigned d=hz>frequencies[i]?hz-frequencies[i]:frequencies[i]-hz;
            if(d<delta){delta=d;best=i;} }
        midi=53+best;
    }
    uint8_t on[3]={0x90,(uint8_t)midi,88};
    if(fm1_midi_post(on,3)==0) {
        fm1_control_request(); sounding_midi=(uint8_t)midi; note_active=1; note_off_at=fm1_millis()+ms;
    }
}
static __attribute__((minsize)) void clear(uint16_t color)
{
    for(unsigned i=0;i<FM1_SCREEN_DMA_PIXELS;i++) band_pixels[i]=color;
}
static const StaffPlatform platform={clear,draw_line,draw_circle,draw_text,tone};

static __attribute__((minsize)) void stop_note(void)
{
    if(note_active) {
        uint8_t off[3]={0x80,sounding_midi,0};
        if(fm1_midi_post(off,3)==0) { fm1_control_request(); note_active=0; }
    }
}

__attribute__((minsize)) int staff_game_fm1_control(const struct fm1_controls *c)
{
    enum { BTN_HOME=8, BTN_PLAY=12, BTN_REC=13 };
    if(!c) return 0;
    uint32_t now=fm1_millis(), key_edges=c->keys&~previous_keys;
    uint32_t button_edges=c->buttons&~previous_buttons;
    previous_keys=c->keys; previous_buttons=c->buttons;
    if(!initialized) { staff_game_init(&game,now); initialized=1; draw_band=0; }
    if((c->buttons&(1u<<BTN_HOME))&&(button_edges&(1u<<BTN_REC))) {
        stop_note(); staff_game_init(&game,now); draw_band=0; return 1;
    }
    if(note_active&&(int32_t)(now-note_off_at)>=0) stop_note();
    int turn=c->encoders[0]>0?1:c->encoders[0]<0?-1:0;
    staff_game_update(&game,now,key_edges,(uint8_t)((button_edges>>BTN_PLAY)&1u),
                      (uint8_t)((button_edges>>BTN_HOME)&1u),(int8_t)turn,&platform);
    if(fm1_screen_pending()==0) {
        if(draw_band==0) draw_snapshot=game;
        draw_top=(unsigned)draw_band*16u;
        staff_game_draw(&draw_snapshot,&platform);
        int submitted=fm1_screen_submit(0,draw_top,240,16,band_pixels,FM1_SCREEN_DMA_PIXELS);
        if(submitted==0) draw_band=(uint8_t)((draw_band+1u)%15u);
        else if(submitted<0) draw_band=0;
    }
    return 1;
}
