# BoltV6 ML Integration

This project is based on the `BoltV6` branch with exported model weights imported from the `ml` branch.

## Imported artifacts

- `models/exported/unmark_mlp_weights.txt`
- `models/exported/unmark_mlp_weights.json`
- `models/exported/shot_target_mlp_weights.txt`
- `models/exported/shot_target_mlp_weights.json`
- Runtime copies in `src/unmark_mlp_weights.txt` and `src/shot_target_mlp_weights.txt`

## C++ integration points

- `src/player/learning/bolt_unmark_inference.*` loads the unmark model.
- `src/player/learning/bolt_shot_inference.*` loads the shot-target model.
- `src/player/bhv_unmark.cpp` adds the unmark score to candidate-position evaluation.
- `src/player/sample_field_evaluator.cpp` adds the shot-target score to shooting-state evaluation.
- `src/CMakeLists.txt` copies both `.txt` weights into `build/bin` with the existing configs.

## Build and run

```bash
cd /home/ubuntu/robocup/BoltV6_ML
mkdir -p build
cd build
cmake ..
make
cd bin
./start.sh
```

