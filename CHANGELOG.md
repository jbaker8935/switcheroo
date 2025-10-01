# F256 Switcharoo - Changelog

## [Unreleased] - 2025-09-30

### Added
- Hardware keyboard integration using F256 kernel events
- Mouse event handling via kernel event system with hardware cursor
- PS/2 mouse initialization and hardware register support
- VSYNC synchronization to eliminate screen tearing
- Full input event framework with keyboard and mouse support
- Proper PS/2 keyboard scan code mapping for F256
- Mouse acceleration boost for fast movement (like F256KsimpleCdoodles example)

### Changed
- Updated input.c to use `kernelNextEvent()` for hardware polling
- Fixed keyboard scan codes to match F256 PS/2 Set 2 keyboard
  - Arrow keys: 0x75 (Up), 0x72 (Down), 0x6B (Left), 0x74 (Right)
  - Enter: 0x5A, Escape: 0x76
  - Game shortcuts: R, I, D, S, H, X, M, +, -, U
- Enhanced input_event_t structure with ascii and is_repeat fields
- Added MOUSE_BUTTON_MIDDLE to mouse_button_t enum
- Mouse position now properly synced with hardware registers (PS2_M_X_LO/PS2_M_Y_LO)
- Mouse position clamped to screen bounds (320x240)
- Rendering now waits for vertical blank before sprite updates

### Fixed
- **Screen flicker/tearing**: Removed text_test() function that modified MMU_IO_CTRL during game loop
- **Missing mouse pointer**: Added PS/2 mouse hardware initialization (PS2_M_MODE_EN = 0x01)
- **Mouse not tracking**: Now properly reads/writes hardware mouse position registers
- **Keyboard not working**: Implemented proper kernel event polling for key press/release
- **Screen "snow" effect**: Added graphicsWaitVerticalBlank() in render_update() to sync with VSYNC
- Input events now properly detect key press/release from hardware
- Mouse delta events properly update cursor position with hardware acceleration

### Technical Details
- Code size: 8,159 bytes (up from 7,100 bytes)
- Total executable: 87 KB
- Input polling now event-driven using F256 kernel
- Zero-latency keyboard response
- Mouse tracking with button state change detection and 2x boost for fast movement
- VSYNC-locked rendering at 60 FPS
- PS/2 hardware registers: 0xD6E0 (mode/enable), 0xD6E2 (X position), 0xD6E4 (Y position)

### Integration Status
- ✅ Video initialization with CLUT
- ✅ Board model with Union-Find
- ✅ Game state management
- ✅ **Input hardware integration complete (keyboard + mouse + vsync)**
- ✅ Rendering pipeline integrated with vsync
- ⏳ Menu system (next)
- ⏳ AI engine
- ⏳ Audio system

### Testing Notes
- Mouse pointer should now be visible and responsive
- Keyboard arrow keys should work without screen artifacts
- No more screen tearing or "snow" effect during input
- Smooth 60 FPS rendering synchronized with display refresh
