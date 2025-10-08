# Module-Based Hook Dispatchers

This directory contains **dispatcher functions** organized by their target module/DLL. Each file manages hooks for a specific memory space.

> **Directory renamed from `funcs/` to `dispatchers/` for better clarity of purpose.**

## Directory Structure

| File | Target Module | Address Range | Description |
|------|---------------|---------------|-------------|
| `exe.cpp` | **SoF.exe** | `0x200xxxxx` | Main executable functions |
| `ref_gl.cpp` | **ref_gl.dll** | `0x140xxxxx` | Renderer/graphics functions |
| `gamex86.cpp` | **gamex86.dll** | `0x180xxxxx` | Game logic functions |

## Architecture Philosophy

### **Separation of Concerns**
- **`dispatchers/`** = "Where to hook" (addresses, dispatching, module mapping)
- **`features/`** = "What to do" (business logic, callbacks)

### **Benefits**
1. **Clear module mapping** - Know exactly which DLL each function belongs to
2. **Address organization** - Group functions by memory space
3. **Easier debugging** - Quickly find which module a function lives in
4. **Centralized dispatching** - All dispatchers for a module in one place

## Usage Pattern

### **Dispatcher (in dispatchers/)**
```cpp
// dispatchers/ref_gl.cpp
REGISTER_HOOK_VOID(RenderHUD, 0x140234560, void, __cdecl) {
    oRenderHUD();                     // Call original
    DISPATCH_SHARED_HOOK(RenderHUD);  // Dispatch to features
}
```

### **Feature Callbacks (in features/)**
```cpp
// features/weapon_stats/hooks.cpp  
REGISTER_SHARED_HOOK_CALLBACK(RenderHUD, weapon_stats, render_weapon_info, 75);
```

## Memory Map Reference

### **SoF.exe (0x200xxxxx)**
- Core game engine
- Main loop
- Player systems
- Console system

### **ref_gl.dll (0x140xxxxx)**  
- Renderer engine
- HUD rendering
- Scene rendering
- OpenGL calls

### **gamex86.dll (0x180xxxxx)**
- Game logic
- AI systems  
- Physics
- Weapon systems

## Migration Status

- ✅ **Current system** - Working with feature-based dispatchers
- 🚧 **Future system** - Module-based dispatchers (this directory)
- 🔄 **Migration** - Gradual transition from feature-based to module-based dispatchers

## Implementation Notes

- All files include necessary headers for macro support
- Each file focuses on its specific module's address space
- Placeholder examples demonstrate the concept
- Real implementations will replace placeholders as needed
