/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#include "staff_game_fm1.h"
#include "staff_game.h"
#include "instrument.h"
#include "daughter_qing_events.h"
#include <string.h>

#define BG 0x0000u
#define CYAN 0x07FFu
#define GOLD 0xFEA0u
#define RED 0xF800u
#define GREEN 0x07E0u
#define WHITE 0xFFFFu

static StaffGame game;
static StaffGame draw_snapshot;
static uint8_t initialized, note_active;
static uint8_t draw_band;
static unsigned draw_top;
static uint32_t previous_keys, previous_buttons, note_off_at;
static uint8_t sounding_midi;
static struct { uint8_t note, on; } midi_events[32];
static volatile uint8_t midi_write, midi_read, midi_overflow;
static uint32_t midi_held, midi_pressed;
static uint16_t dq_player_index, dq_accomp_index;
static uint8_t dq_started, dq_waiting, dq_note_played, dq_stopping;
static uint8_t dq_drum_available, dq_drum_channel;
static uint32_t dq_elapsed_ms, dq_last_ms, dq_draw_elapsed_ms;
static uint16_t dq_draw_player_index;
static uint8_t dq_draw_waiting;
static uint8_t dq_active[3][128];
static uint32_t dq_note_off[27];
static uint8_t dq_note_off_pending[27];
#define DQ_LEAD_MS 4050u
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
    if (y + 7 <= (int)draw_top || y >= (int)draw_top + 16) return;
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
    if(groove_midi_local(on,3)==0) {
        fm1_control_request(); sounding_midi=(uint8_t)midi; note_active=1; note_off_at=fm1_millis()+ms;
    }
}
static __attribute__((minsize)) void clear(uint16_t color)
{
    for(unsigned i=0;i<FM1_SCREEN_DMA_PIXELS;i++) band_pixels[i]=color;
}
static const StaffPlatform platform={clear,draw_line,draw_circle,draw_text,tone};

static uint32_t dq_tick_ms(uint32_t tick)
{
    return (tick * 5u) / 3u; /* MIDI tempo 75 BPM, 480 ticks per quarter. */
}

static int dq_midi(uint8_t lane, uint8_t note, uint8_t velocity)
{
    if (lane == 1u && !dq_drum_available) return 0;
    unsigned channel = lane == 1u ? dq_drum_channel : groove_midi_input_channel(lane);
    uint8_t message[3] = {(uint8_t)((velocity ? 0x90u : 0x80u) |
        channel), note, velocity};
    return groove_midi_local(message, 3);
}

static int dq_note_y(StaffClef clef, unsigned note)
{
    static const uint8_t letter[12] = {0,0,1,1,2,3,3,4,4,5,5,6};
    int octave = (int)(note / 12u) - 1;
    int diatonic = octave * 7 + letter[note % 12u];
    int bottom = clef == STAFF_TREBLE ? 30 : 18; /* E4 / G2 */
    return 168 - (diatonic - bottom) * 12;
}

static void dq_draw(const StaffGame *g, uint32_t elapsed, uint16_t player_index, uint8_t waiting)
{
    for (unsigned i = player_index; i < DQ_PLAYER_COUNT; ++i) {
        uint32_t due = DQ_LEAD_MS + dq_tick_ms(dq_player[i].tick);
        if (due + 20u < elapsed) continue;
        uint32_t ahead = due > elapsed ? due - elapsed : 0;
        int x = 52 + (int)(ahead * 42u / 1000u);
        if (x > 232) break;
        uint16_t color = (waiting && i == player_index) ? RED : GOLD;
        uint32_t mask = dq_player[i].mask;
        for (unsigned k = 0; k < 27; ++k) if (mask & (1u << k)) {
            unsigned pitch = 53u + k;
            int y = dq_note_y(g->clef, pitch);
            if (y + 6 < (int)draw_top || y - 6 >= (int)draw_top + 16) continue;
            int pc = pitch % 12u;
            if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10) {
                draw_line(x - 14, y - 3, x - 10, y - 3, color);
                draw_line(x - 14, y + 2, x - 10, y + 2, color);
            }
            if (y < 72) for (int ly=48; ly>=y-6; ly-=24) draw_line(x-12,ly,x+12,ly,WHITE);
            if (y > 168) for (int ly=192; ly<=y+6; ly+=24) draw_line(x-12,ly,x+12,ly,WHITE);
            draw_circle(x, y, 6, color, 1);
        }
    }
}

static void dq_refresh_stopping(void)
{
    if (!dq_stopping) return;
    for (unsigned lane = 0; lane < 3; ++lane)
        for (unsigned note = 0; note < 128; ++note)
            if (dq_active[lane][note]) {
                if (dq_midi((uint8_t)lane, (uint8_t)note, 0) == 0)
                    dq_active[lane][note] = 0;
            }
    for (unsigned lane = 0; lane < 3; ++lane)
        for (unsigned note = 0; note < 128; ++note)
            if (dq_active[lane][note]) return;
    for (unsigned k = 0; k < 27; ++k)
        if (dq_note_off_pending[k]) return;
    dq_stopping = 0;
}

static void dq_service_note_offs(uint32_t now)
{
    for (unsigned k = 0; k < 27; ++k)
        if (dq_note_off_pending[k] && (int32_t)(now - dq_note_off[k]) >= 0 &&
            dq_midi(0, (uint8_t)(53u + k), 0) == 0) {
            dq_note_off_pending[k] = 0;
            dq_note_off[k] = 0;
        }
}

static void dq_stop_backing(void)
{
    uint32_t now = fm1_millis();
    dq_stopping = 1;
    for (unsigned lane = 0; lane < 3; ++lane)
        for (unsigned note = 0; note < 128; ++note)
            if (dq_active[lane][note] && dq_midi((uint8_t)lane, (uint8_t)note, 0) == 0)
                dq_active[lane][note] = 0;
    for (unsigned k = 0; k < 27; ++k)
        if (dq_note_off_pending[k]) {
            if (dq_midi(0, (uint8_t)(53u + k), 0) == 0) {
                dq_note_off_pending[k] = 0;
                dq_note_off[k] = 0;
            } else {
                dq_note_off[k] = now; /* Retry from the next control tick. */
            }
        }
    dq_refresh_stopping();
}

static void dq_start(StaffGame *g, uint32_t now)
{
    /* Route the MIDI drum lane to an already configured 808 track. Do not
     * change a user's source or clear that track's incompatible sequence. */
    if (dq_stopping) return;
    dq_drum_available = 0;
    for (unsigned track = 0; track < GROOVE_TRACKS; ++track)
        if (groove_source_type(track) == GROOVE_808 && groove_track_role(track) != GROOVE_TRACK_OFF) {
            dq_drum_channel = (uint8_t)groove_midi_input_channel(track);
            dq_drum_available = 1;
            break;
        }
    g->note_index = 0;
    dq_player_index = dq_accomp_index = 0;
    dq_elapsed_ms = dq_draw_elapsed_ms = 0;
    dq_last_ms = now;
    dq_started = 1;
    dq_waiting = 0;
    dq_note_played = 0;
    memset(dq_active, 0, sizeof dq_active);
    memset(dq_note_off, 0, sizeof dq_note_off);
    memset(dq_note_off_pending, 0, sizeof dq_note_off_pending);
    dq_stopping = 0;
}

int staff_game_fm1_midi_received(const uint8_t *message, size_t bytes)
{
    if (!message || bytes != 3) return 0;
    uint8_t type = message[0] & 0xf0u;
    if ((type != 0x80u && type != 0x90u) || message[1] < 53u || message[1] > 79u) return 0;
    uint8_t next = (uint8_t)((midi_write + 1u) & 31u);
    if (next == midi_read) { midi_overflow = 1; return 1; }
    midi_events[midi_write].note = message[1];
    midi_events[midi_write].on = (uint8_t)(type == 0x90u && message[2] != 0);
    __asm__ volatile("csync" ::: "memory");
    midi_write = next;
    return 1;
}

static __attribute__((minsize)) void stop_note(void)
{
    if(note_active) {
        uint8_t off[3]={0x80,sounding_midi,0};
        if(groove_midi_local(off,3)==0) { fm1_control_request(); note_active=0; }
    }
}

__attribute__((minsize)) int staff_game_fm1_control(const struct fm1_controls *c)
{
    /* FM1 Open / emulator matrix IDs: HOME=8, ARP=10, SEQ=11,
     * PLAY=12, REC=13. The emulator's 9/0 keys press ARP/SEQ. */
    enum { BTN_HOME=8, BTN_PREV=10, BTN_NEXT=11, BTN_PLAY=12, BTN_REC=13 };
    if(!c) return 0;
    uint32_t now=fm1_millis(), key_edges=c->keys&~previous_keys;
    uint32_t button_edges=c->buttons&~previous_buttons;
    midi_pressed = 0;
    previous_keys=c->keys; previous_buttons=c->buttons;
    if (midi_overflow) {
        midi_held = 0;
        midi_read = midi_write;
        midi_overflow = 0;
    }
    dq_service_note_offs(now);
    dq_refresh_stopping();
    while (midi_read != midi_write) {
        uint8_t read = midi_read;
        uint8_t note = midi_events[read].note;
        uint32_t bit = 1u << (note - 53u);
        if (midi_events[read].on) { midi_held |= bit; midi_pressed |= bit; }
        else midi_held &= ~bit;
        __asm__ volatile("csync" ::: "memory");
        midi_read = (uint8_t)((read + 1u) & 31u);
    }
    if (game.screen != STAFF_PLAYING) midi_held = 0;
    if(!initialized) { staff_game_init(&game,now); initialized=1; draw_band=0; }
    if((c->buttons&(1u<<BTN_HOME))&&(button_edges&(1u<<BTN_REC))) {
        stop_note(); dq_stop_backing(); dq_started=0; staff_game_init(&game,now); draw_band=0; return 1;
    }
    if(note_active&&(int32_t)(now-note_off_at)>=0) stop_note();
    int turn=c->encoders[0]>0?1:c->encoders[0]<0?-1:
             (button_edges&(1u<<BTN_NEXT))?1:
             (button_edges&(1u<<BTN_PREV))?-1:0;
    uint8_t was_playing = game.screen == STAFF_PLAYING;
    staff_game_update(&game,now,key_edges|midi_held,(uint8_t)((button_edges>>BTN_PLAY)&1u),
                      (uint8_t)((button_edges>>BTN_HOME)&1u),(int8_t)turn,&platform);
    if (!was_playing && game.screen == STAFF_PLAYING && game.song == 3u) dq_start(&game,now);
    if (game.screen == STAFF_PLAYING && game.song == 3u && !dq_started && !dq_stopping)
        dq_start(&game,now);
    if (was_playing && game.screen != STAFF_PLAYING) {
        dq_stop_backing(); dq_started=0; dq_waiting=0; dq_note_played=0;
    }
    if (game.song == 3u && game.screen == STAFF_PLAYING && dq_started) {
        uint32_t delta = now - dq_last_ms;
        dq_last_ms = now;
        if (game.mode == STAFF_MODE_MISS || !dq_waiting) dq_elapsed_ms += delta > 100u ? 100u : delta;
        if (dq_player_index < DQ_PLAYER_COUNT) {
            uint32_t due = DQ_LEAD_MS + dq_tick_ms(dq_player[dq_player_index].tick);
            if (!dq_waiting && dq_elapsed_ms >= due) dq_waiting = 1;
            if (dq_waiting && game.mode == STAFF_MODE_MISS && !dq_note_played) {
                uint32_t mask = dq_player[dq_player_index].mask;
                unsigned sent = 1;
                for (unsigned k=0;k<27;++k) if (mask & (1u<<k)) {
                    if (!dq_note_off_pending[k]) {
                        if (dq_midi(0,(uint8_t)(53u+k),88) != 0) { sent = 0; break; }
                        dq_note_off_pending[k] = 1;
                        dq_note_off[k] = now + 180u;
                    }
                }
                if (sent) dq_note_played = 1;
            }
            if (dq_waiting) {
                uint32_t expected = dq_player[dq_player_index].mask;
                uint32_t active = key_edges | midi_held;
                uint32_t pressed = key_edges | midi_pressed;
                /* A held MIDI key may complete a chord, but it must not
                 * auto-answer a later repeated pitch without a fresh press. */
                if ((active & expected) == expected && (pressed & expected) == expected &&
                    !(pressed & ~expected)) {
                    if (game.mode == STAFF_MODE_PRACTICE)
                        for (unsigned k=0;k<27;++k) if (expected & (1u<<k)) {
                            uint8_t note = (uint8_t)(53u+k);
                            if (dq_midi(0,note,88) == 0) {
                                dq_note_off_pending[k] = 1;
                                dq_note_off[k] = now + 180u;
                            }
                        }
                    ++dq_player_index; game.error=0; dq_waiting=0; dq_note_played=0;
                } else if (game.mode == STAFF_MODE_MISS && (pressed & ~expected)) {
                    ++game.miss_count; game.error=1;
                    ++dq_player_index; dq_waiting=0; dq_note_played=0;
                } else if (game.mode == STAFF_MODE_MISS && dq_elapsed_ms >= due + 300u) {
                    ++game.miss_count; game.error=1;
                    ++dq_player_index; dq_waiting=0; dq_note_played=0;
                } else if (game.mode == STAFF_MODE_PRACTICE && !game.error &&
                           (pressed & ~expected)) {
                    game.error=1; tone(110,120); dq_stop_backing();
                }
            }
        }
        if (dq_player_index >= DQ_PLAYER_COUNT && game.mode == STAFF_MODE_PRACTICE) {
            game.screen=STAFF_DONE; dq_stop_backing(); dq_started=0;
        }
        if (game.mode == STAFF_MODE_MISS || !dq_waiting) {
            while (dq_accomp_index < DQ_ACCOMP_COUNT) {
                const DQAccompEvent *e=&dq_accomp[dq_accomp_index];
                if (DQ_LEAD_MS+dq_tick_ms(e->tick)>dq_elapsed_ms) break;
                if (dq_midi(e->lane,e->note,e->on?e->velocity:0) != 0) break;
                dq_active[e->lane][e->note]=(uint8_t)e->on;
                ++dq_accomp_index;
            }
        }
        if (dq_player_index >= DQ_PLAYER_COUNT && dq_accomp_index >= DQ_ACCOMP_COUNT) {
            game.screen=STAFF_DONE; dq_stop_backing(); dq_started=0;
        }
    }
    if(fm1_screen_pending()==0) {
        if(draw_band==0) {
            draw_snapshot=game;
            dq_draw_elapsed_ms=dq_elapsed_ms;
            dq_draw_player_index=dq_player_index;
            dq_draw_waiting=dq_waiting;
        }
        draw_top=(unsigned)draw_band*16u;
        staff_game_draw(&draw_snapshot,&platform);
        if (draw_snapshot.song == 3u)
            dq_draw(&draw_snapshot,dq_draw_elapsed_ms,dq_draw_player_index,dq_draw_waiting);
        int submitted=fm1_screen_submit(0,draw_top,240,16,band_pixels,FM1_SCREEN_DMA_PIXELS);
        if(submitted==0) draw_band=(uint8_t)((draw_band+1u)%15u);
        else if(submitted<0) draw_band=0;
    }
    return 1;
}
