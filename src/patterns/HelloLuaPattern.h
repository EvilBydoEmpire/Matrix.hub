#pragma once

// The first Lua vertical-slice pattern is compiled into firmware. LittleFS and
// catalog loading come later, after the runtime has proven stable on hardware.
static const char HELLO_LUA_PATTERN[] = R"LUA(
return {
  init = function(ctx)
  end,

  frame = function(ctx)
    local shortSide = math.min(ctx.width, ctx.height)
    local scale = math.max(1, math.floor(shortSide / 16))
    local letterWidth = 4 * scale
    local letterHeight = 7 * scale
    local gap = 2 * scale
    local totalWidth = letterWidth * 3 + gap * 2
    local x = math.floor((ctx.width - totalWidth) / 2)
    local y = math.floor((ctx.height - letterHeight) / 2)
    local stroke = math.max(1, scale)

    local background = color.rgb(2, 3, 10)
    local foreground = color.hsv((ctx.time * 0.10) % 1.0, 0.72, 0.90)
    local accent = color.hsv((ctx.time * 0.10 + 0.35) % 1.0, 0.65, 0.75)
    gfx.clear(background)

    -- L
    gfx.fillRect(x, y, stroke, letterHeight, foreground)
    gfx.fillRect(x, y + letterHeight - stroke, letterWidth, stroke, foreground)
    x = x + letterWidth + gap

    -- U
    gfx.fillRect(x, y, stroke, letterHeight - stroke, foreground)
    gfx.fillRect(x + letterWidth - stroke, y, stroke, letterHeight - stroke, foreground)
    gfx.fillRect(x, y + letterHeight - stroke, letterWidth, stroke, foreground)
    x = x + letterWidth + gap

    -- A
    gfx.fillRect(x, y + stroke, stroke, letterHeight - stroke, foreground)
    gfx.fillRect(x + letterWidth - stroke, y + stroke, stroke, letterHeight - stroke, foreground)
    gfx.fillRect(x, y, letterWidth, stroke, foreground)
    gfx.fillRect(x, y + math.floor(letterHeight / 2), letterWidth, stroke, foreground)

    local orbit = shortSide * 0.38
    local cx = math.floor(ctx.width / 2 + math.cos(ctx.time * 1.7) * orbit)
    local cy = math.floor(ctx.height / 2 + math.sin(ctx.time * 1.3) * orbit)
    gfx.circle(cx, cy, math.max(1, scale), accent)
  end
}
)LUA";

