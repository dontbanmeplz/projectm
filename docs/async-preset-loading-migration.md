# Async Preset Loading Migration Guide

## Overview

projectM 4.1.0 introduces an optional two-phase preset loading API that moves CPU-heavy work (file parsing, HLSL preprocessing, HLSL→GLSL transpilation) off the GL thread. This reduces GL-thread blocking from ~1000ms to ~200-400ms per preset switch.

The existing synchronous API is unchanged. Async loading is opt-in.

## How It Works

| Phase | Thread | What Happens | Time |
|-------|--------|-------------|------|
| **Phase 1: Prepare** | Any thread (no GL context needed) | Parse .milk file, preprocess HLSL, transpile to GLSL | ~80-130ms |
| **Phase 2: Load** | GL thread only | Create GL resources, compile shaders, start transition | ~200-400ms |

## C API Migration

### Before (synchronous)

```c
// Runs entirely on the GL thread — blocks for ~1000ms
projectm_load_preset_file(instance, "/path/to/preset.milk", smooth);
```

### After (async, two-phase)

```c
// Phase 1: Can run on ANY thread. No GL context or projectm_handle needed.
projectm_prepared_preset* prepared = projectm_prepare_preset_file("/path/to/preset.milk");

// Optional: check for preparation errors
if (!projectm_prepared_preset_is_valid(prepared)) {
    const char* error = projectm_prepared_preset_get_error(prepared);
    // Handle error...
    projectm_free_prepared_preset(prepared);
    return;
}

// Phase 2: MUST run on the GL thread. Consumes and frees `prepared`.
projectm_load_prepared_preset(instance, prepared, smooth);
// Do NOT use `prepared` after this call.
```

### New C API Functions

```c
// Opaque handle for prepared preset data
typedef struct projectm_prepared_preset projectm_prepared_preset;

// Phase 1: Prepare (any thread, no GL)
projectm_prepared_preset* projectm_prepare_preset_file(const char* filename);

// Query preparation result
bool        projectm_prepared_preset_is_valid(const projectm_prepared_preset* prepared);
const char* projectm_prepared_preset_get_error(const projectm_prepared_preset* prepared);

// Phase 2: Load (GL thread only, consumes handle)
void projectm_load_prepared_preset(projectm_handle instance,
                                   projectm_prepared_preset* prepared,
                                   bool smooth_transition);

// Free without loading (any thread, for cleanup on error)
void projectm_free_prepared_preset(projectm_prepared_preset* prepared);
```

### Ownership Rules

- `projectm_prepare_preset_file()` allocates — caller owns the handle.
- `projectm_load_prepared_preset()` **consumes and frees** the handle. Do not use it afterward. Do not call `projectm_free_prepared_preset()` on it.
- `projectm_free_prepared_preset()` is for discarding a handle you decided not to load. Safe to call with NULL.

### Fallback Pattern

If async preparation fails, fall back to the synchronous path:

```c
projectm_prepared_preset* prepared = projectm_prepare_preset_file(path);
if (projectm_prepared_preset_is_valid(prepared)) {
    projectm_load_prepared_preset(instance, prepared, smooth);
} else {
    projectm_free_prepared_preset(prepared);
    // Fallback to synchronous loading
    projectm_load_preset_file(instance, path, smooth);
}
```

## C++ API Migration

### Before (synchronous)

```cpp
projectM->LoadPresetFile("/path/to/preset.milk", true);
```

### After (async, two-phase)

```cpp
#include <MilkdropPreset/Factory.hpp>
#include <MilkdropPreset/PreparedPresetData.hpp>

// Phase 1: Background thread (no GL)
auto prepared = libprojectM::MilkdropPreset::Factory::PreparePresetFromFile(
    "/path/to/preset.milk");

// Phase 2: GL thread
if (prepared && prepared->valid) {
    projectM->LoadPreparedPreset(std::move(prepared), true);
} else {
    // Fallback to synchronous
    projectM->LoadPresetFile("/path/to/preset.milk", true);
}
```

## Android / JNI Migration

The projectM library changes are in the C/C++ layer. To use them from Android, you need to add JNI wrappers in your Android project.

### JNI Wrappers (add to your projectm_jni.cpp)

```cpp
// Phase 1: No GL context needed, no projectM mutex needed
JNIEXPORT jlong JNICALL
Java_com_example_ProjectMJNI_nativePreparePresetFile(
    JNIEnv* env, jclass clazz, jstring path)
{
    const char* presetPath = env->GetStringUTFChars(path, nullptr);
    auto* prepared = projectm_prepare_preset_file(presetPath);
    env->ReleaseStringUTFChars(path, presetPath);
    return reinterpret_cast<jlong>(prepared);
}

// Phase 2: GL thread, acquires projectM mutex
JNIEXPORT jboolean JNICALL
Java_com_example_ProjectMJNI_nativeLoadPreparedPreset(
    JNIEnv* env, jclass clazz, jlong preparedHandle, jboolean smooth)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_projectm || !preparedHandle) return JNI_FALSE;

    auto* prepared = reinterpret_cast<projectm_prepared_preset*>(preparedHandle);
    projectm_load_prepared_preset(g_projectm, prepared, static_cast<bool>(smooth));
    // prepared is consumed — do not use after this
    return JNI_TRUE;
}

// Cleanup if you decide not to load
JNIEXPORT void JNICALL
Java_com_example_ProjectMJNI_nativeFreePreparedPreset(
    JNIEnv* env, jclass clazz, jlong preparedHandle)
{
    if (preparedHandle) {
        auto* prepared = reinterpret_cast<projectm_prepared_preset*>(preparedHandle);
        projectm_free_prepared_preset(prepared);
    }
}
```

### Java Native Declarations (add to your ProjectMJNI.java)

```java
/** Phase 1: Prepare preset (any thread, no GL). Returns native handle. */
public static native long nativePreparePresetFile(String path);

/** Phase 2: Load prepared preset (GL thread only). Consumes the handle. */
public static native boolean nativeLoadPreparedPreset(long preparedHandle,
                                                      boolean smoothTransition);

/** Free a prepared preset without loading it. */
public static native void nativeFreePreparedPreset(long preparedHandle);
```

### Kotlin Usage (e.g., PlaybackController.kt)

```kotlin
private fun loadCurrentPreset(glSurface: VisualizerGLSurfaceView?, smooth: Boolean) {
    val preset = getCurrentPreset() ?: return
    val path = preset.filePath

    // Phase 1: Parse + transpile on background thread
    bgExecutor.execute {
        val preparedHandle = ProjectMJNI.nativePreparePresetFile(path)

        if (preparedHandle != 0L) {
            // Phase 2: GL resource creation on GL thread
            glSurface?.queueEvent {
                ProjectMJNI.nativeLoadPreparedPreset(preparedHandle, smooth)
            }
        } else {
            // Fallback to synchronous loading
            glSurface?.queueEvent {
                ProjectMJNI.nativeLoadPresetFile(path, smooth)
            }
        }
    }
}
```

## Thread Safety

- **Phase 1 (`projectm_prepare_preset_file`)** does not access the projectM instance at all. It creates an independent data structure by reading a file and running the CPU-only transpiler. No mutex is needed. Multiple Phase 1 calls can run concurrently.

- **Phase 2 (`projectm_load_prepared_preset`)** accesses the projectM instance and makes GL calls. It must run on the GL thread and must be serialized with other projectM API calls (same as `projectm_load_preset_file`).

- The `PreparedPresetData` struct flows from Phase 1 to Phase 2 as a classic producer-consumer handoff. No shared mutable state.

## Limitations

- **`idle://` presets** cannot be prepared asynchronously. `projectm_prepare_preset_file("idle://")` returns invalid data. Use the synchronous path for idle presets.
- **Stream-based loading** (`projectm_load_preset_data` / `LoadPresetData`) does not have an async variant. Only file-based loading is supported.
- **GPU shader compilation** (100-300ms) still runs on the GL thread. This is irreducible without a shared GL context approach.

## Compatibility

- The synchronous API (`projectm_load_preset_file`, `LoadPresetFile`) is unchanged and continues to work.
- The new API functions are additive — no breaking changes.
- Minimum projectM version: 4.2.0 (when these functions are available in a release).
