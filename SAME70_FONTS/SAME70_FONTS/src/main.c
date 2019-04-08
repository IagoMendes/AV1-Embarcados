/*
 * main.c
 *
 * Created: 05/03/2019 18:00:58
 *  Author: eduardo
 */ 

#include <asf.h>
#include "tfont.h"
#include "sourcecodepro_28.h"
#include "calibri_36.h"
#include "arial_72.h"
#include "stdio.h"

/* Defines */
#define YEAR        2018
#define MOUNTH      3
#define DAY         19
#define WEEK        12
#define HOUR        15
#define MINUTE      45
#define SECOND      0

#define raio 0.325
volatile char string_vel[32];
volatile char string_dist[32];
volatile char string_time[32];
volatile int counter_seg;
volatile int counter_min;
volatile Bool f_rtt_alarme = false;
volatile int voltas = 0;
volatile float distancia = 0;

// Configuracoes do botao
// Botão1
#define BUT1_PIO      PIOD
#define BUT1_PIO_ID   ID_PIOD
#define BUT1_IDX  28
#define BUT1_IDX_MASK (1 << BUT1_IDX)

// Botão2
#define BUT2_PIO      PIOC
#define BUT2_PIO_ID   ID_PIOC
#define BUT2_IDX  31
#define BUT2_IDX_MASK (1 << BUT2_IDX)

struct ili9488_opt_t g_ili9488_display_opt;

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses);

float calcula_velocidade(int n){
	float a = 0.325 * 2.0 * 3.14 * (float) n;
	distancia += a;
	return a * 3.6 / 4 ;
}

/* CALLBACKS */
void but1_callback(void)
{
	
}

void but2_callback(void)
{
	voltas += 1;
}

void RTC_Handler(void){
	counter_seg += 1;
	if (counter_seg == 60){
		counter_seg = 00;
		counter_min += 1;
		
	}
	sprintf(string_time, "%d:%d", counter_min, counter_seg);
	
	rtc_set_time(RTC, HOUR, MINUTE, SECOND);
	rtc_set_date_alarm(RTC, 1, MOUNTH, 1, DAY);
	rtc_set_time_alarm(RTC, 1, HOUR, 1, MINUTE, 1, SECOND+1);
	
	rtc_clear_status(RTC, RTC_SCCR_ALRCLR);	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);	
}

void RTT_Handler(void)
{
	  uint32_t ul_status;

	  /* Get RTT status */
	  ul_status = rtt_get_status(RTT);

	  /* IRQ due to Time has changed */
	  if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	  /* IRQ due to Alarm */
	  if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		  f_rtt_alarme = true;                  
		  sprintf(string_vel, "%f", calcula_velocidade(voltas));
		  sprintf(string_dist, "%f", distancia);
	 }
  voltas = 0;
}

/* INIT */

void RTC_init(){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(RTC, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(RTC, YEAR, MOUNTH, DAY, WEEK);
	rtc_set_time(RTC, HOUR, MINUTE, SECOND);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(RTC_IRQn);
	NVIC_ClearPendingIRQ(RTC_IRQn);
	NVIC_SetPriority(RTC_IRQn, 0);
	NVIC_EnableIRQ(RTC_IRQn);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(RTC,  RTC_IER_ALREN);
}

static float get_time_rtt(){
  uint ul_previous_time = rtt_read_timer_value(RTT); 
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
  uint32_t ul_previous_time;

  /* Configure RTT for a 1 second tick interrupt */
  rtt_sel_source(RTT, false);
  rtt_init(RTT, pllPreScale);
  
  ul_previous_time = rtt_read_timer_value(RTT);
  while (ul_previous_time == rtt_read_timer_value(RTT));
  
  rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

  /* Enable RTT interrupt */
  NVIC_DisableIRQ(RTT_IRQn);
  NVIC_ClearPendingIRQ(RTT_IRQn);
  NVIC_SetPriority(RTT_IRQn, 0);
  NVIC_EnableIRQ(RTT_IRQn);
  rtt_enable_interrupt(RTT, RTT_MR_ALMIEN);
}


void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);
	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
}

void init(void){
	board_init();
	sysclk_init();
	RTC_init();
	
	// Desativa watchdog
	WDT->WDT_MR = WDT_MR_WDDIS;
	
	// Inicializa PIO do botao
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pmc_enable_periph_clk(BUT2_PIO_ID);
	
	// Configura PIO para lidar com o pino do botão como entrada
	// com pull-up
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_IDX_MASK, PIO_PULLUP);

	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BUT1_PIO,BUT1_PIO_ID,BUT1_IDX_MASK,PIO_IT_RISE_EDGE,but1_callback);
	pio_handler_set(BUT2_PIO,BUT2_PIO_ID,BUT2_IDX_MASK,PIO_IT_RISE_EDGE,but2_callback);
	
	// Ativa interrupção
	pio_enable_interrupt(BUT1_PIO, BUT1_IDX_MASK);
	pio_enable_interrupt(BUT2_PIO, BUT2_IDX_MASK);
	
	pio_set_debounce_filter(BUT2_PIO, BUT2_IDX_MASK, 20);
	
	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4
	
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT2_PIO_ID, 4); // Prioridade 4
}

void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while(*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if(letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}	
}

int main(void) {
	init();
	configure_lcd();
	
	rtc_set_date_alarm(RTC, 1, MOUNTH, 1, DAY);
	rtc_set_time_alarm(RTC, 1, HOUR, 1, MINUTE, 1, SECOND+1);
	
	uint16_t pllPreScale = (int) (((float) 32768) / 2.0);
	uint32_t irqRTTvalue = 8;
	f_rtt_alarme = true;
	
	sprintf(string_vel, "%d", 0);
	sprintf(string_dist, "%d", 0);
	sprintf(string_time, "%d:%d", 0, 0);
	
	voltas = 0;
	
	while(1) {
		if (f_rtt_alarme){
			RTT_init(pllPreScale, irqRTTvalue);         
			f_rtt_alarme = false;
			ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
		}
		font_draw_text(&calibri_36, "Velocidade (km/h)", 10, 50, 1);
		font_draw_text(&calibri_36, "Distancia (m)", 10, 200, 1);
		font_draw_text(&calibri_36, "Tempo", 10, 350, 1);
		font_draw_text(&calibri_36, string_vel, 10, 100, 1);
		font_draw_text(&calibri_36, string_dist, 10, 250, 1);
		font_draw_text(&calibri_36, string_time, 10, 400, 1);
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}