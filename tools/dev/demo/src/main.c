#include "platform/init.h"
#include "platform/events.h"
#include "platform/sdl_render.h"

int fb_width = 0;
int fb_height = 0;
uint32_t *framebuffer_back = NULL;

static const uint32_t SCREEN_W = 1920;
static const uint32_t SCREEN_H = 1080;
static const uint8_t SCREEN_BPP = 32;

int main(void)
{
	demo_ctx_t ctx = demo_init(SCREEN_W, SCREEN_H, SCREEN_BPP);

	while (events_process())
		sdl_render(ctx.fb, ctx.renderer, ctx.texture);

	demo_shutdown(&ctx);
	return 0;
}
