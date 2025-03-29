/**
 * @file scroll_text.cpp
 * @brief Implementation of scroll text utility
 * @version 0.1
 * @date 2024-06-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "scroll_text.h"
#include "../common_define.h"
#include "../theme/theme_define.h"
#include <string.h>

namespace UTILS
{
    namespace SCROLL_TEXT
    {
        bool scroll_text_init(
            ScrollTextContext_t* ctx, lgfx::LovyanGFX* canvas, int width, int height, uint32_t speed_ms, uint32_t pause_ms)
        {
            if (!ctx || !canvas || width <= 0 || height <= 0)
                return false;

            // Free any existing sprite
            if (ctx->sprite)
            {
                ctx->sprite->deleteSprite();
                delete ctx->sprite;
            }

            // Initialize context
            ctx->sprite = new LGFX_Sprite(canvas);
            if (!ctx->sprite)
                return false;

            ctx->canvas = canvas;
            ctx->sprite->createSprite(width, height);
            ctx->sprite->setFont(FONT_16);
            ctx->sprite->setTextSize(1);

            ctx->width = width;
            ctx->height = height;
            ctx->scroll_time_count = millis();
            ctx->scroll_period = speed_ms;
            ctx->pause_duration = pause_ms;
            ctx->scroll_pos = 0;
            ctx->scroll_direction = true; // Start scrolling from right to left

            return true;
        }

        bool scroll_text_render(ScrollTextContext_t* ctx, const char* text, int x, int y, int fg_color, uint32_t bg_color)
        {
            if (!ctx || !ctx->sprite || !text)
                return false;

            // Calculate text width (approximate - 8 pixels per character)
            const int text_width = ctx->canvas->textWidth(text);

            // If text fits in the area and we're not forcing scroll, just render it statically
            if (text_width <= ctx->width)
            {
                if (ctx->scroll_direction)
                {
                    ctx->sprite->fillScreen(bg_color);
                    // Static display - text fits in area
                    ctx->sprite->setTextColor(fg_color, bg_color);
                    ctx->sprite->drawString(text, 0, 0);
                    // Push to canvas
                    ctx->sprite->pushSprite(ctx->canvas, x, y);
                    ctx->scroll_direction = false; // using direction flasg as update flag
                    return true;                   // update required
                }
                else
                {
                    return false; // no update required
                }
            }

            // Scrolling is needed
            uint32_t now = millis();
            bool updated = false;

            if (now > (ctx->scroll_time_count + ctx->scroll_period))
            {
                ctx->scroll_time_count = now;
                updated = true;

                // Calculate positions
                if (ctx->scroll_direction)
                {
                    // Scrolling right to left
                    ctx->scroll_pos--;
                    if (ctx->scroll_pos <= ctx->width - text_width)
                    {
                        ctx->scroll_pos = ctx->width - text_width;
                        ctx->scroll_time_count = now + ctx->pause_duration;
                        ctx->scroll_direction = false;
                    }
                }
                else
                {
                    // Scrolling left to right
                    ctx->scroll_pos++;
                    if (ctx->scroll_pos >= 0)
                    {
                        ctx->scroll_pos = 0;
                        ctx->scroll_time_count = now + ctx->pause_duration;
                        ctx->scroll_direction = true;
                    }
                }

                // Update the sprite contents
                ctx->sprite->fillScreen(bg_color);
                ctx->sprite->setTextColor(fg_color, bg_color);
                ctx->sprite->drawString(text, ctx->scroll_pos, 0);

                // Push to canvas
                ctx->sprite->pushSprite(ctx->canvas, x, y);
            }

            return updated;
        }
        void scroll_text_reset(ScrollTextContext_t* ctx)
        {
            if (!ctx)
                return;

            ctx->scroll_pos = 0;
            ctx->scroll_direction = true;
            ctx->scroll_time_count = millis();
        }

        void scroll_text_free(ScrollTextContext_t* ctx)
        {
            if (!ctx)
                return;

            if (ctx->sprite)
            {
                ctx->sprite->deleteSprite();
                delete ctx->sprite;
                ctx->sprite = nullptr;
            }
        }
    } // namespace SCROLL_TEXT
} // namespace UTILS
