# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A sample of a simple CAD app in C++20 / MFC (Windows only). It draws dots, lines, rectangles and ellipses, and supports selection, undo/redo, rubber-band input, zoom/scroll views, serialization and clipboard copy/paste. Visual Studio solution `Shos.MiniCadSample.sln` (toolset v145, configurations Debug/Release × x86/x64).

The repo lives on a Windows filesystem and is opened from WSL. MSBuild and vstest are Windows tools (Visual Studio 18 at `C:\Program Files\Microsoft Visual Studio\18\Enterprise`). WSL can run them directly as `.exe` files (see below). Paths given as arguments must be Windows paths (`wslpath -w`).

## Build and test

```bash
# From WSL
MSB="/mnt/c/Program Files/Microsoft Visual Studio/18/Enterprise/MSBuild/Current/Bin/MSBuild.exe"
VST="/mnt/c/Program Files/Microsoft Visual Studio/18/Enterprise/Common7/IDE/CommonExtensions/Microsoft/TestWindow/vstest.console.exe"

# Build everything (Platform: x64 or x86; outputs go to x64/<Config>/ or <Config>/)
"$MSB" Shos.MiniCadSample.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
# To build a single project, use a solution target, e.g. /t:Shos_MiniCadSample_Test
# (building the .vcxproj directly does not pick up the solution settings).

# Run all unit tests (Microsoft CppUnitTestFramework)
"$VST" "$(wslpath -w x64/Debug/Shos.MiniCadSample.Test.dll)"

# Run a single test (filter by method name)
"$VST" "$(wslpath -w x64/Debug/Shos.MiniCadSample.Test.dll)" /Tests:undo_group

# Memory leak check (Debug build only): exit code 0 = no leak, 1 = leaks (dumped to stderr)
./x64/Debug/Shos.MiniCadSample.MemoryLeakTest.exe
./x64/Debug/Shos.MiniCadSample.MemoryLeakTest.exe --self-check   # leaks on purpose; must exit 1
```

From Windows, use the same commands with `msbuild` / `vstest.console.exe` in a Developer PowerShell.

Projects:
- `Shos.MiniCadSample`: the MFC application (shared-DLL MFC, precompiled header `pch.h`).
- `Shos.MiniCadSample.Test`: CppUnitTest DLL that covers only `undo_redo_vector.h` (included by relative path, no MFC). It replaces the global `operator new` so that a test can make the N-th allocation throw (the `allocation_failure` scope). Exception-safety tests loop over N until the operation succeeds. `tracked` counts live instances to check ownership.
- `Shos.MiniCadSample.MemoryLeakTest`: console app that runs `undo_redo_vector` scenarios in Debug and reports leaks through the CRT debug heap (`MemoryLeakTest.h` redefines `new`; call `CheckMemoryLeaksStart()` at the top of `main`).

## Architecture

Most logic lives in headers. The `.cpp` files mostly hold `IMPLEMENT_DYNCREATE`/`IMPLEMENT_SERIAL` and message maps.

**Model / view separation through the Observer pattern** (`Observer.h`: `Observable<THint>` / `Observer<THint>`):
- `Model` (`Model.h`) owns the figures in a `shos::undo_redo_pointer_vector<Figure>` and is an `Observable<Hint>`. A `Hint` carries a `Type` (`Added`, `Removed`, `Changed`, `All`, `ViewOnly`) and the affected figures, so views can redraw only what changed.
- `Document` (`CDocument`) wraps the `Model`, observes it, and forwards to the views. It also owns the `CommandManager` and receives mouse events.
- `View` derives from `DoubleBufferView` or `DoubleBufferScrollView` (`DoubleBuffer.h`, namespace `Shos::MFCDoubleBuffer`). Drawing has two layers. `OnDrawLayer1` draws the figures into a cached bitmap, which is rebuilt only after `Update()`. `OnDrawLayer2` draws the active command's rubber band and cursor on every `WM_PAINT`. The two layers are combined off-screen and then copied to the screen in one step. `CacheDevice` lets code draw extra content into the cache without rebuilding it, but nothing uses it yet. Which base class `View` uses depends on the `SCROLL_VIEW` / `ZOOMING_VIEW` macros at the top of `View.h`. `MOUSE_EVENT_TRANSLATOR_TEST` in `MouseEventTranslator.h` turns on a debug listener.
- Figure attributes go both ways through `MainFrame` messages: `Application::Set(...)` pushes the current or selected attribute to the modeless `FigureAttributeDialog`. `Model` is also an `Observer<FigureAttribute>` and receives edits from the dialog, which apply to the selected figures, or become the default for new figures when nothing is selected.

**Input pipeline**: `View` → `MouseEventTranslator` (turns raw mouse messages into click/cursor/drag start/dragging/abort/end) → `Document` (checks the point is inside the model area) → `CommandManager` → the current `Command`. Commands (`Command.h`) are `SelectCommand` and the `AddFigureCommand` subclasses (Dot, Line, Rectangle, Ellipse). They support both drag input and click input, and draw their own rubber band. Switching tools means `CommandManager::SetCommand(new XxxCommand())`.

**Figures** (`Figure.h`): `Figure : CObject` is serializable (`IMPLEMENT_SERIAL`) and has `Clone()`. `FigureHelper` makes random figures.

**Undo/redo** (`undo_redo_vector.h`, namespace `shos`): `undo_redo_collection<TElement, TCollection>` records insert/remove/update steps. Its iterators are always `const_iterator`, so elements can only be changed through `push_back`/`insert`/`erase`/`update`/`clear`. The collection is move-only.

A RAII `transaction` groups several steps into one undo unit. Transactions can be nested; only the outermost one creates the step. While a transaction is open, `undo()`/`redo()` return `false`, and `reset()`/`clear_history()` throw. `clear_history()` drops the undo history but keeps the elements (for example, after loading a file).

Element types must be nothrow movable and swappable (enforced by `static_assert`). If `push_back`, `insert` or `update` throws, the element passed in has already been cleaned up, and the collection and its history are left unchanged. With `std::vector`, `undo()`/`redo()` never allocate.

`undo_redo_pointer_collection` owns its pointers and uses `delete` as the clean-up function. That function runs exactly once for each element when the element leaves for good: when its undo history is discarded (redo history truncated), on `reset()`, or when the collection is destroyed. The clean-up function must not throw.

`synchronized_undo_redo_collection` is a thread-safe wrapper. The app does not use it.

Because of this:
- Never delete a `Figure` after passing it to `Model`.
- To change a figure, `Clone()` it, edit the clone, and `update` the slot instead of changing it in place (see `Model::Update`). That is what makes the change undoable. Selection (`Figure::Select`) is the exception: it changes the figure in place and is not recorded.
- Wrap multi-figure operations in `shos::undo_redo_vector<Figure*>::transaction`.

Run `Shos.MiniCadSample.Test` and `Shos.MiniCadSample.MemoryLeakTest` after any change to `undo_redo_vector.h`.
