# F256 Switcharoo - Troubleshooting Guide

## Issue: No Mouse Pointer Visible

### Symptoms
- Program runs but no mouse cursor appears on screen
- Moving mouse has no visible effect

### Root Cause
PS/2 mouse hardware not initialized.

### Solution
Ensure `video_init()` includes:
```c
POKE(PS2_M_MODE_EN, 0x01);  // Enable mouse hardware
POKEW(PS2_M_X_LO, 160);     // Set initial X position
POKEW(PS2_M_Y_LO, 120);     // Set initial Y position
```

### Verification
- Mouse pointer should appear at center of screen on startup
- Moving mouse should move cursor smoothly

---

## Issue: Keyboard Input Not Working

### Symptoms
- Pressing arrow keys has no effect
- Keyboard shortcuts don't respond

### Root Cause
Not polling kernel events or wrong scan codes.

### Solution
1. Ensure `input_poll()` calls `kernelNextEvent()` every frame
2. Verify scan code mapping matches PS/2 Set 2:
   - Up: 0x75, Down: 0x72, Left: 0x6B, Right: 0x74
   - Enter: 0x5A, Escape: 0x76

### Verification
- Arrow keys should generate INPUT_EVENT_KEY_DOWN events
- Scan codes in `kernelEventData.key.raw` should match expected values

---

## Issue: Screen "Snow" or Tearing

### Symptoms
- Visual artifacts when pressing keys
- Horizontal lines or flickering during input
- Sprites appear to "tear" or leave trails

### Root Cause
Updating video memory during active display (not waiting for VSYNC).

### Solution
Add `graphicsWaitVerticalBlank()` before any sprite updates:

```c
void render_update(const game_state_t *state) {
    graphicsWaitVerticalBlank();  // Wait for safe update window
    render_update_pieces(&state->board);
    // ... other updates
}
```

### Verification
- All screen updates should be smooth
- No flickering or tearing artifacts
- Rendering locked to 60 FPS

---

## Issue: Screen Flicker

### Symptoms
- Entire screen flickers periodically
- Display mode seems to reset momentarily

### Root Cause
Code modifying `MMU_IO_CTRL` register during main loop.

### Solution
Remove any code that changes `MMU_IO_CTRL` after initialization:

```c
// BAD - causes flicker:
void text_test() {
    POKE(MMU_IO_CTRL, 2);  // Changes memory banking
    // ... write to text RAM
    POKE(MMU_IO_CTRL, 0);  // Restore
}

// GOOD - do text operations during init only
```

### Verification
- Screen should remain stable during all input operations
- No mode switching or banking changes during game loop

---

## Issue: Mouse Position Drift

### Symptoms
- Mouse cursor slowly drifts off screen
- Cursor position doesn't match physical mouse position

### Root Cause
Not reading/writing hardware mouse registers correctly.

### Solution
Always read current position from hardware, apply delta, then write back:

```c
// Read hardware position
int16_t hw_x = PEEKW(PS2_M_X_LO);
int16_t hw_y = PEEKW(PS2_M_Y_LO);

// Apply movement delta
int16_t new_x = hw_x + delta_x;
int16_t new_y = hw_y + delta_y;

// Clamp to bounds
if (new_x < 0) new_x = 0;
if (new_x >= 320) new_x = 319;
if (new_y < 0) new_y = 0;
if (new_y >= 240) new_y = 239;

// Write back to hardware
POKEW(PS2_M_X_LO, new_x);
POKEW(PS2_M_Y_LO, new_y);
```

### Verification
- Cursor should stay within screen bounds (0-319, 0-239)
- Cursor movement should match physical mouse movement accurately

---

## Issue: Slow Mouse Response

### Symptoms
- Mouse feels sluggish
- Cursor moves too slowly

### Root Cause
Missing movement acceleration boost.

### Solution
Apply 2x boost for fast movements (>4 pixels/frame):

```c
int8_t boost_x = 1;
int8_t delta_x = (int8_t)kernelEventData.mouse.delta.x;
if (delta_x > 4 || delta_x < -4) boost_x = 2;

new_x = hw_x + boost_x * delta_x;
```

### Verification
- Slow mouse movements: 1:1 pixel tracking
- Fast mouse movements: 2:1 acceleration for easier navigation

---

## Diagnostic Commands

### Check PS/2 Mouse Status
```c
uint8_t mode = PEEK(PS2_M_MODE_EN);
printf("Mouse enabled: %s\\n", (mode & 0x01) ? "YES" : "NO");
printf("Mouse mode: %d\\n", (mode & 0x02) >> 1);
```

### Monitor Kernel Events
```c
kernelNextEvent();
if (kernelEventData.type) {
    printf("Event type: 0x%02X\\n", kernelEventData.type);
    if (kernelEventData.type == kernelEvent(key.PRESSED)) {
        printf("Key scan: 0x%02X ASCII: '%c'\\n", 
               kernelEventData.key.raw, 
               kernelEventData.key.ascii);
    }
}
```

### Check VSYNC Timing
```c
uint32_t frame_count = 0;
uint32_t start_time = /* get time */;

while (frame_count < 60) {
    graphicsWaitVerticalBlank();
    frame_count++;
}

uint32_t elapsed = /* get time */ - start_time;
printf("60 frames took: %ld ms (should be ~1000ms)\\n", elapsed);
```

---

## Hardware Reference

### PS/2 Mouse Registers
| Address | Name | Description |
|---------|------|-------------|
| 0xD6E0 | PS2_M_MODE_EN | bit0=enable, bit1=mode |
| 0xD6E2-0xD6E3 | PS2_M_X_LO/HI | 16-bit X position |
| 0xD6E4-0xD6E5 | PS2_M_Y_LO/HI | 16-bit Y position |

### Video Timing (320x240@60Hz)
- Active display: 240 lines
- Vblank period: ~16.67ms per frame
- Safe update window: During vblank only

### Kernel Event Types
| Event | Value | Description |
|-------|-------|-------------|
| key.PRESSED | varies | Key down with scan code |
| key.RELEASED | varies | Key up with scan code |
| mouse.DELTA | varies | Movement + button state |
| mouse.CLICKS | varies | Click events |

---

## Getting Help

If issues persist after following this guide:

1. Verify you're using the correct F256 kernel version
2. Test with the F256KsimpleCdoodles/mousing example to verify hardware
3. Check that PS/2 keyboard and mouse are properly connected
4. Try on real hardware vs emulator to isolate issues
5. Review f256lib.h for any API changes in your version
