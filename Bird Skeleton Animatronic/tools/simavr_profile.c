#define _POSIX_C_SOURCE 200809L
#include <simavr/sim_avr.h>
#include <simavr/sim_hex.h>
#include <simavr/sim_elf.h>
#include <simavr/sim_io.h>
#include <simavr/sim_irq.h>
#include <simavr/avr_uart.h>
#include <simavr/avr_ioport.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define FREQ 16000000ULL
#define CYCLES_PER_US 16ULL
#define DMX_SLOT_CYCLES 704ULL
#define DMX_BREAK_MAB_CYCLES 1728ULL
#define DMX_FRAME_CYCLES (DMX_BREAK_MAB_CYCLES + 513ULL * DMX_SLOT_CYCLES)

struct symbols {
    uint32_t loop, packet, timer0_start, timer0_end;
    uint32_t dmx_start, dmx_end, servo_start, servo_end;
    uint32_t tx_start, tx_end, led_start, led_end;
    uint32_t lowflash, highflash;
    uint32_t packet_end;
    uint32_t address, address_valid, frame_values, frame_generation;
    uint32_t channel_number, receive_state, millis_value;
};
static const avr_symbol_t *find_symbol(const elf_firmware_t *fw, const char *name) {
    for (uint32_t i=0; i<fw->symbolcount; ++i)
        if (!strcmp(fw->symbol[i]->symbol, name)) return fw->symbol[i];
    return NULL;
}
static const avr_symbol_t *find_prefix(const elf_firmware_t *fw, const char *prefix) {
    for (uint32_t i=0; i<fw->symbolcount; ++i)
        if (!strncmp(fw->symbol[i]->symbol, prefix, strlen(prefix))) return fw->symbol[i];
    return NULL;
}
static void load_symbols(const char *path, struct symbols *s) {
    elf_firmware_t fw={0};
    if (elf_read_firmware(path, &fw) < 0) { fprintf(stderr,"cannot read ELF %s\n",path); exit(2); }
    const avr_symbol_t *sym;
#define GET(name, field) do { sym=find_symbol(&fw, name); if (sym) s->field=sym->addr; } while (0)
    GET("main",loop); GET("__vector_9",timer0_start); GET("__vector_6",timer0_end);
    GET("__vector_11",dmx_start); GET("__vector_3",servo_start);
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_17addressE"); if(sym)s->address=sym->addr;
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_114addressIsValidE"); if(sym)s->address_valid=sym->addr;
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_16valuesE"); if(sym)s->frame_values=sym->addr;
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_115frameGenerationE"); if(sym)s->frame_generation=sym->addr;
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_113channelNumberE"); if(sym)s->channel_number=sym->addr;
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_112receiveStateE"); if(sym)s->receive_state=sym->addr;
    sym=find_symbol(&fw,"timer0_millis"); if(sym)s->millis_value=sym->addr;
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_110sendPacket"); if(sym){s->packet=sym->addr;s->packet_end=sym->addr+sym->size;}
    sym=find_prefix(&fw,"_ZN12_GLOBAL__N_114writeIfChanged"); if(sym){s->led_start=sym->addr;s->led_end=sym->addr+sym->size;}
    s->lowflash=UINT32_MAX;
    for(uint32_t i=0;i<fw.symbolcount;i++){
        uint32_t a=fw.symbol[i]->addr;
        if(a<AVR_SEGMENT_OFFSET_DATA && a<s->lowflash)s->lowflash=a;
        if(a<AVR_SEGMENT_OFFSET_DATA && a>s->highflash)s->highflash=a;
    }
    if(!s->loop||!s->dmx_start||!s->address||!s->address_valid||!s->frame_values||!s->frame_generation||!s->receive_state||!s->millis_value||!s->packet_end||!s->led_end){
        fprintf(stderr,"ELF is missing required profiler symbols\n"); exit(2);
    }
#undef GET
}

struct profile {
    struct symbols *sym;
    uint64_t cycles, loop_iterations;
    uint64_t timer0, servo, dmxrx, txserial, ledwire, sendpacket, other;
    uint64_t irqoff_fg, irqoff_tx, irqoff_led, max_irqoff_fg, current_irqoff_fg;
    uint64_t servo_edges, led_edges, audio_edges;
    uint64_t dmx_frames_sent, dmx_packets_completed;
    uint64_t uart_wait_cycles, uart_wait_events, uart_max_wait;
    uint64_t uart_bytes_presented, uart_bytes_blocked;
};
struct edge_ctx { struct profile *p; int kind; uint32_t last; };
struct uart_flow { int xon; };

static void uart_xon_notify(struct avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq; (void)value; ((struct uart_flow *)param)->xon = 1;
}
static void uart_xoff_notify(struct avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq; (void)value; ((struct uart_flow *)param)->xon = 0;
}

static void edge_notify(struct avr_irq_t *irq, uint32_t value, void *param) {
    (void)irq;
    struct edge_ctx *c = param;
    uint32_t bit = value & 1u;
    if (bit != c->last) {
        if (c->kind == 0) c->p->servo_edges++;
        else if (c->kind == 1) c->p->led_edges++;
        else c->p->audio_edges++;
        c->last = bit;
    }
}

static void set_dips(avr_t *avr, unsigned address) {
    avr_ioport_external_t pb = {.name='B', .mask=0x3e, .value=(uint8_t)((~address) & 0x3e)};
    avr_ioport_external_t pc = {.name='C', .mask=0x07, .value=(uint8_t)((~(address >> 6)) & 0x07)};
    if (avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL('B'), &pb) != 0 ||
        avr_ioctl(avr, AVR_IOCTL_IOPORT_SET_EXTERNAL('C'), &pc) != 0) {
        fprintf(stderr, "failed to set DIP external state\n"); exit(2);
    }
}

static void load_hex(avr_t *avr, const char *path) {
    uint32_t size=0, start=0;
    uint8_t *code = read_ihex_file(path, &size, &start);
    if (!code) { fprintf(stderr, "cannot read %s\n", path); exit(2); }
    avr_loadcode(avr, code, size, start);
    free(code);
}

static int in(uint32_t pc, uint32_t a, uint32_t b) { return pc >= a && pc < b; }

static void account(struct profile *p, uint32_t pc, uint64_t delta, int sreg_i) {
    p->cycles += delta;
    struct symbols *s=p->sym;
    int timer0=in(pc,s->timer0_start,s->timer0_end), dmx=in(pc,s->dmx_start,s->dmx_end);
    int servo=in(pc,s->servo_start,s->servo_start+0x200);
    int is_isr=timer0||dmx||servo;
    if (timer0) p->timer0 += delta;
    else if (dmx) p->dmxrx += delta;
    else if (servo) p->servo += delta;
    else if (in(pc,s->tx_start,s->tx_end)) p->txserial += delta;
    else if (in(pc,s->led_start,s->led_end)) p->ledwire += delta;
    else if (in(pc,s->packet,s->packet_end)) p->sendpacket += delta;
    else p->other += delta;

    if (!sreg_i && !is_isr) {
        p->irqoff_fg += delta;
        p->current_irqoff_fg += delta;
        if (in(pc,s->tx_start,s->tx_end)) p->irqoff_tx += delta;
        if (in(pc,s->led_start,s->led_end)) p->irqoff_led += delta;
        if (p->current_irqoff_fg > p->max_irqoff_fg) p->max_irqoff_fg = p->current_irqoff_fg;
    } else p->current_irqoff_fg = 0;
}

static void run_until(avr_t *avr, uint64_t target, struct profile *p, int measure) {
    while (avr->cycle < target && avr->state != cpu_Crashed && avr->state != cpu_Done) {
        uint32_t pc = avr->pc;
        int i = avr->sreg[7];
        uint64_t before = avr->cycle;
        avr_run(avr);
        uint64_t d = avr->cycle - before;
        if (measure) {
            if (pc == p->sym->loop) p->loop_iterations++;
            
            account(p, pc, d, i);
        }
    }
}

static void send_byte_at(avr_t *avr, avr_irq_t *uart_in, struct uart_flow *flow,
                         uint64_t when, uint16_t value, struct profile *p,
                         int measure, int cooperative) {
    run_until(avr, when, p, measure);
    p->uart_bytes_presented++;
    if (cooperative) {
        uint64_t wait_start = avr->cycle;
        while (!flow->xon && avr->state != cpu_Crashed && avr->state != cpu_Done)
            run_until(avr, avr->cycle + 1, p, measure);
        uint64_t waited = avr->cycle - wait_start;
        if (waited) {
            p->uart_wait_cycles += waited; p->uart_wait_events++;
            if (waited > p->uart_max_wait) p->uart_max_wait = waited;
        }
    } else if (!flow->xon) {
        p->uart_bytes_blocked++;
    }
    avr_raise_irq(uart_in, value);
}

static void send_frame(avr_t *avr, avr_irq_t *uart_in, struct uart_flow *flow,
                       uint64_t start, const uint8_t ch[13], struct profile *p,
                       int measure, int cooperative) {
    send_byte_at(avr, uart_in, flow, start, UART_INPUT_FE, p, measure, cooperative);
    uint64_t t = start + DMX_BREAK_MAB_CYCLES;
    send_byte_at(avr, uart_in, flow, t, 0, p, measure, cooperative); // start code
    for (unsigned slot=1; slot<=512; slot++) {
        uint8_t v = 0;
        unsigned addr = avr->data[p->sym->address] | ((unsigned)avr->data[p->sym->address + 1] << 8);
        if (slot >= addr && slot < addr + 13) v = ch[slot-addr];
        t += DMX_SLOT_CYCLES;
        send_byte_at(avr, uart_in, flow, t, v, p, measure, cooperative);
    }
    p->dmx_frames_sent++;
}

static int run_case(const char *hex, const struct symbols *sym, unsigned address, const char *name,
                    const uint8_t ch[13], int audio_transition, int cooperative) {
    avr_t *avr = avr_make_mcu_by_name("atmega8");
    if (!avr) return 2;
    avr->frequency = FREQ;
    if (avr_init(avr) != 0) return 2;
    set_dips(avr, address);
    load_hex(avr, hex);
    avr_irq_t *uart_in = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
    if (!uart_in) uart_in = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ(' '), UART_IRQ_INPUT);
    if (!uart_in) { fprintf(stderr,"no uart input irq\n"); return 2; }
    avr_irq_t *uart_xon = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUT_XON);
    avr_irq_t *uart_xoff = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUT_XOFF);
    struct uart_flow flow = {.xon = 1};
    if (!uart_xon || !uart_xoff) { fprintf(stderr,"no uart flow IRQs\n"); return 2; }
    avr_irq_register_notify(uart_xon, uart_xon_notify, &flow);
    avr_irq_register_notify(uart_xoff, uart_xoff_notify, &flow);
    uint32_t uart_flags = 0;
    avr_ioctl(avr, AVR_IOCTL_UART_SET_FLAGS('0'), &uart_flags);

    struct profile p={0}; p.sym=(struct symbols *)sym;
    struct edge_ctx ec[3]={{&p,0,0},{&p,1,0},{&p,2,0}};
    avr_irq_t *pd2=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),IOPORT_IRQ_PIN2);
    avr_irq_t *pd5=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),IOPORT_IRQ_PIN5);
    avr_irq_t *pd6=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ('D'),IOPORT_IRQ_PIN6);
    avr_irq_register_notify(pd2,edge_notify,&ec[0]);
    avr_irq_register_notify(pd5,edge_notify,&ec[1]);
    avr_irq_register_notify(pd6,edge_notify,&ec[2]);

    run_until(avr, FREQ/20, &p, 0); // 50 ms setup stabilization
    unsigned decoded=avr->data[sym->address] | ((unsigned)avr->data[sym->address+1]<<8);
    unsigned valid=avr->data[sym->address_valid];
    // DMXSerial ignores the first received item in STARTUP, prime it.
    send_byte_at(avr, uart_in, &flow, avr->cycle, UART_INPUT_FE, &p, 0, cooperative);
    run_until(avr, avr->cycle + 2000, &p, 0);

    uint8_t warm_ch[13]; memcpy(warm_ch, ch, sizeof warm_ch);
    if (audio_transition) { warm_ch[11] = 0; warm_ch[12] = 0; }
    uint64_t warm_end=avr->cycle + FREQ/2; // 0.5 s warmup with DMX
    uint64_t next=avr->cycle;
    while (avr->cycle < warm_end) {
        send_frame(avr,uart_in,&flow,next,warm_ch,&p,0,cooperative);
        next += DMX_FRAME_CYCLES;
    }
    run_until(avr,warm_end,&p,0);

    memset(&p,0,sizeof p); ec[0].p=ec[1].p=ec[2].p=&p;
    uint64_t start=avr->cycle, end=start+FREQ/2; next=start;
    while (next < end) {
        send_frame(avr,uart_in,&flow,next,ch,&p,1,cooperative);
        next += DMX_FRAME_CYCLES;
    }
    run_until(avr,end,&p,1);
    unsigned values_ok=1;
    for(unsigned i=0;i<13;i++) if(avr->data[sym->frame_values+i]!=ch[i]) values_ok=0;
    unsigned xfr=avr->data[sym->frame_generation];
    unsigned recvstate=avr->data[sym->receive_state];
    unsigned long ms=(unsigned long)(avr->data[sym->millis_value]|((uint32_t)avr->data[sym->millis_value+1]<<8)|((uint32_t)avr->data[sym->millis_value+2]<<16)|((uint32_t)avr->data[sym->millis_value+3]<<24));
    printf("%-12s addr=%3u decoded=%3u valid=%u state=%u gen=%u values=%s ms=%lu cycles=%llu loops=%llu ",name,address,decoded,valid,recvstate,xfr,values_ok?"ok":"BAD",ms,(unsigned long long)p.cycles,(unsigned long long)p.loop_iterations);
    printf("isr[t0=%.3f%% servo=%.3f%% dmx=%.3f%%] block[led=%.3f%% tx=%.3f%% max=%.1fus] edges[s=%llu l=%llu a=%llu]\n",
        100.0*p.timer0/p.cycles,100.0*p.servo/p.cycles,100.0*p.dmxrx/p.cycles,
        100.0*p.irqoff_led/p.cycles,100.0*p.irqoff_tx/p.cycles,p.max_irqoff_fg/16.0,
        (unsigned long long)p.servo_edges,(unsigned long long)p.led_edges,(unsigned long long)p.audio_edges);
    printf("  rate loops=%.0f/s packets=%llu/%llu uart[ presented=%llu blocked=%llu waits=%llu total=%.1fus max=%.1fus ]\n",
        p.loop_iterations * (double)FREQ / p.cycles,
        (unsigned long long)p.dmx_packets_completed, (unsigned long long)p.dmx_frames_sent,
        (unsigned long long)p.uart_bytes_presented, (unsigned long long)p.uart_bytes_blocked,
        (unsigned long long)p.uart_wait_events, p.uart_wait_cycles/16.0, p.uart_max_wait/16.0);
    if (avr->state==cpu_Crashed) printf("  CRASHED\n");
    avr_terminate(avr); free(avr);
    return decoded==address && valid==(address<=499) && (address>499 || values_ok) ? 0 : 1;
}

int main(int argc,char **argv){
 setvbuf(stdout, NULL, _IOLBF, 0);
  if(argc < 2 || argc > 3){fprintf(stderr,"usage: %s firmware.hex [--fixed]\n",argv[0]);return 2;}
  int cooperative = argc == 2 || strcmp(argv[2], "--cooperative") == 0;
  if (!cooperative && strcmp(argv[2], "--fixed") != 0) {
      fprintf(stderr, "unknown mode: %s (use --cooperative or --fixed)\n", argv[2]);
      return 2;
  }
 struct symbols sym={0}; load_symbols(argv[1],&sym);
  struct C{const char*n;uint8_t mode,effect,speed,rate,sound,vol;} cases[]={
  {"manual",0,0,255,255,0,0},{"idle",16,16,128,128,0,0},{"synchronized",48,48,255,255,0,0},
  {"alternating",80,80,255,255,0,0},{"chatter",112,112,255,255,0,0},{"startled",144,144,255,255,0,0},
  {"haunted",176,176,255,255,0,0},{"lightning",208,208,255,255,246,30},{"bird-call",224,224,255,255,249,30},
  {"demo",240,240,255,255,250,30}};
 unsigned addrs[]={1,173,255,499,501,511}; int rc=0;
 for(unsigned ai=0;ai<sizeof(addrs)/sizeof(addrs[0]);ai++) for(unsigned ci=0;ci<sizeof(cases)/sizeof(cases[0]);ci++){
  uint8_t ch[13]={cases[ci].mode,cases[ci].speed,255,255,255,255,255,128,64,cases[ci].effect,cases[ci].rate,cases[ci].sound,cases[ci].vol};
  // Only representative mode at all addresses; full mode matrix at address 1.
  if(ai>0 && ci>0) continue;
   rc |= run_case(argv[1],&sym,addrs[ai],cases[ci].n,ch,0,cooperative);
 }
 uint8_t audio_ch[13]={176,255,255,255,255,255,255,128,64,176,255,1,255};
 rc |= run_case(argv[1],&sym,1,"audio+led",audio_ch,1,cooperative);
 return rc;
}
