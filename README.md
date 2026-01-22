# Magnifier Headless Mode

Blocks the Magnifier window creation, keeping zoom functionality with Win+"-" and Win+"+" keyboard shortcuts.

## Overview

This Windhawk mod completely hides the Windows Magnifier UI while preserving full zoom functionality. Perfect for users who want the magnification features without the visual distraction of the Magnifier window.

## Features

- **Complete UI Hiding**: Prevents Magnifier window from ever appearing
- **Keyboard Shortcuts Preserved**: Win + Plus and Win + Minus still work perfectly
- **No Taskbar Icon**: Magnifier won't show up in your taskbar
- **Fixes Mouse Cursor Freeze**: Eliminates the 5-6 second mouse cursor freeze that occurs when Magnifier touch controls appear on startup (a known Windows Magnifier bug affecting users even without touch devices)
- **Blocks Touch Interface**: Completely hides the Magnifier touch controls (semi-transparent squares with black borders that appear at screen corners), which are unnecessary for non-touch users
- **Thread-Safe**: Robust implementation with race condition protection
- **High Performance**: Optimized with LRU cache and fast-path filtering
- **Comprehensive Coverage**: Hooks 11 Windows APIs for complete control

## Technical Details

**Version**: 1.3.2

**Improvements**:
- Thread-safe implementation with CRITICAL_SECTION protection
- 11 API hooks for complete window visibility control
- Window procedure interception for message-level blocking
- LRU cache (16 entries) for fast window detection
- Process ID filtering for minimal overhead
- Portable error handling with retry mechanisms

**Compatibility**: Windows 10/11, magnify.exe only

## Installation

1. Install [Windhawk](https://windhawk.net/)
2. Search for "Magnifier Headless Mode" in the mod browser
3. Click Install
4. Launch Windows Magnifier (Win + Plus)
5. Enjoy invisible magnification!

## Usage

Simply use the standard Windows Magnifier keyboard shortcuts:
- **Win + Plus**: Zoom in
- **Win + Minus**: Zoom out
- **Win + Esc**: Exit Magnifier

The Magnifier window will remain completely hidden while all functionality works normally.
