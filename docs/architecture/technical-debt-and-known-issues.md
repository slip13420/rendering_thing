# Technical Debt and Known Issues

## Critical Technical Debt for GPU Integration

1. **Path Tracer Algorithm**: Currently uses object-oriented ray intersection
   - **Impact**: Virtual function calls not GPU-friendly
   - **Solution**: Flatten to GPU-compatible data structures

2. **Memory Layout**: Current primitive storage uses inheritance
   - **Impact**: Poor GPU memory access patterns
   - **Solution**: Structure-of-arrays for GPU efficiency

3. **Threading Model**: CPU-only std::thread coordination
   - **Impact**: No GPU-CPU work distribution
   - **Solution**: GPU compute queue integration

## Performance Bottlenecks (Current CPU Implementation)

- **Ray-Sphere Intersection**: ~70% of compute time in `ray_color()` function
- **Random Number Generation**: Serial RNG limits parallel scaling
- **Memory Access**: Object-oriented design causes cache misses
- **Progressive Rendering**: CPU thread coordination overhead

## Workarounds and Gotchas for GPU Development

- **Cross-Platform Compatibility**: Must support Windows/Linux/macOS GPU drivers
- **Memory Synchronization**: GPU-CPU data transfer overhead critical
- **Precision Differences**: GPU float precision vs CPU double precision
- **Debug Complexity**: GPU compute shader debugging more difficult than CPU
