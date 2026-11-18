## Summary

<!-- Briefly describe what this PR changes and why -->

## Related Issue

<!-- Link the issue this PR resolves, e.g. Closes #1 -->

## Verification & Benchmark Checklist

- [ ] `test.bat` passes all unit and scenario tests (`g++ -std=c++17 -O2 -Wall -Wextra -Isrc tests/heuristics_test.cpp`)
- [ ] `build.bat` compiles cleanly with zero warnings (`-Wall -Wextra`)
- [ ] `TrimFlow.exe` Private Working Set remains `< 250 KB` when idle in tray
- [ ] Foreground UI, GPU compositor, and audio/network subprocesses remain protected from aggressive hard trims
