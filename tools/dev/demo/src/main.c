#include "platform/init.h"
#include "platform/events.h"
#include "platform/keyboard.h"
#include "platform/sdl_render.h"

static const uint32_t SCREEN_W = 1920;
static const uint32_t SCREEN_H = 1080;
static const uint8_t SCREEN_BPP = 32;

int main(void)
{
	demo_ctx_t ctx = demo_init(SCREEN_W, SCREEN_H, SCREEN_BPP);

	while (events_process())
	{
		keyboard_update();
		sdl_render(ctx.fb, ctx.renderer, ctx.texture);
	}

	demo_shutdown(&ctx);
	return 0;
}
