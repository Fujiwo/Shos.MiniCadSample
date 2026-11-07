# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A sample of a simple CAD app in C++20 / MFC (Windows only). It draws dots, lines, rectangles and ellipses, and supports selection, undo/redo, rubber-band input, zoom/scroll views, serialization and clipboard copy/paste. Visual Studio solution `Shos.MiniCadSample.sln` (toolset v145, configurations Debug/Release × x86/x64).

The repo lives on a Windows filesystem and is opened from WSL. MSBuild/VS are Windows tools, so build from Visual Studio or a Developer PowerShell, not from the Linux shell.

## Build and test

```powershell
# Build everything
msbuild Shos.MiniCadSample.sln /p:Configuration=Debug /p:Platform=x64

# Run all unit tests (Microsoft CppUnitTestFramework; the test DLL lands under x64\Debug)
vstest.console.exe x64\Debug\Shos.MiniCadSample.Test.dll

# Run a single test
vstest.console.exe x64\Debug\Shos.MiniCadSample.Test.dll /Tests:undo_redo_vector_test::undo_group
```

Projects:
- `Shos.MiniCadSample`: the MFC application (shared-DLL MFC, precompiled header `pch.h`).
- `Shos.MiniCadSample.Test`: CppUnitTest DLL that covers only `undo_redo_vector.h` (included by relative path, no MFC).
- `Shos.MiniCadSample.MemoryLeakTest`: console app that runs `undo_redo_vector` scenarios in Debug and reports leaks through the CRT debug heap (`MemoryLeakTest.h` redefines `new`; call `CheckMemoryLeaksStart()` at the top of `main`).

## Architecture

Most logic lives in headers. The `.cpp` files mostly hold `IMPLEMENT_DYNCREATE`/`IMPLEMENT_SERIAL` and message maps.

**Model / view separation through the Observer pattern** (`Observer.h`: `Observable<THint>` / `Observer<THint>`):
- `Model` (`Model.h`) owns the figures in a `shos::undo_redo_pointer_vector<Figure>` and is an `Observable<Hint>`. A `Hint` carries a `Type` (`Added`, `Removed`, `Changed`, `All`, `ViewOnly`) and the affected figures, so views can redraw only what changed.
- `Document` (`CDocument`) wraps the `Model`, observes it, and forwards to the views. It also owns the `CommandManager` and receives mouse events.
- `View` derives from `DoubleBufferView` or `DoubleBufferScrollView` (`DoubleBuffer.h`), depending on the `SCROLL_VIEW` / `ZOOMING_VIEW` macros at the top of `View.h`. `MOUSE_EVENT_TRANSLATOR_TEST` in `MouseEventTranslator.h` turns on a debug listener.
- Figure attributes go both ways through `MainFrame` messages: `Application::Set(...)` pushes the current or selected attribute to the modeless `FigureAttributeDialog`. `Model` is also an `Observer<FigureAttribute>` and receives edits from the dialog, which apply to the selected figures, or become the default for new figures when nothing is selected.

**Input pipeline**: `View` → `MouseEventTranslator` (turns raw mouse messages into click/cursor/drag start/dragging/abort/end) → `Document` (checks the point is inside the model area) → `CommandManager` → the current `Command`. Commands (`Command.h`) are `SelectCommand` and the `AddFigureCommand` subclasses (Dot, Line, Rectangle, Ellipse). They support both drag input and click input, and draw their own rubber band. Switching tools means `CommandManager::SetCommand(new XxxCommand())`.

**Figures** (`Figure.h`): `Figure : CObject` is serializable (`IMPLEMENT_SERIAL`) and has `Clone()`. `FigureHelper` makes random figures.

**Undo/redo** (`undo_redo_vector.h`, namespace `shos`): `undo_redo_collection<TElement, TCollection>` records insert/remove/update steps. Its iterators are always `const_iterator`, so elements can only be changed through `push_back`/`insert`/`erase`/`update`/`clear`. The collection is move-only.

A RAII `transaction` groups several steps into one undo unit. Transactions can be nested; only the outermost one creates the step. While a transaction is open, `undo()`/`redo()` return `false` and `reset()` throws.

`undo_redo_pointer_collection` owns its pointers and uses `delete` as the clean-up function. That function runs exactly once for each element when the element leaves for good: when its undo history is discarded (redo history truncated), on `reset()`, or when the collection is destroyed. The clean-up function must not throw.

`synchronized_undo_redo_collection` is a thread-safe wrapper. The app does not use it.

Because of this:
- Never delete a `Figure` after passing it to `Model`.
- To change a figure, `Clone()` it, edit the clone, and `update` the slot instead of changing it in place (see `Model::Update`). That is what makes the change undoable. Selection (`Figure::Select`) is the exception: it changes the figure in place and is not recorded.
- Wrap multi-figure operations in `shos::undo_redo_vector<Figure*>::transaction`.

Run `Shos.MiniCadSample.Test` and `Shos.MiniCadSample.MemoryLeakTest` after any change to `undo_redo_vector.h`.
